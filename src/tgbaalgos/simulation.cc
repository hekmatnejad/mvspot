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
#include "simulation.hh"
#include "misc/acccompl.hh"
#include "misc/minato.hh"
#include "tgba/bddprint.hh"

// The way we developed this algorithm is the following: We'll take
// an automaton, and reverse all these acceptance conditions.  Then,
// to check if a transition i-dominates another, we'll use the bdd:
// sig(transA) = cond(trans) & acc(trans) & implied(class(trans->state)).
// Idem for sig(transB). The 'implied' (represented by a hash table
// 'relation_' in the implementation) is a conjunction of all the class
// dominated by the class of the destination. This is how the relation is
// included in the signature. It makes the simplifications alone, and the
// work is done.
// the algorithm is cut into several step:
//
// 1. Run through the tgba and switch the acceptance condition to their
//    negation, and initializing relation_ by the 'init_ -> init_' where
//    init_ is the bdd which represents the class. This function is the
//    constructor of Simulation.
// 2. Enter in the loop (run).
//    - Rename the class.
//    - run through the automaton and computing the signature of each
//      state. This function is `update_sig'.
//    - Enter in a double loop to adapt the partial order, and set
//      'relation_' accordingly. This function is `update_po'.
// 3. Rename the class.
// 4. Building an automaton with the result, with the condition:
// "a transition in the original automaton appears in the simulated one
// iff this transition is included in the set of i-maximal neighbour."
// This function is `build_output'.
// The automaton simulated is recomplemented to come back to its initial
// state when the object Simulation is destroyed.
//
// Obviously these functions are possibly cut into several little one.
// This is just the general development idea.

// How to use isop:
// I need all variable non_acceptance & non_class.
// bdd_support(sig(X)): All var
// bdd_support(sig(X)) - allacc - allclassvar


namespace spot
{
  namespace
  {
    // Some useful typedef:

    // Used to get the signature of the state.
    typedef Sgi::hash_map<const state*, bdd,
                          state_ptr_hash,
                          state_ptr_equal> map_state_bdd;

    typedef Sgi::hash_map<const state*, unsigned,
                          state_ptr_hash,
                          state_ptr_equal> map_state_unsigned;


    // Get the list of state for each class.
    typedef std::map<bdd, std::list<const state*>,
                     bdd_less_than> map_bdd_lstate;


#define DEBUGSIMUL
#ifdef DEBUGSIMUL
# define PRINT(line) line
#endif

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
          : a_(const_cast<tgba*> (t)),
            bdd_false_(bdd_ithvar(a_->get_dict()
                                  ->register_anonymous_variables
                                    (1,
                                     a_))),
            po_size_(0),
            all_class_var_(bddtrue)
        {
          ComplAutomatonRecordState
            acc_compl(a_,
                      bdd_ithvar(a_
                                 ->get_dict()
                                 ->register_anonymous_variables
                                 (1, a_)));

          used_var_.push_back(acc_compl.init_);
          all_class_var_ = acc_compl.init_;


          // We'll start our work by replacing all the acceptance
          // conditions by their complement.
          acc_compl.run();

          size_a_ = acc_compl.size;

          previous_it_class_ = acc_compl.previous_it_class_;

          // Now, we have to get the bdd which will represent the
          // class. We register one bdd by state, because in the worst
          // case, |Class| == |State|.  In the call to
          // register_anonymous_variables, there is a "size_a_ -
          // 1" because we have already register one.
          unsigned set_num =
            a_
              ->get_dict()
              ->register_anonymous_variables(size_a_ - 1,
                                             a_);

          for (unsigned i = set_num; i < set_num + size_a_ - 1; ++i)
          {
            free_var_.push(i);
            all_class_var_ &= bdd_ithvar(i);
          }

          relation_[acc_compl.init_] = acc_compl.init_;
        }

        ~Simulation()
        {
          AccComplAutomaton acc_compl(a_);
          acc_compl.revert();
        }


        // We update the name of the class.
        void update_previous_it_class()
        {
          std::list<bdd>::iterator it_bdd = used_var_.begin();

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

        // The core loop of the algorithm.
        tgba* run()
        {
          unsigned int nb_partition_before = 0;
          unsigned int nb_po_before = po_size_ - 1;
          while (nb_partition_before != bdd_lstate_.size()
                 || nb_po_before != po_size_)
          {
            update_previous_it_class();
            nb_partition_before = bdd_lstate_.size();
            bdd_lstate_.clear();
            nb_po_before = po_size_;
            po_size_ = 0;
            update_sig();
            go_to_next_it();
          }

          update_previous_it_class();
          return build_result();
        }

        // Take a state and compute its signature.
        bdd compute_sig(const state* src)
        {
          tgba_succ_iterator* sit = a_->succ_iter(src);
          bdd res = bddfalse;

          for (sit->first(); !sit->done(); sit->next())
          {
            const state* dst = sit->current_state();
            bdd acc = sit->current_acceptance_conditions();

            // to_add is a conjunction of the acceptance condition,
            // the label of the transition and the class of the
            // destination and all the class it implies.
            bdd to_add = acc & sit->current_condition()
              & relation_[previous_it_class_[dst]];

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
        }

        // This function compute the new po with previous_it_class_
        // and the argument. `now_to_next' contains the relation
        // "C^(i-1)&N^(i-1)" -> "A" and `next_to_now' contains the
        // opposite.
        void update_po(const map_bdd_bdd& now_to_next)
        {
          // This loop follows the pattern given by the paper.
          // foreach class do
          // |  foreach class do
          // |  | update po if needed
          // |  od
          // od

          for (map_bdd_bdd::const_iterator it1 = now_to_next.begin();
               it1 != now_to_next.end();
               ++it1)
          {
            bdd accu = it1->second;

            for (map_bdd_bdd::const_iterator it2 = now_to_next.begin();
                 it2 != now_to_next.end();
                 ++it2)
            {
              if (it1 == it2)
                continue;

              // We detect that "a&b -> a" by testing "a&b = a".
              if ((it1->first & it2->first) == (it1->first))
              {
                accu &= it2->second;
                ++po_size_;
              }
            }
            relation_[it1->second] = accu;
          }
        }

        // Build the minimal resulting automaton
        tgba* build_result()
        {
          // Now we need to create a state per partition. But the
          // problem is that we don't know exactly the class. We know
          // that it is a combination of the acceptance condition
          // contained in all_class_var_. So we need to make a little
          // workaround. We will create a map which will associate bdd
          // and unsigned.We will create a state each time the class
          // we have does not exist.
          std::map<bdd, unsigned, bdd_less_than> bdd2state;
          unsigned int current_max = 0;

          bdd all_acceptance_conditions
            = a_->all_acceptance_conditions();

          // We have all the a_ acceptance conditions
          // complemented.  So we need to complement it when adding a
          // transition.  We *must* keep the complemented because it
          // is easy to know if an acceptance condition is maximal or
          // not.
          AccCompl reverser(all_acceptance_conditions,
                            a_->neg_acceptance_conditions());

          typedef tgba_explicit_number::transition trs;
          tgba_explicit_number* res
            = new tgba_explicit_number(a_->get_dict());
          res->set_acceptance_conditions
            (all_acceptance_conditions);

          bdd sup_all_acc = bdd_support(all_acceptance_conditions);
          // Non atomic propositions variables (= acc and class)
          bdd nonapvars = sup_all_acc & bdd_support(all_class_var_);

          // Create one state per partition.
          for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
               it != bdd_lstate_.end(); ++it)
          {
            res->add_state(++current_max);
            bdd part = previous_it_class_[*it->second.begin()];

            // The difference between the two next lines is:
            // the first says "if you see A", the second "if you
            // see A and all the class implied by it".
            bdd2state[part] = current_max;
            bdd2state[relation_[part]] = current_max;
          }

          // For each partition, we will create
          // all the transitions between the states.
          for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
               it != bdd_lstate_.end();
               ++it)
          {
            // Get the signature.
            bdd sig = compute_sig(*(it->second.begin()));

            // Get all the variable in the signature.
            bdd sup_sig = bdd_support(sig);

            // Get the variable in the signature which represents conditions.
            bdd sup_all_atomic_prop = bdd_exist(sup_sig, nonapvars);

            // Get the part of the signature composed only with the atomic
            // proposition.
            bdd all_atomic_prop = bdd_exist(sig, nonapvars);

            while (all_atomic_prop != bddfalse)
            {
              bdd one = bdd_satoneset(all_atomic_prop,
                                      sup_all_atomic_prop,
                                      bddtrue);
              all_atomic_prop -= one;

              minato_isop isop(sig & one);

              bdd cond_acc_dest;
              while ((cond_acc_dest = isop.next()) != bddfalse)
              {
                // Take the transition, and keep only the variable which
                // are used to represent the class.
                bdd dest = bdd_existcomp(cond_acc_dest,
                                         all_class_var_);

                // Keep only ones who are acceptance condition.
                bdd acc = bdd_existcomp(cond_acc_dest, sup_all_acc);

                // Keep the other !
                bdd cond = bdd_existcomp(cond_acc_dest, sup_all_atomic_prop);

                // Because we have complemented all the acceptance condition
                // on the input automaton, we must re invert them to create
                // a new transition.
                acc = reverser.reverse_complement(acc);

                // Take the id of the source and destination.
                // To know the source, we must take a random state in the
                // list which is in the class we currently work on.
                int src = bdd2state[previous_it_class_[*it->second.begin()]];
                int dst = bdd2state[dest];

                // src or dst == 0 means "dest" or "prev..." isn't in the map.
                // so it is a bug.
                assert(src != 0 && dst != 0);

                // Create the transition, add the condition and the
                // acceptance condition.
                tgba_explicit_number::transition* t
                  = res->create_transition(src , dst);
                res->add_conditions(t, cond);
                res->add_acceptance_conditions(t, acc);
              }
            }
          }

          res->set_init_state(bdd2state[previous_it_class_
                                         [a_->get_init_state()]]);
          res->merge_transitions();
          return res;
        }


        // Debug:
        void print_partition()
        {
          for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
               it != bdd_lstate_.end();
               ++it)
          {
            std::cerr << "partition: "
                      << bdd_format_set(a_->get_dict(), it->first) << std::endl;

            for (std::list<const state*>::iterator it_s = it->second.begin();
                 it_s != it->second.end();
                 ++it_s)
            {
              std::cerr << "  - "
                        << a_->format_state(*it_s) << std::endl;
            }
          }

          std::cerr << "\nPrevious iteration\n" << std::endl;

          for (map_state_bdd::const_iterator it = previous_it_class_.begin();
               it != previous_it_class_.end();
               ++it)
          {
            std::cerr << a_->format_state(it->first)
                      << " was in "
                      << bdd_format_set(a_->get_dict(), it->second)
                      << std::endl;
          }
        }

      private:
        // The automaton which is simulated.
        tgba* a_;

        // Relation is aimed to represent the same thing than
        // rel_. The difference is in the way it does.
        // If A => A /\ A => B, rel will be (!A U B), but relation_
        // will have A /\ B at the key A. This trick is due to a problem
        // with the computation of the resulting automaton with the signature.
        // rel_ will pollute the meaning of the signature.
        map_bdd_bdd relation_;


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

        // Size of the automaton.
        unsigned int size_a_;

        // Used to know when there is no evolution in the po. Updated
        // in the `update_po' method.
        unsigned int po_size_;

        // All the class variable:
        bdd all_class_var_;
    };
  } // End namespace anonymous.

  tgba*
  simulation(const tgba* t)
  {
    Simulation foo(t);

    return foo.run();
  }
} // End namespace spot.
