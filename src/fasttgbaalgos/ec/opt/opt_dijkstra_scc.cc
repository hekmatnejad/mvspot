// Copyright (C) 2013 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// #define OPT_DIJKSTRA_SCCTRACE
#ifdef OPT_DIJKSTRA_SCCTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

#include "fasttgba/fasttgba_product.hh"
#include <iostream>
#include "opt_dijkstra_scc.hh"
#include <assert.h>

namespace spot
{
  opt_dijkstra_scc::opt_dijkstra_scc(instanciator* i,
				     std::string option,
				     bool swarm,
				     int tid) :
    counterexample_found(false),
    inst(i->new_instance()),
    max_live_size_(0),
    max_dfs_size_(0),
    update_cpt_(0),
    update_loop_cpt_(0),
    roots_poped_cpt_(0),
    states_cpt_(0),
    transitions_cpt_(0),
    memory_cost_(0),
    trivial_scc_(0),
    swarm_(swarm),
    tid_(tid)
  {
    a_ = inst->get_automaton ();
    if (!option.compare("-cs"))
      {
	//roots_stack_ = new stack_of_roots (a_->get_acc());
	stack_ = new generic_stack (a_->get_acc());
	deadstore_ = new deadstore();
      }
    else
      {
	assert(!option.compare("+cs") || !option.compare(""));
	//roots_stack_ = new compressed_stack_of_roots (a_->get_acc());
	stack_ = new compressed_generic_stack (a_->get_acc());
	deadstore_ = new deadstore();
      }
    empty_ = new markset(a_->get_acc());
  }

  opt_dijkstra_scc::~opt_dijkstra_scc()
  {
    delete empty_;
    delete stack_;
    //delete roots_stack_;
    delete deadstore_;
    while (!todo.empty())
      {
    	delete todo.back().lasttr;
    	todo.pop_back();
      }

    seen_map::const_iterator s = H.begin();
    while (s != H.end())
      {
	s->first->destroy();
	++s;
      }
    H.clear();

    delete inst;
  }

  bool
  opt_dijkstra_scc::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void opt_dijkstra_scc::init()
  {
    trace << "Opt_Dijkstra_Scc::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    dfs_push(init);
  }

  void opt_dijkstra_scc::dfs_push(fasttgba_state* s)
  {
    trace << "Opt_Dijkstra_Scc::DFS_push "
    	  << std::endl;
    ++states_cpt_;
    //live.push_back(s);
    assert(H.find(s) == H.end());
    H.insert(std::make_pair(s, H.size()));
    // Count!
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    stack_->push_transient(todo.size());

    todo.push_back ({s, 0, H.size() -1});
    // Count!
    max_dfs_size_ = max_dfs_size_ > todo.size() ?
      max_dfs_size_ : todo.size();


    int tmp_cost = 1*stack_->size() + 2*H.size() + 1*live.size()
      + (deadstore_? deadstore_->size() : 0);
    if (tmp_cost > memory_cost_)
      memory_cost_ = tmp_cost;

  }

  void opt_dijkstra_scc::dfs_pop()
  {
    trace << "Opt_Dijkstra_Scc::DFS_pop " << std::endl;
    auto pair = todo.back();
    delete pair.lasttr;
    todo.pop_back();

    if (todo.size() == stack_->top(todo.size()).pos)
      {
	++roots_poped_cpt_;
	stack_->pop(todo.size());
	int trivial = 0;
	deadstore_->add(pair.state);
	seen_map::const_iterator it1 = H.find(pair.state);
	H.erase(it1);
	while (H.size() > pair.position)
	  {
	    ++trivial;
	    auto toerase = live.back();
	    deadstore_->add(toerase);
	    seen_map::const_iterator it = H.find(toerase);
	    H.erase(it);
	    live.pop_back();
 	  }
	if (trivial == 0) // we just popped a trivial
	  ++trivial_scc_;
      }
    else
      {
	// This is the integration of Nuutila's optimisation.
	live.push_back(pair.state);
      }
  }

  bool opt_dijkstra_scc::merge(fasttgba_state* d)
  {
    trace << "Opt_Dijkstra_Scc::merge " << std::endl;
    ++update_cpt_;
    assert(H.find(d) != H.end());

    int dpos = H[d];
    auto top = stack_->pop(todo.size()-1);
    int r = top.pos;
    assert(todo[r].state);

    while ((unsigned)dpos < todo[r].position)
      {
	++update_loop_cpt_;
	assert(todo[r].lasttr);
	auto newtop = stack_->pop(r-1);
	r = newtop.pos;
      }
    stack_->push_non_transient(r, top.acc/*empty*/);

    return false;
  }

  bool opt_dijkstra_ec::merge(fasttgba_state* d)
  {
    trace << "Opt_Dijkstra_EC::merge " << std::endl;
    ++update_cpt_;
    assert(H.find(d) != H.end());

    int dpos = H[d];

    auto top = stack_->pop(todo.size()-1);
    //markset a = top.acc |
    top.acc |= todo.back().lasttr->current_acceptance_marks();
    int r = top.pos;
    assert(todo[r].state);

    while ((unsigned)dpos < todo[r].position)
      {
	++update_loop_cpt_;
	assert(todo[r].lasttr);
	auto newtop = stack_->pop(r-1);

	// [r-1] Because acceptances are stored in the predecessor!
	top.acc |= newtop.acc | todo[r-1].lasttr->current_acceptance_marks();
	r = newtop.pos;
      }
    stack_->push_non_transient(r, top.acc);

    return top.acc.all();
  }

  opt_dijkstra_scc::color
  opt_dijkstra_scc::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (deadstore_->contains(state))
      return Dead;
    else
      return Unknown;
  }


  void opt_dijkstra_scc::main()
  {
    opt_dijkstra_scc::color c;
    while (!todo.empty())
      {
	trace << "Main " << std::endl;

	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = swarm_ ?
	      a_->swarm_succ_iter(todo.back().state, tid_) :
	      a_->succ_iter(todo.back().state);
	    todo.back().lasttr->first();
	  }
	else
	  {
	    assert(todo.back().lasttr);
	    todo.back().lasttr->next();
	  }

    	if (todo.back().lasttr->done())
    	  {
	    dfs_pop ();
    	  }
    	else
    	  {
	    ++transitions_cpt_;
	    assert(todo.back().lasttr);
    	    fasttgba_state* d = todo.back().lasttr->current_state();
	    c = get_color (d);
    	    if (c == Unknown)
    	      {
		dfs_push (d);
    	    	continue;
    	      }
    	    else if (c == Alive)
    	      {
    	    	if (merge (d))
    	    	  {
    	    	    counterexample_found = true;
    	    	    d->destroy();
    	    	    return;
    	    	  }
    	      }
    	    d->destroy();
    	  }
      }
  }

  std::string
  opt_dijkstra_scc::extra_info_csv()
  {
    // dfs max size
    // root max size
    // live max size
    // deadstore max size
    // number of UPDATE calls
    // number of loop inside UPDATE
    // Number of Roots poped
    // visited states
    // visited transitions

    return
      std::to_string(max_dfs_size_)
      + ","
      + std::to_string(stack_->max_size())
      + ","
      + std::to_string(max_live_size_)
      + ","
      + std::to_string(deadstore_? deadstore_->size() : 0)
      + ","
      + std::to_string(update_cpt_)
      + ","
      + std::to_string(update_loop_cpt_)
      + ","
      + std::to_string(roots_poped_cpt_)
      + ","
      + std::to_string(transitions_cpt_)
      + ","
      + std::to_string(states_cpt_)
      + ","
      + std::to_string(memory_cost_)
      + ","
      + std::to_string(trivial_scc_);
  }
}