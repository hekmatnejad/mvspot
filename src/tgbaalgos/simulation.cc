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

// Note from the 12/03:
//   * We can switch the renaming before the update of the po.


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


    // I was stuck here, because I need to run through the automaton, to
    // complement the acceptance condition on transition, AND to register
    // each state into a map, I'll create this class which inherits from
    // AccComplAutomaton, and decorate its process state method.
    class ComplAutomatonRecordState: public AccComplAutomaton
    {
        typedef AccComplAutomaton super_type;

      public:
        ComplAutomatonRecordState(tgba* a, bdd i)
          : AccComplAutomaton(a),
            init_(i)
        {
        }

        // b and c are useless but needed to call the super_type method.
        void process_state(const state* s, int b, tgba_succ_iterator* c)
        {
          super_type::process_state(s, b, c);

          previous_it_class_[s] = init_;
        }

      public:
        // We do not really need an encapsulation here. This object is
        // here only to be copied after the run.
        map_state_bdd previous_it_class_;

        // We need to know the bdd which will be the initial class.
        bdd init_;
    };


    class Simulation
    {
        // Shortcut used in update_po and go_to_next_it.
        typedef std::map<bdd, bdd, bdd_less_than> map_bdd_bdd;
      public:
        Simulation(const tgba* t)
          : automata_(const_cast<tgba*> (t)),
            bdd_false_(bdd_ithvar(automata_->get_dict()
                                  ->register_anonymous_variables
                                    (1,
                                     automata_)))
        {
          ComplAutomatonRecordState
            acc_compl(automata_,
                      bdd_ithvar(automata_
                                 ->get_dict()
                                 ->register_anonymous_variables
                                 (1, automata_)));

          used_var_.push_back(acc_compl.init_);

          rel_ = acc_compl.init_ >> acc_compl.init_;

          // We'll start our work by replacing all the acceptance
          // conditions by their complement.
          acc_compl.run();

          size_automata_ = acc_compl.size;

          previous_it_class_ = acc_compl.previous_it_class_;

          // Now, we have to get the bdd which will represent the
          // class. We register one bdd by state, because in the worst
          // case, |Class| == |State|.  In the call to
          // register_anonymous_variables, there is a "size_automata_ -
          // 1" because we have already register one.
          unsigned set_num =
            automata_
              ->get_dict()
              ->register_anonymous_variables(size_automata_ - 1,
                                             automata_);

          for (unsigned i = set_num; i < set_num + size_automata_ - 1; ++i)
            free_var_.push(i);
        }

        ~Simulation()
        {
          AccComplAutomaton acc_compl(automata_);

          acc_compl.revert();
        }


        tgba* run()
        {
          update_sig();
          go_to_next_it();

          return 0;
        }


        // Take a state and compute its Ni.
        bdd compute_sig(const state* src)
        {
          tgba_succ_iterator* sit = automata_->succ_iter(src);
          bdd res = bddfalse;

          for (sit->first(); !sit->done(); sit->next())
          {
            const state* dst = sit->current_state();
            bdd before_acc = sit->current_acceptance_conditions();
            // We want to have the information that the acceptance
            // condition is bdd false. But if you keep bddfalse, our
            // signature is meaningless.
            bdd acc = before_acc == bddfalse ? bdd_false_ : before_acc;

            // The rel_ is here to allow the bdd to know which class
            // dominates another class.
            bdd to_add = previous_it_class_[src] & previous_it_class_[dst]
              & acc & sit->current_condition();


            // In fact, I don't think rel_ must be used here, but only in
            // the final computation, that's why there is comments.

            // // I need to create temporary variable because otherwise,
            // // I've got a compilation error.
            // bdd left = (res | to_add) & rel_;
            // bdd right = res & rel_;

            // // Include the signature implied by this transition in the
            // // signature of this state only if `to_add' is i-maximal.
            // if (left != right)
            res |= to_add;
            dst->destroy();
          }

          delete sit;
          return res;
        }

        void update_sig()
        {
          // At this time, current_class_ must be empty.  It implies
          // that the "previous_it_class_ = current_class_" must be
          // done before.
          assert(current_class_.empty());

          // Here we suppose that previous_it_class_ always contains
          // all the reachable states of this automaton. We do not
          // have to make (again) a traversal. We just have to run
          // through this map.
          for (map_state_bdd::iterator it = previous_it_class_.begin();
               it != previous_it_class_.end();
               ++it)
          {
            const state* src = it->first;

            bdd_lstate_[compute_sig(src)].push_back(src);
          }
        }

        // This method rename the color set, update the partial order.
        void go_to_next_it()
        {
          int nb_new_color = bdd_lstate_.size() - used_var_.size();

          for (int i = 0; i < nb_new_color; ++i)
          {
            assert(!free_var_.empty());
            used_var_.push_back(bdd_ithvar(free_var_.front()));
            free_var_.pop();
          }

          assert(bdd_lstate_.size() == used_var_.size());

          // Now we make a temporary hash_table which links the tuple
          // "C^(i-1), N^(i-1)" to the new class coloring.  If we
          // rename the class before updating the partial order, we
          // loose the information, and if we make it after, I can't
          // figure out how to apply this renaming on rel_.
          // It adds a data structure but it solves our problem.
          map_bdd_bdd now_to_next;

          std::list<bdd>::iterator it_bdd = used_var_.begin();

          for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
               it != bdd_lstate_.end();
               ++it)
          {
            now_to_next[it->first] = *it_bdd;
            ++it_bdd;
          }

          update_po(now_to_next);

          // We run through the map bdd/list<state>, and we update
          // the previous_it_class_ with the new data.
          it_bdd = used_var_.begin();
          for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
               it != bdd_lstate_.end();
               ++it)
          {
            for (std::list<const state*>::iterator it_s = it->second.begin();
                 it_s != it->second.end();
                 ++it_s)
            {
              previous_it_class_[*it_s] = *it_bdd;
            }
            ++it_bdd;
          }
        }

        // This function compute the new po with previous_it_class_
        // and the argument. `now_to_next' contains the relation
        // "C^(i-1)&N^(i-1)" -> "A" and `next_to_now' contains the
        // opposite.
        void update_po(const map_bdd_bdd& now_to_next)
        {
          bdd new_rel = bddtrue;

          for (map_bdd_bdd::const_iterator it1 = now_to_next.begin();
               it1 != now_to_next.end();
               ++it1)
          {
            for (map_bdd_bdd::const_iterator it2 = now_to_next.begin();
                 it2 != now_to_next.end();
                 ++it2)
            {
              bdd left = it1->first & it2->first & rel_;
              bdd right = it1->first & rel_;

              if ((left & right) == right)
                new_rel &= (it1->second >> it2->second);
            }
          }

          rel_ = new_rel;
        }


      private:
        // The automaton which is simulated.
        tgba* automata_;

        // The bdd which represents the domination relation between the
        // class. It is the po.
        bdd rel_;

        // Represent the class of each state at the previous iteration.
        map_state_bdd previous_it_class_;

        // The class at the current iteration.
        map_state_bdd current_class_;

        // The list of state for each class at the current_iteration.
        // Computed in `update_sig'.
        map_bdd_lstate bdd_lstate_;

        // bddfalse is valid as acceptance condition, but it leads to
        // destroy the meaning of the signature: a /\ bddfalse /\ ... = F.
        // So this bdd_false will be the replacement of bddfalse.
        bdd bdd_false_;

        // The queue of free bdd. They will be used as the identifier
        // for the class.
        std::queue<int> free_var_;

        // The list of used bdd. They are in used as identifier for class.
        std::list<bdd> used_var_;

        unsigned int size_automata_;
    };
  } // End namespace anonymous.



  tgba*
  simulation(const tgba*)
  {



    return 0;
  }



} // End namespace spot.
