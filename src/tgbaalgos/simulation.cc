// Copyright (C) 2009, 2010, 2011, 2012 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include <queue>
#include <map>
#include <utility>
#include "tgba/tgbaexplicit.hh"
#include "ltlast/formula.hh"
#include "ltlast/allnodes.hh"
#include "simulation.hh"
#include "misc/acccompl.hh"

// The way we will develop this algorithm is the following:
// We'll take an automaton, and reverse all these acceptance conditions.
// Then, to check if a transition i-dominates another, we'll use the bdd:
// sig(transA) = cond(trans) & acc(trans) & class(trans->state).
// Idem for sig(transB). Then, the transA dominates the transB if
// sig(transA) & sig(transB) == sig(transA).
// This work likely without the class, but with the class we need to
// include the relation. To achieve this goal, we'll use a bdd `rel',
// defined like this:
// `rel' is the conjunction of all the class implication. For example, if
// C(q1) => C(q2), and C(q2) => C(q3) in `rel', we'll see:
// rel = C(q1) => C(q2) & C(q2) => C(q3)
// We let the library do the simplification work if needed. This is
// useful in the domination computation:
// sig(transA) & sig(transB) & rel == sig(transA) & rel
// So the algorithm is cut into several step:
//
// 1. Running through the ba and switch the acceptance condition to their
//    negation, and initializing rel to bddtrue. This function is the
//    constructor of Simulation.
// 2. Entering in the loop.
//    - running through the automaton and computing the signature of each
//      state. This function is `update_sig'.
//    - Entering in a double loop to adapt the partial order, and set
//      rel accordingly. This function is `update_po'.
// 3. Building an automaton with the result, with the condition:
// "a transition in the original automaton appears in the simulated one
// iff this transition is included in the set of i-maximal neighbour."
// This function is `build_output'.
//
// Obviously these functions are possibly cut into several little one.
// This is just the general development idea.



namespace spot
{
  namespace
  {
    // Some useful typedef:

    // Used to get the signature of the state.
    typedef Sgi::hash_map<const state*, bdd,
                          state_ptr_hash, state_ptr_equal> map_state_bdd;

    // Get the list of state for each class.
    typedef std::map<bdd, std::list<const state*>,
                     bdd_less_than> map_bdd_lstate;


    class Simulation
    {
      public:
        Simulation(const tgba* t)
          : automata_(const_cast<tgba*> (t)),
            acc_compl_(automata_)
        {
          // We'll start our work by replacing all the acceptance
          // conditions by their complement.
          acc_compl_.run();
        }

        ~Simulation()
        {
          // Re-invert the complement of all acceptance condition
          acc_compl_.run();
        }


        bdd
        compute_sig(state* src)
        {
          tgba_succ_iterator* sit = automata_->succ_iter(src);
          bdd res = bddtrue;

          for (sit->first(); !sit->done(); sit->next())
          {
            const state* dst = sit->current_state();
            bdd acc = sit->current_acceptance_conditions();

            bdd to_add = previous_it_class_[src] & previous_it_class_[dst]
              & acc & sit->current_condition();

            // Include the signature implied by this transition in the
            // signature of this state.
            res &= to_add;
            dst->destroy();
          }

          delete sit;
          return res;
        }

        void update_sig()
        {

        }



      private:
        // The automaton which is simulated.
        tgba* automata_;

        // The object which run through the automaton and replace all
        // acceptance condition by their complement.
        AccComplAutomaton acc_compl_;

        // The bdd which represents the domination relation between the
        // class. It is the po.
        bdd rel_;

        // Represent the class of each state at the previous iteration.
        map_state_bdd previous_it_class_;
    };


  } // End namespace anonymous.



  tgba*
  simulation(const tgba*)
  {



    return 0;
  }



} // End namespace spot.
