// -*- coding: utf-8 -*-
// Copyright (C) 2008, 2009, 2011, 2013, 2015 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
// Copyright (C) 2003, 2004 Laboratoire d'Informatique de Paris 6
// (LIP6), département Systèmes Répartis Coopératifs (SRC), Université
// Pierre et Marie Curie.
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

#include "misc/hash.hh"
#include "twa/twa.hh"
#include <stack>
#include <deque>

namespace spot
{
  /// \ingroup twa_generic
  /// \brief Iterate over all reachable states of a spot::tgba.
  class SPOT_API tgba_reachable_iterator
  {
  public:
    tgba_reachable_iterator(const const_twa_ptr& a);
    virtual ~tgba_reachable_iterator();

    /// \brief Iterate over all reachable states of a spot::tgba.
    ///
    /// This is a template method that will call add_state(),
    /// next_state(), want_state(), start(), end(), process_state(),
    /// and process_link(), while it iterates over states.
    virtual void run();

    /// \name Todo list management.
    ///
    /// See e.g.
    /// spot::tgba_reachable_iterator_breadth_first for precanned
    /// implementations for these functions.
    /// \{
    /// \brief Called by run() to register newly discovered states.
    virtual void add_state(const state* s) = 0;
    /// \brief Called by run() to obtain the next state to process.
    virtual const state* next_state() = 0;
    /// \}

    /// Called by add_state or next_states implementations to filter
    /// states.  Default implementation always return true.
    virtual bool want_state(const state* s) const;

    /// Called by run() before starting its iteration.
    virtual void start();
    /// Called by run() once all states have been explored.
    virtual void end();

    /// Called by run() to process a state.
    ///
    /// \param s The current state.
    /// \param n A unique number assigned to \a s.
    /// \param si The spot::twa_succ_iterator for \a s.
    virtual void process_state(const state* s, int n, twa_succ_iterator* si);
    /// Called by run() to process a transition.
    ///
    /// \param in_s The source state
    /// \param in The source state number.
    /// \param out_s The destination state
    /// \param out The destination state number.
    /// \param si The spot::twa_succ_iterator positionned on the current
    ///             transition.
    ///
    /// The in_s and out_s states are owned by the
    /// spot::tgba_reachable_iterator instance and destroyed when the
    /// instance is destroyed.
    virtual void process_link(const state* in_s, int in,
			      const state* out_s, int out,
			      const twa_succ_iterator* si);

  protected:
    const_twa_ptr aut_;	///< The spot::tgba to explore.

    typedef std::unordered_map<const state*, int,
			       state_ptr_hash, state_ptr_equal> seen_map;
    seen_map seen;		///< States already seen.
  };

  /// \ingroup twa_generic
  /// \brief An implementation of spot::tgba_reachable_iterator that browses
  /// states breadth first.
  class SPOT_API tgba_reachable_iterator_breadth_first :
    public tgba_reachable_iterator
  {
  public:
    tgba_reachable_iterator_breadth_first(const const_twa_ptr& a);

    virtual void add_state(const state* s);
    virtual const state* next_state();

  protected:
    std::deque<const state*> todo; ///< A queue of states yet to explore.
  };

  /// \ingroup twa_generic
  /// \brief Iterate over all states of an automaton using a DFS.
  class SPOT_API tgba_reachable_iterator_depth_first
  {
  public:
    tgba_reachable_iterator_depth_first(const const_twa_ptr& a);
    virtual ~tgba_reachable_iterator_depth_first();

    /// \brief Iterate over all reachable states of a spot::tgba.
    ///
    /// This is a template method that will call start(), end(),
    /// want_state(), process_state(), and process_link(), while it
    /// iterates over states.
    virtual void run();

    /// Called by add_state or next_states implementations to filter
    /// states.  Default implementation always return true.
    virtual bool want_state(const state* s) const;

    /// Called by run() before starting its iteration.
    virtual void start();
    /// Called by run() once all states have been explored.
    virtual void end();

    /// Called by run() to process a state.
    ///
    /// \param s The current state.
    /// \param n A unique number assigned to \a s.
    /// \param si The spot::twa_succ_iterator for \a s.
    virtual void process_state(const state* s, int n, twa_succ_iterator* si);

    /// Called by run() to process a state before it is popped.
    /// It this method return true the state will be popped, otherwise
    /// the state will not be popped. In this case the iterator must be
    /// set to a valid position (other than end()). So if this method
    /// return false, the iterator must be change. Typically this happen
    /// during model checking with partial order reductions.
    ///
    /// \param s The current state.
    /// \param n A unique number assigned to \a s.
    /// \param si The spot::tgba_succ_iterator for \a s.
    virtual bool will_pop_state(const state* s, int n, twa_succ_iterator* si);

    /// Called by run() to process a transition.
    ///
    /// \param in_s The source state
    /// \param in The source state number.
    /// \param out_s The destination state
    /// \param out The destination state number.
    /// \param si The spot::twa_succ_iterator positionned on the current
    ///             transition.
    ///
    /// The in_s and out_s states are owned by the
    /// spot::tgba_reachable_iterator instance and destroyed when the
    /// instance is destroyed.
    virtual void process_link(const state* in_s, int in,
			      const state* out_s, int out,
			      const twa_succ_iterator* si);

  protected:
    const_twa_ptr aut_;		///< The spot::tgba to explore.

    typedef std::unordered_map<const state*, int,
			       state_ptr_hash, state_ptr_equal> seen_map;
    seen_map seen;		///< States already seen.
    struct stack_item
    {
      const state* src;
      int src_n;
      twa_succ_iterator* it;
    };
    std::deque<stack_item> todo; ///< the DFS stack

    /// Push a new state in todo.
    virtual void push(const state* s, int sn);
    /// Pop the DFS stack.
    virtual void pop();
  };

  /// \ingroup twa_generic
  /// \brief Iterate over all states of an automaton using a DFS.
  ///
  /// This variant also maintains a set of states that are on the DFS
  /// stack.  It can be checked using the on_stack() method.
  class tgba_reachable_iterator_depth_first_stack
    : public tgba_reachable_iterator_depth_first
  {
  public:
    tgba_reachable_iterator_depth_first_stack(const const_twa_ptr& a);
    /// \brief Whether state sn is on the DFS stack.
    ///
    /// Note the destination state of a transition is only pushed to
    /// the stack after process_link() has been called.
    bool on_stack(int sn) const;
  protected:
    virtual void push(const state* s, int sn);
    virtual void pop();

    std::unordered_set<int> stack_;
  };
}