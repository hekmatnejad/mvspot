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

// #define DIJKSTRACHECKTRACE
#ifdef DIJKSTRACHECKTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

#include "fasttgba/fasttgba_product.hh"
#include <iostream>
#include "dijkstracheck.hh"
#include <assert.h>

namespace spot
{
  dijkstracheck::dijkstracheck(instanciator* i, std::string option) :
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
    trivial_scc_(0)
  {
    a_ = inst->get_automaton ();
    if (!option.compare("-cs-ds"))
      {
	deadstore_ = 0;
	roots_stack_ = new stack_of_roots (a_->get_acc());
      }
    else if (!option.compare("-cs+ds"))
      {
	roots_stack_ = new stack_of_roots (a_->get_acc());
	deadstore_ = new deadstore();
      }
    else if (!option.compare("+cs-ds"))
      {
	deadstore_ = 0;
	roots_stack_ = new compressed_stack_of_roots (a_->get_acc());
      }
    else
      {
	assert(!option.compare("+cs+ds") || !option.compare(""));
	roots_stack_ = new compressed_stack_of_roots (a_->get_acc());
	deadstore_ = new deadstore();
      }
  }

  dijkstracheck::~dijkstracheck()
  {
    delete roots_stack_;
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
  dijkstracheck::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void dijkstracheck::init()
  {
    trace << "Dijkstracheck::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    dfs_push(init);
  }

  void dijkstracheck::dfs_push(fasttgba_state* s)
  {
    trace << "Dijkstracheck::DFS_push "
    	  << std::endl;
    ++states_cpt_;
    live.push_back(s);
    H[s] = live.size() -1;
    // Count!
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    todo.push_back ({s, 0, live.size() -1});
    // Count!
    max_dfs_size_ = max_dfs_size_ > todo.size() ?
      max_dfs_size_ : todo.size();

    roots_stack_->push_trivial(todo.size() -1);


    int tmp_cost = 2*roots_stack_->size() + 2*H.size() + 1*live.size()
      + (deadstore_? deadstore_->size() : 0);
    if (tmp_cost > memory_cost_)
      memory_cost_ = tmp_cost;

  }

  void dijkstracheck::dfs_pop()
  {
    trace << "Dijkstracheck::DFS_pop " << std::endl;
    delete todo.back().lasttr;

    unsigned int rtop = roots_stack_->root_of_the_top();
    unsigned int steppos = todo[rtop].position;
    todo.pop_back();

    if (rtop == todo.size())
      {
	++roots_poped_cpt_;
	roots_stack_->pop();
	int trivial = 0;
	while (live.size() > steppos)//todo[rtop].position)
	  {
	    ++trivial;
	    if (deadstore_)	// There is a deadstore
	      {
		deadstore_->add(live.back());
		seen_map::const_iterator it = H.find(live.back());
		H.erase(it);
		live.pop_back();
	      }
	    else
	      {
		H[live.back()] = -1;
		live.pop_back();
	      }
	  }
	if (trivial == 1) // we just popped a trivial
	  ++trivial_scc_;
      }
  }

  bool dijkstracheck::merge(fasttgba_state* d)
  {
    trace << "Dijkstracheck::merge " << std::endl;
    ++update_cpt_;
    int dpos = H[d];
    int rpos = roots_stack_->root_of_the_top();
    markset a = todo[rpos].lasttr->current_acceptance_marks();

    roots_stack_->pop();
    while ((unsigned)dpos < todo[rpos].position)
      {
	++update_loop_cpt_;
    	markset m = todo[rpos].lasttr->current_acceptance_marks();
    	a |= m | roots_stack_->top_acceptance();
    	rpos = roots_stack_->root_of_the_top();
    	roots_stack_->pop();
      }
    roots_stack_->push_non_trivial(rpos, a, todo.size() -1);

    return a.all();
  }

  dijkstracheck::color
  dijkstracheck::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      {
	if (deadstore_)
	  return Alive;
	else
	  return i->second == -1 ? Dead : Alive;
      }
    else if (deadstore_ && deadstore_->contains(state))
      return Dead;
    else
      return Unknown;
  }


  void dijkstracheck::main()
  {
    dijkstracheck::color c;
    while (!todo.empty())
      {
	trace << "Main " << std::endl;

	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = a_->succ_iter(todo.back().state);
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
  dijkstracheck::extra_info_csv()
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
      + std::to_string(roots_stack_->max_size())
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