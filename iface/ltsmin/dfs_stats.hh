// -*- coding: utf-8 -*-
// Copyright (C)  2015 Laboratoire de Recherche et
// Developpement de l'Epita (LRDE)
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

#pragma once

#include "twaalgos/reachiter.hh"
#include "twaalgos/sccinfo.hh"
#include "twaalgos/isweakscc.hh"
#include "twaalgos/stats.hh"
#include "proviso.hh"
#include <random>

namespace spot
{

  /// \brief This Union-Find data structure is dedicated for the dfs_stats class
  /// below. The key of this union-find is int. Moreover, we suppose that only
  /// consecutive int are inserted. This union-find includes most of the
  /// classical optimisations.
  class int_unionfind_IPC_LRPC_MS
  {
  private:
    // Store the parent relation, i.e. -1 or x < id.size()
    std::vector<int> id;

    // id of a specially managed partition of "dead" elements
    const int DEAD = 0;

    int root(int i)
    {
      assert(i > 0);
      int p = id[i];
      if (p == DEAD)
	return DEAD;
      if (p < 0)
	return i;
      int gp = id[p];
      if (gp == DEAD)
	return DEAD;
      if (gp < 0)
	return p;
      p = root(p);
      id[i] = p;
      return p;
    }

  public:
    int_unionfind_IPC_LRPC_MS() : id()
    {
      id.push_back(DEAD);
    }

    void makeset(int e)
    {
      assert(e == (int) id.size());
      id.push_back(-1);
    }

    bool unite(int e1, int e2)
    {
      // IPC - Immediate Parent Check
      int p1 = id[e1];
      int p2 = id[e2];
      if ((p1 < 0 ? e1 : p1) == (p2 < 0 ? e2 : p2))
	return false;
      int root1 = root(e1);
      int root2 = root(e2);
      if (root1 == root2)
	return false;
      int rk1 = -id[root1];
      int rk2 = -id[root2];
      if (rk1 < rk2)
	id[root1] = root2;
      else {
	id[root2] = root1;
	if (rk1 == rk2)
	  id[root1] = -(rk1 + 1);
      }
      return true;
    }

    void markdead(int e)
    {
      id[root(e)] = DEAD;
    }

    bool sameset(int e1, int e2)
    {
      assert(e1 < (int)id.size() && e2 < (int)id.size());
      return root(e1) == root(e2);
    }

    bool isdead(int e)
    {
      assert(e < (int)id.size());
      return root(e) == DEAD;
    }
  };


  /// \brief This class implements a simple DFS. This DFS is more complicated
  /// than the average since it allows to express many options that are useful
  /// for provisos.
  template<bool Anticipated, bool FullyAnticipated, bool ComputeSCC,
	   // If Checker is activated, then the automaton representing
	   // the state space is build and then we can check that
	   // every cycle contains an expanded state.
	   bool Checker>
  class SPOT_API dfs_stats: public dfs_inspector
  {
    // The state space automaton, only available when Checker is activated. Note
    // that is this automaton, we represent expanded states as accepting ones.
    // Doing this allows to simplify the check about all cycles containing an
    // expanded state: we can only check that every SCC is accepting and
    // inherently weak.
    twa_graph_ptr state_space_;

  public:
    dfs_stats(const const_twa_ptr& a, proviso& proviso):
      aut_(a), proviso_(proviso)
    { }

    void push_state(state* st)
    {
      if (SPOT_UNLIKELY(Checker))
	{
	  auto s1 =  state_space_->new_state();
	  assert(s1+1 == dfs_number);
	  if (!todo.empty())
	    state_space_->new_edge(seen[todo.back().src].live_number-1,
				   s1, bddtrue);
	}

      ++states_;
      todo.push_back({st, aut_->succ_iter(st)});
      todo.back().it->first();

      max_dfssize_ = max_dfssize_ > todo.size()?
	max_dfssize_ : todo.size();

      if (todo.back().it->all_enabled())
	++already_expanded_;

      cumul_red_ += todo.back().it->reduced();
      cumul_en_ += todo.back().it->enabled();

      if (proviso_.notify_push(st, *this))
	++expanded_;

      if (ComputeSCC)
	{
	  // The root stack contains the live number of each states.
	  assert(seen[st].live_number == dfs_number);
	  roots.push_back(dfs_number);

	  // We can now update the union-find data structure
	  uf.makeset(dfs_number);
	}

      if (Anticipated || FullyAnticipated)
	{
	  // FIXME bypass static by something faster than std::function
	  static seen_map* seen_ptr;
	  seen_ptr = &seen;
	  todo.back().it->
	    reorder_remaining([](const state* s)
			      {
				return seen_ptr->find(s) != seen_ptr->end();
			      });
	}
    }

    void pop_state()
    {
      if (!proviso_.before_pop(todo.back().src, *this))
	return;

      seen[todo.back().src].dfs_position = -1;

      if (SPOT_UNLIKELY(Checker))
	{
	  // We mark expanded states has "accepting" so we can
	  // only check all SCC of  the state_space_ automaton are
	  // weak accepting to be sure that every cycle contains at
	  // least one  state fully expanded.
	  if (todo.back().it->all_enabled())
	    {
	      for (auto& e: state_space_->
		     out(seen[todo.back().src].live_number-1))
		e.acc.set(0);
	    }
	}

      if (ComputeSCC)
	{
	  int l = seen[todo.back().src].live_number;
	  if (l == roots.back())
	    {
	      roots.pop_back();
	      uf.markdead(l);
	    }
	}

      aut_->release_iter(todo.back().it);
      todo.pop_back();

      if (!todo.empty() && FullyAnticipated)
	{
	  // FIXME bypass static by something faster than std::function
	  static seen_map* seen_ptr;
	  seen_ptr = &seen;
	  todo.back().it->
	    reorder_remaining([](const state* s)
			      {
				return seen_ptr->find(s) != seen_ptr->end();
			      });
	}
    }

    void run()
    {
      if (SPOT_UNLIKELY(Checker))
	{
	  state_space_ = make_twa_graph(aut_->get_dict());
	  state_space_->prop_state_based_acc(true);
	  state_space_->set_acceptance(1, state_space_->get_acceptance());
	}

      state* initial =  aut_->get_init_state();
      seen[initial] = {++dfs_number, (int) todo.size(), false, {}, 0};
      push_state(initial);

      while (!todo.empty())
      	{
       	  if (todo.back().it->done())
      	    {
	      pop_state();
      	    }
	  else
	    {
	      ++transitions_;
	      state* dst = todo.back().it->current_state();
	      todo.back().it->next();
	      state_info info = {dfs_number+1, (int)todo.size(), false, {}, 0};
	      auto res = seen.emplace(dst, info);
	      if (res.second)
		{
		  // it's a new state
		  ++dfs_number;
		  push_state(dst);
		}
	      else
		{
		  if (SPOT_UNLIKELY(Checker))
		    {
		      // There is a difference of (+1)..
		      auto s1 =  seen[todo.back().src].live_number-1;
		      auto s2 =  seen[dst].live_number-1;
		      state_space_->new_edge(s1, s2, bddtrue);
		    }

		  // Here we can detect dead edge, i.e. edge going to a
		  // DEAD state. In this case, no expansion is required
		  // so we can bypass all actions from the proviso.
		  if (ComputeSCC)
		    {
		      if (uf.isdead(seen[dst].live_number))
			{
			  dst->destroy();
			  continue;
			}
		    }

		  // This may be a closing edge
		  int toexpand = proviso_.maybe_closingedge(todo.back().src,
							    dst, *this);

		  // It's not a dead-edge, so we must update both the root
		  // stack and the union-find data structure.
		  // This block cannot be moved upward, i.e. before the call
		  // to proviso.maybe_closingedge, since some proviso need to
		  // work with a non-modified root stack.
		  if (ComputeSCC)
		    {
		      int dst_ln = seen[dst].live_number;
		      while (!uf.sameset(dst_ln, roots.back()))
			{
			  uf.unite(dst_ln, roots.back());
			  roots.pop_back();
			}
		    }

		  // Count the number of backedges
		  if (dfs_position(dst) != -1)
		    ++backedges_;

		  // ... and an expansion is needed.
		  if (toexpand != -1)
		    {
		      assert(toexpand < (int) todo.size());
		      todo[toexpand].it->fire_all();
		      ++expanded_;
		    }

		  dst->destroy();
		}
	    }
	}
      if (SPOT_UNLIKELY(Checker))
	{
	  //print_dot(std::cout, state_space_);
	  scc_info si(state_space_);
	  bool all_cycles_expanded = true;
	  for (unsigned int i = 0; i < si.scc_count(); ++i)
	    if (!si.is_trivial(i))
	      all_cycles_expanded =
		all_cycles_expanded &&  si.acc(i)
		&& is_inherently_weak_scc(si, i);

	  if (!all_cycles_expanded)
	    {
	      std::cerr << "ERROR ! SOME CYCLES ARE NOT EXPANDED!\n";
	      exit (1);
	    }
	  // dump_scc_info_dot(std::cout, state_space_, &si);
	}
    }

    std::string dump()
    {
      return
	" states           : " + std::to_string(states_)           + '\n' +
	" transitions      : " + std::to_string(transitions_)      + '\n' +
	" already_expanded : " + std::to_string(already_expanded_) + '\n' +
	" expanded         : " + std::to_string(expanded_)         + '\n' +
	" backedges        : " + std::to_string(backedges_)        + '\n' +
	" max_dfs_size     : " + std::to_string(max_dfssize_)      + '\n' +
	" cumul_reduced    : " + std::to_string(cumul_red_)        + '\n' +
	" cumul_enabled    : " + std::to_string(cumul_en_)         + '\n' +
	proviso_.dump();
    }

    std::string dump_csv()
    {
      return
	std::to_string(states_)           + ',' +
	std::to_string(transitions_)      + ',' +
	std::to_string(already_expanded_) + ',' +
	std::to_string(expanded_)         + ',' +
	std::to_string(backedges_)        + ',' +
	std::to_string(max_dfssize_)      + ',' +
	std::to_string(cumul_red_)        + ',' +
	std::to_string(cumul_en_)         + ',' +
	proviso_.dump_csv();
    }

    // ----------------------------------------------------------
    // Implement the dfs_inspector

    virtual int dfs_position(const state* st) const
    {
      if (seen.find(st) != seen.end())
	return seen[st].dfs_position;
      return -1;
    }
    virtual const state* dfs_state(int position) const
    {
      assert(position < (int)todo.size());
      return todo[position].src;
    }
    virtual twa_succ_iterator* get_iterator(unsigned dfs_position) const
    {
      assert(dfs_position < todo.size());
      return todo[dfs_position].it;
    }
    virtual bool visited(const state* s) const
    {
      return seen.find(s) != seen.end();
    }

    virtual std::vector<bool>& get_colors(const state* st) const
    {
      assert(seen.find(st) != seen.end());
      return seen[st].colors;
    }
    virtual int& get_weight(const state* st) const
    {
      assert(seen.find(st) != seen.end());
      return seen[st].weight;
    }

  private:
    const_twa_ptr aut_;		///< The spot::tgba to explore.
    proviso& proviso_;
    unsigned dfs_number = 0;
    unsigned int states_ = 0;
    unsigned int transitions_ = 0;
    unsigned int already_expanded_ = 0;
    unsigned int expanded_ = 0;
    unsigned int backedges_ = 0;
    unsigned int max_dfssize_ = 0;
    unsigned int cumul_red_ = 0;
    unsigned int cumul_en_ = 0;

    struct state_info
    {
      unsigned live_number;	///< Unique id
      int dfs_position;
      bool expanded_; 		/// Only used by the checker
      std::vector<bool> colors; ///< set by proviso
      int weight; 		///< set by proviso
    };
    typedef std::unordered_map<const state*, state_info,
			       state_ptr_hash, state_ptr_equal> seen_map;
    mutable seen_map seen;		///< States already seen.
    struct stack_item
    {
      const state* src;
      twa_succ_iterator* it;
    };
    std::deque<stack_item> todo; ///< the DFS stack
    std::vector<int> roots;
    int_unionfind_IPC_LRPC_MS uf;
  };
}