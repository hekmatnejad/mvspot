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


#include <thread>
#include <vector>
#include "dead_share.hh"
#include "fasttgba/fasttgba_explicit.hh"
#include "fasttgba/fasttgba_product.hh"
#include "fasttgbaalgos/dotty_dfs.hh"
#include <iostream>

namespace spot
{
  // ----------------------------------------------------------------------
  // Concurrent Tarjan Algorithm with shared union find.
  // ======================================================================
  concur_opt_tarjan_scc::concur_opt_tarjan_scc(instanciator* i,
					       spot::uf* uf,
					       int tn,
					       int *stop,
					       int *stop_strong,
					       bool swarming,
					       std::string option)
    : opt_tarjan_scc(i, option, swarming, tn)
  {
    uf_ = uf;
    tn_ = tn;
    stop_ = stop;
    stop_strong_ = stop_strong;
    make_cpt_ = 0;
  }

  bool
  concur_opt_tarjan_scc::check()
  {
    start = std::chrono::system_clock::now();
    init();
    main();
    *stop_strong_ = 1;
    end = std::chrono::system_clock::now();
    return counterexample_found;
  }

  void
  concur_opt_tarjan_scc::dfs_push(fasttgba_state* q)
  {
    int position = H.size();
    H.insert(std::make_pair(q, position));
    stack_->push_transient(position);
    todo.push_back ({q, 0, H.size() -1});

    if (uf_->make_set(q, tn_))
      {
    	q->clone();
    	++make_cpt_;
      }

    ++dfs_size_;
    ++states_cpt_;
    max_dfs_size_ = max_dfs_size_ > dfs_size_ ?
      max_dfs_size_ : dfs_size_;
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    int tmp_cost = 1*stack_->size() + 2*H.size() +1*live.size()
      + (deadstore_? deadstore_->size() : 0);
    if (tmp_cost > memory_cost_)
      memory_cost_ = tmp_cost;
  }

  bool concur_opt_tarjan_scc::dfs_update(fasttgba_state* d)
  {
    ++update_cpt_;
    auto top = stack_->pop(todo.back().position);

    if (H[d] <= (int) top.pos)
      stack_->push_non_transient(H[d], top.acc/*empty*/);
    else
      stack_->push_non_transient(top.pos, top.acc/*empty*/);

    bool fast_backtrack = false;
    uf_->unite (d, todo.back().state, top.acc, &fast_backtrack, tn_);
    return false;
  }

  opt_tarjan_scc::color
  concur_opt_tarjan_scc::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (uf_->is_dead(state))
      return Dead;
    else
      return Unknown;
  }

  void
  concur_opt_tarjan_scc::dfs_pop()
  {
    --dfs_size_;
    auto top = stack_->pop(todo.back().position);

    auto pair = todo.back();
    delete pair.lasttr;
    todo.pop_back();

    if (pair.position == (unsigned) top.pos)
      {
	++roots_poped_cpt_;
	int trivial = 0;

	// Delete the root that is not inside of live Stack
	uf_->make_dead(pair.state, tn_);
	seen_map::const_iterator it1 = H.find(pair.state);
	H.erase(it1);
	while (H.size() > pair.position)
	  {
	    ++trivial;
	    //deadstore_->add(live.back());
	    seen_map::const_iterator it = H.find(live.back());
	    H.erase(it);
	    live.pop_back();
	  }

	// This change regarding original algorithm
	if (trivial == 0)
	  ++trivial_scc_;
      }
    else
      {
	auto newtop = stack_->pop(todo.back().position);

	if (top.pos <= newtop.pos)
	  stack_->push_non_transient(top.pos, newtop.acc);
	else
	  stack_->push_non_transient(newtop.pos, newtop.acc);

	live.push_back(pair.state);
    	bool fast_backtrack = false;
    	uf_->unite (pair.state, todo.back().state, newtop.acc,
    		    &fast_backtrack, tn_);
      }
  }

  bool
  concur_opt_tarjan_scc::has_counterexample()
  {
    return counterexample_found;
  }


  void concur_opt_tarjan_scc::main()
  {
    opt_tarjan_scc::color c;
    while (!todo.empty() && !*stop_ && !*stop_strong_)
      {

	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = swarm_ ?
	      a_->swarm_succ_iter(todo.back().state, tn_) :
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
	    if (counterexample_found)
	      return;
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
    	    	if (dfs_update (d))
    	    	  {
		    *stop_ = 1;
    	    	    counterexample_found = true;
    	    	    d->destroy();
    	    	    return;
    	    	  }
    	      }
    	    d->destroy();
    	  }
      }
  }


  std::chrono::milliseconds::rep concur_opt_tarjan_scc::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds;
  }

  std::string concur_opt_tarjan_scc::csv()
  {
    return "tarjan," + extra_info_csv();
  }

  int concur_opt_tarjan_scc::nb_inserted()
  {
    return make_cpt_;
  }

  // ----------------------------------------------------------------------
  // Concurrent Dijkstra Algorithm with shared union find.
  // ======================================================================
  concur_opt_dijkstra_scc::concur_opt_dijkstra_scc(instanciator* i,
						   spot::uf* uf,
						   int tn,
						   int *stop,
						   int *stop_strong,
						   bool swarming,
						   std::string option)
    : opt_dijkstra_scc(i, option, swarming, tn)
  {
    uf_ = uf;
    tn_ = tn;
    stop_ = stop;
    stop_strong_ = stop_strong;
    make_cpt_ = 0;
  }

  bool
  concur_opt_dijkstra_scc::check()
  {
    start = std::chrono::system_clock::now();
    init();
    main();
    *stop_strong_ = 1;
    end  = std::chrono::system_clock::now();
    return counterexample_found;
  }

  void concur_opt_dijkstra_scc::dfs_push(fasttgba_state* s)
  {
    ++states_cpt_;
    assert(H.find(s) == H.end());
    H.insert(std::make_pair(s, H.size()));

    if (uf_->make_set(s, tn_))
      {
    	s->clone();
    	++make_cpt_;
      }

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

  bool concur_opt_dijkstra_scc::merge(fasttgba_state* d)
  {
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
    	bool fast_backtrack = false;
    	uf_->unite (d, todo[r].state, top.acc, &fast_backtrack, tn_);
      }
    stack_->push_non_transient(r, top.acc/*empty*/);

    return false;
  }

  void concur_opt_dijkstra_scc::dfs_pop()
  {

    auto pair = todo.back();
    delete pair.lasttr;
    todo.pop_back();

    if (todo.size() == stack_->top(todo.size()).pos)
      {
	++roots_poped_cpt_;
	stack_->pop(todo.size());
	int trivial = 0;
	uf_->make_dead(pair.state, tn_);
	seen_map::const_iterator it1 = H.find(pair.state);
	H.erase(it1);
	while (H.size() > pair.position)
	  {
	    ++trivial;
	    auto toerase = live.back();
	    //deadstore_->add(toerase);
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

  opt_dijkstra_scc::color
  concur_opt_dijkstra_scc::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (uf_->is_dead(state))
      return Dead;
    else
      return Unknown;
  }

  bool
  concur_opt_dijkstra_scc::has_counterexample()
  {
    return counterexample_found;
  }

  void concur_opt_dijkstra_scc::main()
  {
    opt_dijkstra_scc::color c;
    while (!todo.empty() && !*stop_ && !*stop_strong_)
      {
	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = swarm_ ?
	      a_->swarm_succ_iter(todo.back().state, tn_) :
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
		    *stop_ = 1;
    	    	    counterexample_found = true;
    	    	    d->destroy();
    	    	    return;
    	    	  }
    	      }
    	    d->destroy();
    	  }
      }
  }

  // ----------------------------------------------------------------------
  // Concurrent Tarjan Emptiness check  with shared union find.
  // ======================================================================
  void concur_opt_tarjan_ec::dfs_pop()
  {
    --dfs_size_;
    auto top = stack_->pop(todo.back().position);

    auto pair = todo.back();
    delete pair.lasttr;
    todo.pop_back();

    if (pair.position == (unsigned) top.pos)
      {
	++roots_poped_cpt_;
	int trivial = 0;

	// Delete the root that is not inside of live Stack
	uf_->make_dead(pair.state, tn_);
	seen_map::const_iterator it1 = H.find(pair.state);
	H.erase(it1);
	while (H.size() > pair.position)
	  {
	    ++trivial;
	    seen_map::const_iterator it = H.find(live.back());
	    H.erase(it);
	    live.pop_back();
	  }

	// This change regarding original algorithm
	if (trivial == 0)
	  ++trivial_scc_;
      }
    else
      {
	auto newtop = stack_->pop(todo.back().position);
	newtop.acc |= top.acc | todo.back().lasttr->current_acceptance_marks();

	if (top.pos <= newtop.pos)
	  stack_->push_non_transient(top.pos, newtop.acc);
	else
	  stack_->push_non_transient(newtop.pos, newtop.acc);

	live.push_back(pair.state);

	if (newtop.acc.all())
	  counterexample_found = true;

    	bool fast_backtrack = false;
    	uf_->unite (pair.state, todo.back().state, newtop.acc,
    		    &fast_backtrack, tn_);

    	// if (fast_backtrack)
    	//   fastbacktrack();
      }
  }

  bool concur_opt_tarjan_ec::dfs_update(fasttgba_state* d)
  {
    ++update_cpt_;
    auto top = stack_->pop(todo.back().position);
    top.acc |= todo.back().lasttr->current_acceptance_marks();

    if (H[d] <= (int) top.pos)
      stack_->push_non_transient(H[d], top.acc);
    else
      stack_->push_non_transient(top.pos, top.acc);

    bool fast_backtrack = false;
    uf_->unite (d, todo.back().state, top.acc, &fast_backtrack, tn_);

    bool rv = top.acc.all();
    // if (!rv && fast_backtrack)
    //   fastbacktrack();

    return rv;
  }

  void concur_opt_tarjan_ec::fastbacktrack()
  {
    ++fastb_cpt_;

    int s = todo.back().position;
    while (!todo.empty() && uf_->is_dead(todo.back().state))
      {
	// Grab the position
	s = todo.back().position;

	// Remove from H
	seen_map::const_iterator it = H.find(todo.back().state);
    	H.erase(it);

	// Release memory
	todo.back().state->destroy();
	delete todo.back().lasttr;

	// Pop todo and dstack
	stack_->pop(todo.back().position);
	todo.pop_back();
	//dstack_->pop();
      }

    if (todo.empty())
      {
	while (!live.empty())
	  {
	    auto toerase = live.back();
	    seen_map::const_iterator it = H.find(toerase);
	    H.erase(it);
	    toerase->destroy();
	    live.pop_back();
	  }
      }
    else
      {
	while (!live.empty() && H[live.back()] > s)
	  {
	    auto toerase = live.back();
	    seen_map::const_iterator it = H.find(toerase);
	    H.erase(it);
	    toerase->destroy();
	    live.pop_back();
	  }
      }

    return;
  }

  std::string concur_opt_tarjan_ec::csv()
  {
    return "tarjan_ec," + extra_info_csv() + "," + std::to_string(fastb_cpt_);
  }

  // ----------------------------------------------------------------------
  // Concurrent Dijkstra Emptiness check  with shared union find.
  // ======================================================================

  bool concur_opt_dijkstra_ec::merge(fasttgba_state* d)
  {
    ++update_cpt_;
    assert(H.find(d) != H.end());

    int dpos = H[d];

    auto top = stack_->pop(todo.size()-1);
    top.acc |= todo.back().lasttr->current_acceptance_marks();
    int r = top.pos;
    assert(todo[r].state);

    while ((unsigned)dpos < todo[r].position)
      {
	++update_loop_cpt_;
	assert(todo[r].lasttr);
	auto newtop = stack_->top(r-1);
	int oldr = r;
	r = newtop.pos;

	// [r-1] Because acceptances are stored in the predecessor!
	top.acc |= newtop.acc | todo[oldr-1].lasttr->current_acceptance_marks();
	//top.acc |= newtop.acc | todo[r].lasttr->current_acceptance_marks();

	bool fast_backtrack = false;
    	uf_->unite (d, todo[r].state, top.acc, &fast_backtrack, tn_);

    	// if (fast_backtrack)
    	//   {
    	//     fastbacktrack();
    	//     return false;
    	//   }
	// To fastbacktrack efficently
	stack_->pop(oldr -1);
      }
    stack_->push_non_transient(r, top.acc);

    return top.acc.all();

  }


 void concur_opt_dijkstra_ec::fastbacktrack()
  {
    ++fastb_cpt_;

    int s = todo.back().position;
    while (!todo.empty() && uf_->is_dead(todo.back().state))
      {
	// Grab the position
	s = todo.back().position;

	// Remove from H
	seen_map::const_iterator it = H.find(todo.back().state);
    	H.erase(it);

	// Release memory
	todo.back().state->destroy();
	delete todo.back().lasttr;

	// Pop todo and the root stack
	// Only pop root stack when the top of todo is the top of root stack
	// There are no consistency problems since the pop is done after
	// calling fastbacktrack in the merge
	//
	// Warning ! The root stack must not be bigger than todo!
	todo.pop_back();
	if (stack_->top(todo.size()).pos == todo.size())
	  stack_->pop(todo.size());
      }

    if (todo.empty())
      {
	while (!live.empty())
	  {
	    auto toerase = live.back();
	    seen_map::const_iterator it = H.find(toerase);
	    H.erase(it);
	    toerase->destroy();
	    live.pop_back();
	  }
      }
    else
      {
	while (!live.empty() && H[live.back()] > s)
	  {
	    auto toerase = live.back();
	    seen_map::const_iterator it = H.find(toerase);
	    H.erase(it);
	    toerase->destroy();
	    live.pop_back();
	  }
      }

    return;
  }


  std::string concur_opt_dijkstra_ec::csv()
  {
    return "dijkstra_ec," + extra_info_csv() + "," + std::to_string(fastb_cpt_);
  }

  // ----------------------------------------------------------------------
  // Concurrent Reachability with shared Open Set.
  // ======================================================================

  concur_reachability_ec::concur_reachability_ec(instanciator* i,
						 spot::openset* os,
						 int thread_number,
						 int total_threads,
						 int *stop,
						 int *stop_terminal,
						 std::atomic<int>& giddle,
						 std::string)
    : tn_(thread_number),
      tt_(total_threads),
      iddle_(false),
      inst(i->new_instance()),
      counterexample_(false),
      giddle_(giddle)
  {
    (void) iddle_;
    insert_cpt_ = 0;
    fail_cpt_ = 0;
    stop_ = stop;
    stop_terminal_ = stop_terminal;
    os_ = os;
    if (i->have_terminal())
      a_ = inst->get_terminal_automaton ();
    else
      a_ = inst->get_automaton ();
    assert(a_);
  }

  concur_reachability_ec::~concur_reachability_ec()
  {
    auto  it = store.begin();
    while (it != store.end())
      {
    	const fasttgba_state* ptr = *it;
    	++it;
    	ptr->destroy();
      }
    delete inst;
  }

  bool
  concur_reachability_ec::check()
  {
    start = std::chrono::system_clock::now();

    auto init = a_->get_init_state();
    if (os_->find_or_put(init))
      {
	++insert_cpt_;
	store.insert(init);
	init->clone();
      }
    else
      {
	++fail_cpt_;
	//   init->destroy();
      }

    while (!*stop_ && !*stop_terminal_)
      {
	auto s = os_->get_one(tn_);

	// Nothing to grab ! The thread become iddle, if all threads
	// are iddle the reachability must end now!
	if (s == 0)
	  {
	    ++giddle_;
	    if (giddle_ == tt_)
	      {
		*stop_terminal_ = 1;
		break;
	      }

	    int size = os_->size();
	    while (os_->size() == size && !*stop_ && !*stop_terminal_)
	      {
		if (giddle_ == tt_)
		  {
		    *stop_terminal_ = 1;
		    break;
		  }
	      }

	    --giddle_;
	    continue;
	  }

	// Walk all successors
	auto succ = a_->succ_iter(s);
	succ->first();
	while (!succ->done())
	  {
	    auto next = succ->current_state();

	    // Check wether this state belong to Terminal (under
	    // assumption that models always have succ!
	    if (is_terminal(next))
	      {
		*stop_ = 1;
		*stop_terminal_ = 1;
		counterexample_ = true;
		break;
	      }

	    // If new insert it ! and keep it for destroy it later!
	    if (os_->find_or_put(next))
	      {
		++insert_cpt_;
		store.insert(next);
		next->clone();
	      }
	    else
	      {
		//next->destroy();
		++fail_cpt_;
	      }
	    succ->next();
	  }
      }
    end = std::chrono::system_clock::now();

    return counterexample_;
  }

  bool
  concur_reachability_ec::is_terminal(const fasttgba_state* s)
  {
    if (auto t = dynamic_cast<const fast_product_state*>(s))
      {
	if (auto q = dynamic_cast<const fast_explicit_state*>(t->right()))
	  {
	    return q->get_strength() == TERMINAL_SCC;
	  }
      }

    // Otherwise what to do? Simple reachability
    return false;
  }

  bool
  concur_reachability_ec::has_counterexample()
  {
    return counterexample_;
  }

  std::string
  concur_reachability_ec::csv()
  {
    return  "reachability," + std::to_string(fail_cpt_);
  }


  std::chrono::milliseconds::rep
  concur_reachability_ec::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds;
  }

  int
  concur_reachability_ec::nb_inserted()
  {
    return insert_cpt_;
  }


  // ----------------------------------------------------------------------
  // Concurrent DFS with shared Hash Table
  // ======================================================================

  concur_weak_ec::concur_weak_ec(instanciator* i,
				 spot::sharedhashtable* sht,
				 int thread_number,
				 int *stop,
				 int *stop_weak,
				 std::string)
    : inst(i->new_instance()),
      sht_(sht),
      tn_(thread_number),
      insert_cpt_(0),
      counterexample_(false)
  {
    a_ = inst->get_weak_automaton ();
    assert(a_);
    stop_ = stop;
    stop_weak_ = stop_weak;
  }

  /// \brief A simple destructor
  concur_weak_ec::~concur_weak_ec()
  {
    // Delete states discovered by this thread
    std::unordered_set<const fasttgba_state*,
		       fasttgba_state_ptr_hash,
		       fasttgba_state_ptr_equal>::const_iterator s;
    s = store.begin();
    while (s != store.end())
      {
	// Advance the iterator before deleting the "key" pointer.
	const fasttgba_state* ptr = *s;
	++s;
	ptr->destroy();
      }
    delete inst;
  }

  void
  concur_weak_ec::push_state(const spot::fasttgba_state* state)
  {
    H.insert(state);
    todo.push_back ({state, 0});
  }

  concur_weak_ec::color
  concur_weak_ec::get_color(const spot::fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (sht_->is_dead(state))
      return Dead;
    else
      return Unknown;
  }

  void
  concur_weak_ec::dfs_pop()
  {
    const fasttgba_state* last = todo.back().state;


    auto st_ref = sht_->find_or_put(last);
    if (st_ref == last)
      {
    	++insert_cpt_;
	last->clone();
	store.insert(last);
      }

    delete todo.back().lasttr;
    todo.pop_back();
    seen_map::const_iterator it = H.find(last);
    H.erase(it);
  }

  bool
  concur_weak_ec::check()
  {
    start = std::chrono::system_clock::now();
    concur_weak_ec::color c;
    auto init = a_->get_init_state();
    push_state(init);

    while (!todo.empty() && !*stop_ && !*stop_weak_)
      {
	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr =
	      a_->swarm_succ_iter(todo.back().state, tn_);
	    todo.back().lasttr->first();
	  }
	else
	  {
	    todo.back().lasttr->next();
	  }

	if (todo.back().lasttr->done())
	  {
	    dfs_pop ();
	  }
	else
	  {
	    fasttgba_state* d = todo.back().lasttr->current_state();
	    c = get_color (d);
	    if (c == Unknown)
	      {
		push_state (d);
		continue;
	      }
	    else if (c == Alive)
	      {
		accepting_cycle_check(todo.back().state, d);
		if (counterexample_)
		  {
		    *stop_ = 1;
		  }
		continue;
	      }

	    // Only check if the owner is this thread! in this case should
	    // not be destroyed!
	    //if (store.find(d) == store.end())
	    d->destroy();
	  }
      }
    end = std::chrono::system_clock::now();
    return counterexample_;
  }

  void
  concur_weak_ec::accepting_cycle_check(const fasttgba_state* left,
					const fasttgba_state* right)
  {
    if (auto t = dynamic_cast<const fast_product_state*>(left))
      {
	if (auto q = dynamic_cast<const fast_explicit_state*>(t->right()))
	  {
	    assert(q);
	    if (!q->formula_scc_accepting)
	      return;

	    if (auto tt = dynamic_cast<const fast_product_state*>(right))
	      {
		if (auto qq = dynamic_cast<const fast_explicit_state*>
		    (tt->right()))
		  {
		    assert(qq);
		    if (q->formula_scc_number == qq->formula_scc_number)
		      counterexample_ = true;
		  }
	      }
	  }
      }
  }

  bool
  concur_weak_ec::has_counterexample()
  {
    return counterexample_;
  }

  std::string
  concur_weak_ec::csv()
  {
    return "weak_dfs";
  }

  std::chrono::milliseconds::rep
  concur_weak_ec::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds > 0 ? elapsed_seconds : 0;
  }

  int
  concur_weak_ec::nb_inserted()
  {
    return insert_cpt_;
  }


  // ----------------------------------------------------------------------
  // TACAS13 -- Single Reachability with swarming
  // ======================================================================

  reachability_ec::reachability_ec(instanciator* i,
				   int thread_number,
				   int *stop)   :
    tn_(thread_number),
    insert_cpt_(0),
    inst(i->new_instance()),
    counterexample_(false)
  {
    (void) tn_;
    (void) update_cpt_;
    a_ = inst->get_terminal_automaton ();
    stop_ = stop;
    deadstore_ = new deadstore();
    transitions_cpt_ = 0;
    max_dfs_size_ = 0;
    memory_cost_ = 0;
    max_live_size_ = 0;
  }

  reachability_ec::~reachability_ec()
  {
    // Delete states discovered by this thread
    std::unordered_set<const fasttgba_state*,
		       fasttgba_state_ptr_hash,
		       fasttgba_state_ptr_equal>::const_iterator s;
    s = H.begin();
    while (s != H.end())
      {
	// Advance the iterator before deleting the "key" pointer.
	const fasttgba_state* ptr = *s;
	++s;
	ptr->destroy();
      }
    delete deadstore_;
    delete inst;

  }

  void
  reachability_ec::push_state(const spot::fasttgba_state* state)
  {
    H.insert(state);
    todo.push_back ({state, 0});
    ++insert_cpt_;
    if (is_terminal(state))
      {
	*stop_ = 1;
	end = std::chrono::system_clock::now();
	counterexample_ = true;
      }

    max_dfs_size_ = max_dfs_size_ > todo.size() ?
      max_dfs_size_ : todo.size();

    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    unsigned int tmp_cost = 1*H.size() + (deadstore_? deadstore_->size() : 0);
    if (tmp_cost > memory_cost_)
      memory_cost_ = tmp_cost;

  }

  reachability_ec::color
  reachability_ec::get_color(const spot::fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (deadstore_->contains(state))
      return Dead;
    else
      return Unknown;
  }

  bool
  reachability_ec::check()
  {
    start = std::chrono::system_clock::now();
    auto init = a_->get_init_state();
    push_state(init);

    while (!todo.empty() && !*stop_)
      {
	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr =
	      a_->succ_iter(todo.back().state);
	    todo.back().lasttr->first();
	  }
	else
	  {
	    todo.back().lasttr->next();
	  }

	if (todo.back().lasttr->done())
	  {
	    const fasttgba_state* last = todo.back().state;
	    deadstore_->add(last);
	    delete todo.back().lasttr;
	    todo.pop_back();
	    seen_map::const_iterator it = H.find(last);
	    H.erase(it);
	  }
	else
	  {
	    ++transitions_cpt_;
	    fasttgba_state* d = todo.back().lasttr->current_state();
	    auto c = get_color (d);
	    if (c == Unknown)
	      {
		push_state(d);
		continue;
	      }
	    d->destroy();
	  }
      }
      end = std::chrono::system_clock::now();
      return counterexample_;
    }

  bool
  reachability_ec::is_terminal(const fasttgba_state* s)
  {
    if (auto t = dynamic_cast<const fast_product_state*>(s))
      {
	if (auto q = dynamic_cast<const fast_explicit_state*>(t->right()))
	  {
	    return q->get_strength() == TERMINAL_SCC;
	  }
      }

    // Otherwise what to do? Simple reachability
      return false;
  }

  bool
  reachability_ec::has_counterexample()
  {
    return counterexample_;
  }

  std::string
  reachability_ec::csv()
  {
    return "reachability,"
      + std::to_string(max_dfs_size_)
      + ","
      + std::to_string(0/*stack_->max_size()*/)
      + ","
      + std::to_string(max_live_size_)
      + ","
      + std::to_string(deadstore_? deadstore_->size() : 0)
      + ","
      + std::to_string(0/*update_cpt_*/)
      + ","
      + std::to_string(0/*update_loop_cpt_*/)
      + ","
      + std::to_string(0/*roots_poped_cpt_*/)
      + ",@"
      + std::to_string(transitions_cpt_)
      + ","
      + std::to_string(insert_cpt_/*states_cpt_*/)
      + ","
      + std::to_string(memory_cost_)
      + ","
      + std::to_string(0/*trivial_scc_*/);
  }

  std::chrono::milliseconds::rep
  reachability_ec::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds < 0 ? 0 : elapsed_seconds;//too quick!
  }

  int
  reachability_ec::nb_inserted()
  {
    return insert_cpt_;
  }



  // ----------------------------------------------------------------------
  // TACAS'13 -- Single Weak EC with swarming
  // ======================================================================

  weak_ec::weak_ec(instanciator* i,
		   int thread_number,
		   int *stop)
    : inst(i->new_instance()),
      tn_(thread_number),
      insert_cpt_(0),
      counterexample_(false)
  {
    (void) tn_;
    a_ = inst->get_weak_automaton ();
    assert(a_);
    stop_ = stop;
    deadstore_ = new deadstore();
    transitions_cpt_ = 0;
    max_dfs_size_ = 0;
    memory_cost_ = 0;
    max_live_size_ = 0;
    update_cpt_ = 0;
  }

  /// \brief A simple destructor
  weak_ec::~weak_ec()
  {
    // Delete states discovered by this thread
    std::unordered_set<const fasttgba_state*,
		       fasttgba_state_ptr_hash,
		       fasttgba_state_ptr_equal>::const_iterator s;
    s = H.begin();
    while (s != H.end())
      {
	// Advance the iterator before deleting the "key" pointer.
	const fasttgba_state* ptr = *s;
	++s;
	ptr->destroy();
      }
    delete deadstore_;
    delete inst;
  }

  void
  weak_ec::push_state(const spot::fasttgba_state* state)
  {
    H.insert(state);
    todo.push_back ({state, 0});
    ++insert_cpt_;
    max_dfs_size_ = max_dfs_size_ > todo.size() ?
      max_dfs_size_ : todo.size();

    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    unsigned int tmp_cost = 1*H.size() + (deadstore_? deadstore_->size() : 0);
    if (tmp_cost > memory_cost_)
      memory_cost_ = tmp_cost;
  }

  weak_ec::color
  weak_ec::get_color(const spot::fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (deadstore_->contains(state))
      return Dead;
    else
      return Unknown;
  }

  void
  weak_ec::dfs_pop()
  {
    const fasttgba_state* last = todo.back().state;
    deadstore_->add(last);
    delete todo.back().lasttr;
    todo.pop_back();
    seen_map::const_iterator it = H.find(last);
    H.erase(it);
  }

  bool
  weak_ec::check()
  {
    start = std::chrono::system_clock::now();
    weak_ec::color c;
    auto init = a_->get_init_state();
    push_state(init);

    while (!todo.empty() && !*stop_)
      {
	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr =
	      a_->succ_iter(todo.back().state);
	    todo.back().lasttr->first();
	  }
	else
	  {
	    todo.back().lasttr->next();
	  }

	if (todo.back().lasttr->done())
	  {
	    dfs_pop ();
	  }
	else
	  {
	    ++transitions_cpt_;
	    fasttgba_state* d = todo.back().lasttr->current_state();
	    c = get_color (d);
	    if (c == Unknown)
	      {
		push_state (d);
		continue;
	      }
	    else if (c == Alive)
	      {
		accepting_cycle_check(todo.back().state, d);
		if (counterexample_)
		  {
		    *stop_ = 1;
		  }
		continue;
	      }

	    d->destroy();
	  }
      }
    end = std::chrono::system_clock::now();
    return counterexample_;
  }

  void
  weak_ec::accepting_cycle_check(const fasttgba_state* left,
					const fasttgba_state* right)
  {
    ++update_cpt_;
    if (auto t = dynamic_cast<const fast_product_state*>(left))
      {
	if (auto q = dynamic_cast<const fast_explicit_state*>(t->right()))
	  {
	    assert(q);
	    if (!q->formula_scc_accepting)
	      return;

	    if (auto tt = dynamic_cast<const fast_product_state*>(right))
	      {
		if (auto qq = dynamic_cast<const fast_explicit_state*>
		    (tt->right()))
		  {
		    assert(qq);
		    if (q->formula_scc_number == qq->formula_scc_number)
		      counterexample_ = true;
		  }
	      }
	  }
      }
  }

  bool
  weak_ec::has_counterexample()
  {
    return counterexample_;
  }

  std::string
  weak_ec::csv()
  {
    return "weak_dfs,"
      + std::to_string(max_dfs_size_)
      + ","
      + std::to_string(0/*stack_->max_size()*/)
      + ","
      + std::to_string(max_live_size_)
      + ","
      + std::to_string(deadstore_? deadstore_->size() : 0)
      + ","
      + std::to_string(update_cpt_)
      + ","
      + std::to_string(0/*update_loop_cpt_*/)
      + ","
      + std::to_string(0/*roots_poped_cpt_*/)
      + ","
      + std::to_string(transitions_cpt_)
      + ","
      + std::to_string(insert_cpt_/*states_cpt_*/)
      + ","
      + std::to_string(memory_cost_)
      + ","
      + std::to_string(0/*trivial_scc_*/);
  }

  std::chrono::milliseconds::rep
  weak_ec::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds > 0 ? elapsed_seconds : 0;
  }

  int
  weak_ec::nb_inserted()
  {
    return insert_cpt_;
  }

  // ----------------------------------------------------------------------
  // TACAS'13 -- Single Tarjan Algorithm with swarming
  // ======================================================================

  single_opt_tarjan_ec::single_opt_tarjan_ec(instanciator* i,
					     int thread_number,
					     int *stop,
					     std::string option)
    : opt_tarjan_ec(i, option)
  {
    tn_ = thread_number;
    stop_ =  stop;
  }

  void
  single_opt_tarjan_ec::main ()
  {
    opt_tarjan_scc::color c;
    while (!todo.empty() && !*stop_)
      {
	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = swarm_ ?
	      a_->swarm_succ_iter(todo.back().state, tn_) :
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
	    if (counterexample_found)
	      return;
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
		if (dfs_update (d))
		  {
		    *stop_ = 1;
		    counterexample_found = true;
		    d->destroy();
		    return;
		  }
	      }
	    d->destroy();
	  }
      }
  }

  bool
  single_opt_tarjan_ec::check()
  {
    start = std::chrono::system_clock::now();
    init();
    main();
    end = std::chrono::system_clock::now();
    return counterexample_found;
  }

  bool
  single_opt_tarjan_ec::has_counterexample()
  {
    return counterexample_found;
  }

  std::string
  single_opt_tarjan_ec::csv()
  {
    return "tarjan," + extra_info_csv();
  }

  std::chrono::milliseconds::rep
  single_opt_tarjan_ec::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds < 0 ? 0 : elapsed_seconds;//too quick!
  }

  int
  single_opt_tarjan_ec::nb_inserted()
  {
    return states_cpt_;
  }

  // ----------------------------------------------------------------------
  // TACAS'13 -- Single Dijkstra Algorithm with swarming
  // ======================================================================

  single_opt_dijkstra_ec::single_opt_dijkstra_ec(instanciator* i,
					     int thread_number,
					     int *stop,
					     std::string option)
    : opt_dijkstra_ec(i, option)
  {
    tn_ = thread_number;
    stop_ =  stop;
  }

  void
  single_opt_dijkstra_ec::main ()
  {
    opt_dijkstra_scc::color c;
    while (!todo.empty() && !*stop_)
      {
	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = swarm_ ?
	      a_->swarm_succ_iter(todo.back().state, tn_) :
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
		    *stop_ = 1;
    	    	    counterexample_found = true;
    	    	    d->destroy();
    	    	    return;
    	    	  }
    	      }
    	    d->destroy();
    	  }
      }
  }

  bool
  single_opt_dijkstra_ec::check()
  {
    start = std::chrono::system_clock::now();
    init();
    main();
    end = std::chrono::system_clock::now();
    return counterexample_found;
  }

  bool
  single_opt_dijkstra_ec::has_counterexample()
  {
    return counterexample_found;
  }

  std::string
  single_opt_dijkstra_ec::csv()
  {
    return "dijkstra," + extra_info_csv();
  }

  std::chrono::milliseconds::rep
  single_opt_dijkstra_ec::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds < 0 ? 0 : elapsed_seconds;//too quick!
  }

  int
  single_opt_dijkstra_ec::nb_inserted()
  {
    return states_cpt_;
  }






  // ----------------------------------------------------------------------
  // TACAS'13 -- Single NDFS Algorithm with swarming
  // ======================================================================

  single_opt_ndfs_ec::single_opt_ndfs_ec(instanciator* i,
					     int thread_number,
					     int *stop,
					     std::string option)
    : opt_ndfs(i, option)
  {
    tn_ = thread_number;
    stop_ =  stop;
  }

  void
  single_opt_ndfs_ec::main ()
  {
    opt_ndfs::color c;
    while (!todo.empty()  && !*stop_)
      {
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
	    if (counterexample_found)
	      return;
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
    	    	if (dfs_update (d))
    	    	  {
    	    	    counterexample_found = true;
    	    	    d->destroy();
		    *stop_ = 1;
    	    	    return;
    	    	  }
    	      }
    	    d->destroy();
    	  }
      }
  }

  bool
  single_opt_ndfs_ec::check()
  {
    start = std::chrono::system_clock::now();
    init();
    main();
    end = std::chrono::system_clock::now();
    return counterexample_found;
  }

  bool
  single_opt_ndfs_ec::has_counterexample()
  {
    return counterexample_found;
  }

  std::string
  single_opt_ndfs_ec::csv()
  {
    return "ndfs," + extra_info_csv();
  }

  std::chrono::milliseconds::rep
  single_opt_ndfs_ec::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds < 0 ? 0 : elapsed_seconds;//too quick!
  }

  int
  single_opt_ndfs_ec::nb_inserted()
  {
    return states_cpt_;
  }





  // ----------------------------------------------------------------------
  // TACAS'13 -- Single UC13 Algorithm with swarming
  // ======================================================================

  single_opt_uc13_ec::single_opt_uc13_ec(instanciator* i,
					     int thread_number,
					     int *stop,
					     std::string option)
    : unioncheck(i, option)
  {
    tn_ = thread_number;
    stop_ =  stop;
  }

  void
  single_opt_uc13_ec::main ()
  {
    union_find::color c;
    while (!todo.empty() && !*stop_)
      {
	assert(!uf->is_dead(todo.back().state));

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
	    c = uf->get_color(d);
    	    if (c == union_find::Unknown)
    	      {
		dfs_push (d);
    	    	continue;
    	      }
    	    else if (c == union_find::Alive)
    	      {
    	    	if (merge (d))
    	    	  {
    	    	    counterexample_found = true;
		    *stop_ = 1;
 	    	    d->destroy();
    	    	    return;
    	    	  }
    	      }
    	    d->destroy();
    	  }
      }
  }

  bool
  single_opt_uc13_ec::check()
  {
    start = std::chrono::system_clock::now();
    init();
    main();
    end = std::chrono::system_clock::now();
    return counterexample_found;
  }

  bool
  single_opt_uc13_ec::has_counterexample()
  {
    return counterexample_found;
  }

  std::string
  single_opt_uc13_ec::csv()
  {
    return "uc13," + extra_info_csv();
  }

  std::chrono::milliseconds::rep
  single_opt_uc13_ec::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds < 0 ? 0 : elapsed_seconds;//too quick!
  }

  int
  single_opt_uc13_ec::nb_inserted()
  {
    return states_cpt_;
  }

  // ----------------------------------------------------------------------
  // TACAS'13 -- Single TUC13 Algorithm with swarming
  // ======================================================================

  single_opt_tuc13_ec::single_opt_tuc13_ec(instanciator* i,
					     int thread_number,
					     int *stop,
					     std::string option)
    : tarjanunioncheck(i, option)
  {
    tn_ = thread_number;
    stop_ =  stop;
  }

  void
  single_opt_tuc13_ec::main ()
  {
    union_find::color c;
    while (!todo.empty() && !*stop_)
      {
	assert(!uf->is_dead(todo.back().state));

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
	    if (counterexample_found)
	      {
		*stop_ = 1;
		return;
	      }
    	  }
    	else
    	  {
	    ++transitions_cpt_;
	    assert(todo.back().lasttr);
    	    fasttgba_state* d = todo.back().lasttr->current_state();
	    c = uf->get_color(d);
    	    if (c == union_find::Unknown)
    	      {
		dfs_push (d);
    	    	continue;
    	      }
    	    else if (c == union_find::Alive)
    	      {
    	    	if (dfs_update(d))
    	    	  {
    	    	    counterexample_found = true;
		    *stop_ = 1;
    	    	    d->destroy();
    	    	    return;
    	    	  }
    	      }
    	    d->destroy();
    	  }
      }
  }

  bool
  single_opt_tuc13_ec::check()
  {
    start = std::chrono::system_clock::now();
    init();
    main();
    end = std::chrono::system_clock::now();
    return counterexample_found;
  }

  bool
  single_opt_tuc13_ec::has_counterexample()
  {
    return counterexample_found;
  }

  std::string
  single_opt_tuc13_ec::csv()
  {
    return "tuc13," + extra_info_csv();
  }

  std::chrono::milliseconds::rep
  single_opt_tuc13_ec::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds < 0 ? 0 : elapsed_seconds;//too quick!
  }

  int
  single_opt_tuc13_ec::nb_inserted()
  {
    return states_cpt_;
  }











  // ----------------------------------------------------------------------
  // Definition of the core of Dead_share
  // ======================================================================

  dead_share::dead_share(instanciator* i,
			 int thread_number,
			 DeadSharePolicy policy,
			 std::string option) :
    itor_(i), tn_(thread_number), policy_(policy), max_diff(0), option_(option)
  {
    assert(i && thread_number);
    uf_ = new spot::uf(tn_);
    if (policy_ == ASYNC_DIJKSTRA)
      queue_ = new spot::queue(tn_-1);
    else if (policy_ == W2_ASYNC_DIJKSTRA)
      queue_ = new spot::queue(tn_-2);
    else
      queue_ = new spot::queue(tn_);
    os_ = new spot::openset(tn_);
    sht_ = new spot::sharedhashtable(tn_);

    // Must we stop the world? It is valid to use a non atomic variable
    // since it will only pass this variable to true once
    stop = 0;
    stop_terminal = 0;
    stop_weak = 0;
    term_iddle_ = 0;
    stop_strong = 0;

    // Let us instanciate the checker according to the policy
    for (int i = 0; i < tn_; ++i)
      {
	bool s_ = i != 0;// true;

	if (policy_ == FULL_TARJAN)
	  chk.push_back(new spot::concur_opt_tarjan_scc(itor_, uf_,
							i, &stop,
							&stop_strong, s_,
							option_));
	else if (policy_ == FULL_TARJAN_EC)
	  chk.push_back(new spot::concur_opt_tarjan_ec(itor_, uf_,
						       i, &stop,
						       &stop_strong, s_,
						       option_));
	else if (policy_ == REACHABILITY_EC)
	  chk.push_back(new spot::concur_reachability_ec
			(itor_, os_, i, tn_, &stop, &stop_terminal,
			 term_iddle_));
	else if (policy_ == FULL_DIJKSTRA)
	  chk.push_back(new spot::concur_opt_dijkstra_scc(itor_, uf_,
							  i, &stop,
							  &stop_strong,
							  s_, option_));
	else if (policy_ == FULL_DIJKSTRA_EC)
	  chk.push_back(new spot::concur_opt_dijkstra_ec(itor_, uf_,
							 i, &stop,
							 &stop_strong,
							 s_, option_));
	else if (policy_ == MIXED_EC)
	  {
	    if (i%2)
	      chk.push_back(new spot::concur_opt_tarjan_ec(itor_, uf_,
							   i, &stop,
							   &stop_strong,
							   s_, option_));
	    else
	      chk.push_back(new spot::concur_opt_dijkstra_ec(itor_, uf_,
							     i, &stop,
							     &stop_strong,
							     s_, option_));
	  }
	else if (policy_ == MIXED)
	  {
	    if (i%2)
	      chk.push_back(new spot::concur_opt_tarjan_scc(itor_, uf_,
							    i, &stop,
							    &stop_strong,
							    s_, option_));
	    else
	      chk.push_back(new spot::concur_opt_dijkstra_scc(itor_, uf_,
							      i, &stop,
							     &stop_strong,
							      s_, option_));
	  }
	else if (policy_ == ASYNC_DIJKSTRA)
	  {
	    if (i == tn_-1)
	      chk.push_back(new spot::async_worker(itor_, uf_, queue_,
						   i, &stop,
						   &stop_strong,
						   option_));
	    else
	      chk.push_back(new spot::dijkstra_async(itor_, uf_, queue_,
						     i, &stop,
						     &stop_strong,
						     s_, option_));
	  }
	else if (policy_ == W2_ASYNC_DIJKSTRA)
	  {
	    if (tn_-2 <= i)
	      chk.push_back(new spot::async_worker(itor_, uf_, queue_,
						   i, &stop,
						   &stop_strong,
						   option_));
	    else
	      chk.push_back(new spot::dijkstra_async(itor_, uf_, queue_,
						     i, &stop,
						     &stop_strong,
						     s_, option_));
	  }
	else
	  {
	    assert(policy_ == DECOMP_EC || policy_ == DECOMP_EC_SEQ ||
		   policy_ == DECOMP_TACAS13_TARJAN ||
		   policy_ == DECOMP_TACAS13_DIJKSTRA ||
		   policy_ == DECOMP_TACAS13_NDFS ||
		   policy_ == DECOMP_TACAS13_UC13 ||
		   policy_ == DECOMP_TACAS13_TUC13);


	    if (policy_ == DECOMP_EC)
	      {
		int how_many = 1; // At least strong
		if (itor_->have_weak())
		  ++how_many;
		if (itor_->have_terminal())
		  ++how_many;

		int how_many_strong = tn_ / how_many + tn_ % how_many;
		int how_many_weak = ! itor_->have_weak() ? 0 : tn_ / how_many ;
		int how_many_terminal = ! itor_->have_terminal() ? 0 :
		  tn_ / how_many ;

		int k = 0;
		int j = 0;
		// Launch Strong
		while (k++ != how_many_strong)
		  {
		    // It's the mixed algorithm!
		    if (j%2)
		      chk.push_back(new spot::concur_opt_tarjan_ec
				    (itor_, uf_,
				     j, &stop,
				     &stop_strong,
				     s_, option_));
		    else
		      chk.push_back(new spot::concur_opt_dijkstra_ec
				    (itor_, uf_,
				     j, &stop,
				     &stop_strong,
				     s_, option_));
		    j++;
		  }

		std::cout << "Number of threads : " << tn_
			  << ", strong: " << how_many_strong
			  << ", weak: " << how_many_weak
			  << ", terminal: " << how_many_terminal
			  << std::endl;


		// Launch Weak
		k = 0;
		while (k++ != how_many_weak)
		  {
		    chk.push_back(new spot::concur_weak_ec
				  (itor_, sht_, j, &stop, &stop_weak, option_));
		    ++j;
		  }

		k = 0;
		// Launch terminal
		while (k++ != how_many_terminal)
		  {
		    chk.push_back(new spot::concur_reachability_ec
				  (itor_, os_, j, how_many_terminal,
				   &stop, &stop_terminal,
				   term_iddle_, option_));
		    ++j;
		  }

		// All threads have been set ! That's it!
		break;
	      }
	    else if (policy_ == DECOMP_EC_SEQ)
	      {
		int k = 0;
		int j = 0;
		while (k++ != tn_)
		  {
		    // It's the mixed algorithm!
		    if (j%2)
		      chk.push_back(new spot::concur_opt_tarjan_ec(itor_, uf_,
								   j, &stop,
								   &stop_strong,
								   s_,
								   option_));
		    else
		      chk.push_back(new spot::concur_opt_dijkstra_ec
				    (itor_, uf_,
				     j, &stop,
				     &stop_strong,
				     s_, option_));
		    j++;
		  }

		j = 0;
		k = 0;
		while (k++ != tn_ && itor_->have_weak())
		  {
		    chk.push_back(new spot::concur_weak_ec
				  (itor_, sht_, j, &stop, &stop_weak, option_));
		    ++j;
		  }

		j = 0;
		k = 0;
		// Launch terminal
		while (k++ != tn_ && itor_->have_terminal())
		  {
		    chk.push_back(new spot::concur_reachability_ec
				  (itor_, os_, j, tn_,
				   &stop, &stop_terminal,
				   term_iddle_, option_));
		    ++j;
		  }

		break;
	      }
	    else
	      {
		assert(policy == DECOMP_TACAS13_TARJAN ||
		       policy == DECOMP_TACAS13_DIJKSTRA ||
		       policy == DECOMP_TACAS13_NDFS ||
		       policy == DECOMP_TACAS13_UC13 ||
		       policy == DECOMP_TACAS13_TUC13);

		stop = 0;
		if(!itor_->have_terminal())
		  chk.push_back(new spot::fake_ec(0/*id*/));
		else
		  chk.push_back(new spot::reachability_ec
				(itor_, 0/*id*/, &stop));

		if(!itor_->have_weak())
		  chk.push_back(new spot::fake_ec(1/*id*/));
		else
		  chk.push_back(new spot::weak_ec
				(itor_, 1 /*id*/, &stop));

		if(!itor_->have_strong())
		  chk.push_back(new spot::fake_ec(2/*id*/));
		else
		  {
		    if (policy == DECOMP_TACAS13_TARJAN)
		      chk.push_back(new spot::single_opt_tarjan_ec
				    (itor_, 2/*id*/,&stop, option_));
		    else if (policy == DECOMP_TACAS13_DIJKSTRA)
		      chk.push_back(new spot::single_opt_dijkstra_ec
				    (itor_, 2/*id*/,&stop, option_));
		    else if (policy == DECOMP_TACAS13_NDFS)
		      chk.push_back(new spot::single_opt_ndfs_ec
				    (itor_, 2/*id*/,&stop, option_));
		    else  if (policy == DECOMP_TACAS13_UC13)
		      chk.push_back(new spot::single_opt_uc13_ec
				    (itor_, 2/*id*/,&stop, option_));
		    else
		      chk.push_back(new spot::single_opt_tuc13_ec
				    (itor_, 2/*id*/,&stop, option_));
		  }
		break;
	      }
	  }
      }
  }

  dead_share::~dead_share()
  {
    // Release memory
    for (int i = 0; i < (int)chk.size(); ++i)
      {
  	delete chk[i];
      }
    delete sht_;
    delete os_;
    delete queue_;
    delete uf_;
  }

  bool
  dead_share::check()
  {
    std::vector<std::thread> v;
    std::chrono::time_point<std::chrono::system_clock> start, end;

    // Start Global Timer
    std::cout << "Start threads ..." << std::endl;
    start = std::chrono::system_clock::now();


    if (policy_ == DECOMP_EC_SEQ)
      {
	int curr = chk.size();

	while (curr)
	  {
	    curr -= tn_;
	    for (int i = curr; i < curr + tn_; ++i)
	      {
		v.push_back(std::thread ([&](int tid){
		    srand (tid);
		    chk[tid]->check();
		    }, i));
	      }

	    // Wait all threads
	    for (int i = 0; i < (int) v.size(); ++i)
	      {
		v[i].join();
	      }
	    if (stop)
	      break;
	    v.clear();
	  }
	goto the_end;
      }

    // Launch all threads
    for (int i = 0; i < (int)chk.size(); ++i)
      v.push_back(std::thread ([&](int tid){
	    srand (tid);
	    chk[tid]->check();
	  }, i));

    // Wait all threads
    for (int i = 0; i < (int) v.size(); ++i)
      {
	v[i].join();
      }

  the_end:
    // Stop Global Timer
    end  = std::chrono::system_clock::now();

    auto elapsed_seconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

    // Clean up checker and construct CSV
    bool ctrexple = false;
    for (int i = 0; i < (int)chk.size(); ++i)
      ctrexple |= chk[i]->has_counterexample();

    // Display results
    if (policy_ == DECOMP_EC || policy_ == REACHABILITY_EC ||
	policy_ == DECOMP_TACAS13_TARJAN || policy_ == DECOMP_TACAS13_DIJKSTRA)
      {
	if (os_->size())
	  printf("\n[OS] num of threads = %d insert = %d  elapsed time = %d\n",
		 tn_, os_->size(), (int)elapsed_seconds);
	if (sht_->size())
	  printf("\n[SHT] num of threads = %d insert = %d  elapsed time = %d\n",
		 tn_, sht_->size(), (int)elapsed_seconds);
	if (uf_->size())
	  printf("\n[WF] num of threads = %d insert = %d  elapsed time = %d\n",
		 tn_, uf_->size(), (int)elapsed_seconds);

      }
    else
      printf("\n[WF] num of threads = %d insert = %d  elapsed time = %d\n",
	     tn_, uf_->size(), (int)elapsed_seconds);

    return ctrexple;
  }

  void dead_share::dump_threads()
  {
    std::cout << std::endl << "THREADS DUMP : " << std::endl;
    auto  min = chk[0]->get_elapsed_time();
    auto  max = chk[0]->get_elapsed_time();
    for (int i = 0; i < (int)chk.size(); ++i)
      {
	min = min > chk[i]->get_elapsed_time() ?
	  chk[i]->get_elapsed_time() : min;
        max = max < chk[i]->get_elapsed_time() ?
	  chk[i]->get_elapsed_time() : max;

	std::cout << "      @thread : " << i << "  csv : "
		  << (chk[i]->has_counterexample() ? "VIOLATED,"
		      : "VERIFIED,")
		  << chk[i]->get_elapsed_time() << ","
		  << chk[i]->csv() << ","
		  << chk[i]->nb_inserted()
		  << std::endl;
      }
    std::cout << std::endl;
    max_diff = max-min;
  }

  std::string dead_share::csv()
  {
    std::stringstream res;
    switch (policy_)
      {
      case FULL_TARJAN:
	res << "FULL_TARJAN,";
	break;
      case FULL_DIJKSTRA:
	res << "FULL_DIJKSTRA,";
	break;
      case MIXED:
	res << "MIXED,";
	break;
      case FULL_TARJAN_EC:
	res << "FULL_TARJAN_EC,";
	break;
      case FULL_DIJKSTRA_EC:
	res << "FULL_DIJKSTRA_EC,";
	break;
      case MIXED_EC:
	res << "MIXED_EC,";
	break;
      case DECOMP_EC:
	res << "DECOMP_EC,";
	break;
      case DECOMP_EC_SEQ:
	res << "DECOMP_EC_SEQ,";
	break;
      case REACHABILITY_EC:
	res << "REACHABILITY_EC,";
	break;
      case DECOMP_TACAS13_TARJAN:
	res << "DECOMP_TACAS13_TARJAN,";
	break;
      case DECOMP_TACAS13_DIJKSTRA:
	res << "DECOMP_TACAS13_DIJKSTRA,";
	break;
      case DECOMP_TACAS13_NDFS:
	res << "DECOMP_TACAS13_NDFS,";
	break;
      case DECOMP_TACAS13_UC13:
	res << "DECOMP_TACAS13_UC13,";
	break;
      case DECOMP_TACAS13_TUC13:
	res << "DECOMP_TACAS13_TUC13,";
	break;
      case ASYNC_DIJKSTRA:
	res << "ASYNC_DIJKSTRA,";
	break;
      case W2_ASYNC_DIJKSTRA:
	res << "W2_ASYNC_DIJKSTRA,";
	break;
      default:
	std::cout << "Error undefined thread policy" << std::endl;
	assert(false);
      }

    res << tn_  << "," << max_diff;
    return res.str();
  }

}