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
#include "misc/minato.hh"

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
          : automata_(const_cast<tgba*> (t)),
            rel_(bddtrue),
            bdd_false_(bdd_ithvar(automata_->get_dict()
                                  ->register_anonymous_variables
                                    (1,
                                     automata_))),
            po_size_(0),
            all_class_var_(bddtrue)
        {
          ComplAutomatonRecordState
            acc_compl(automata_,
                      bdd_ithvar(automata_
                                 ->get_dict()
                                 ->register_anonymous_variables
                                 (1, automata_)));

          used_var_.push_back(acc_compl.init_);
          all_class_var_ = acc_compl.init_;


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
          {
            free_var_.push(i);
            all_class_var_ &= bdd_ithvar(i);
          }

          relation_[bddtrue] = bddtrue;
        }

        ~Simulation()
        {
          AccComplAutomaton acc_compl(automata_);
          acc_compl.revert();
        }



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
            std::cout << "nb_partition: " << bdd_lstate_.size() << std::endl;
          }

          return build_result();
        }

        bdd strip_neg_acc(bdd in)
        {
          if (in == bddtrue)
            return bddtrue;
          assert(in != bddfalse);
          if (bdd_high(in) == bddfalse)
            return strip_neg(bdd_low(in));
          assert(bdd_low(in) == bddfalse);
          return bdd_ithvar(bdd_var(in)) & strip_neg(bdd_high(in));
        }


        // Take a state and compute its Ni.
        bdd compute_sig(const state* src)
        {
          std::cout << "\nState: " << automata_->format_state(src) << std::endl;
          tgba_succ_iterator* sit = automata_->succ_iter(src);
          bdd res = bddfalse;
          // std::cout << "previous_it_class_ src: " << previous_it_class_[src]
          //           << std::endl;
          for (sit->first(); !sit->done(); sit->next())
          {
            const state* dst = sit->current_state();
            bdd before_acc
              = strip_neg_acc(bdd_support(sit->current_acceptance_conditions()));

            // We want to have the information that the acceptance
            // condition is bdd false. But if you keep bddfalse, our
            // signature is meaningless.
            bdd acc = before_acc == bddfalse ? bddtrue : before_acc;
#if 0
            std::cout << "previous_it_class_ dst: " << previous_it_class_[dst]
                      << std::endl;

            std::cout << "acc: " << acc << std::endl;
            std::cout << "current_cond: " << sit->current_condition()
                      << std::endl;
#endif
            // bdd all = automata_->all_acceptance_conditions();
            // bdd sup = bdd_support(all);

            // acc = bdd_exist(sup, acc);

            // The rel_ is here to allow the bdd to know which class
            // dominates another class.
            bdd to_add = acc & sit->current_condition()
              & previous_it_class_[dst];

            // std::cout << "compute_sig: previous_it_class_[dst]: "
            //           << previous_it_class_[dst] << std::endl;

            // In fact, I don't think rel_ must be used here, but only in
            // the final computation, that's why there is comments.

            // I need to create temporary variable because otherwise,
            // I've got a compilation error.
            // bdd left = (res | to_add) & rel_;
            // bdd right = res & rel_;

            std::cout << "to add: " << to_add << std::endl;
            // Include the signature implied by this transition in the
            // signature of this state only if `to_add' is i-maximal.
            res |= to_add;
            std::cout << "res: " << res << std::endl;
            dst->destroy();
          }

          delete sit;
          std::cout << "res VS res&rel_: " << res << " VS "
                    << (res & rel_)
                    << std::endl;
          std::cout << "rel: " << rel_ << std::endl;

          return res & rel_;
        }

        void update_sig()
        {
          // At this time, current_class_ must be empty.  It implies
          // that the "previous_it_class_ = current_class_" must be
          // done before.
          assert(current_class_.empty());
          std::cout << "Next iteration\n" << std::endl;
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
          bdd new_rel = bddtrue;
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
                std::cout << "\n" << std::endl;
                std::cout << "" << it1->second
                          << " => " << it2->second << std::endl;
                new_rel &= (it1->second >> it2->second);
                accu &= it2->second;
                ++po_size_;
              }
            }
            relation_[it1->second] = accu;
          }

          rel_ = new_rel;
        }

        // This function is a temporary hack to solve the problem
        // implied by including rel_ in the signature. It takes a bdd
        // which is a conjunction of variable, and returns its bdd
        // without the variable which are in all_class_var_ and which
        // are negative.
        bdd strip_neg(bdd in)
        {
          if (in == bddtrue)
            return bddtrue;
          assert(in != bddfalse);
          // If the current var doesn't appear in rel_, we keep it
          // as in the original.
          if (bdd_exist(rel_, bdd_ithvar(bdd_var(in))) == rel_)
          {
            if (bdd_high(in) == bddfalse)
              return bdd_nithvar(bdd_var(in)) & strip_neg(bdd_low(in));
            else
              return bdd_ithvar(bdd_var(in)) & strip_neg(bdd_high(in));
          }
          if (bdd_high(in) == bddfalse)
            return strip_neg(bdd_low(in));
          assert(bdd_low(in) == bddfalse);

          return bdd_ithvar(bdd_var(in)) & strip_neg(bdd_high(in));
        }

        // This is a little function to get the state corresponding to
        // this bdd.
        inline
        int get_state(std::map<bdd, unsigned, bdd_less_than>& bdd2state,
                      bdd me,
                      unsigned int& current_max,
                      tgba_explicit_number* res)
        {
          std::cout << "me: " << me << std::endl;
          if (bdd2state.find(me) == bdd2state.end())
          {
            bdd2state[me] = ++current_max;
            res->add_state(current_max);
          }

          std::cout << "bdd2state.size(): " << bdd2state.size() << std::endl;
          assert(current_max <= bdd_lstate_.size());

          return bdd2state[me];
        }


        bdd normalize(bdd in)
        {
          bdd res = bddfalse;

          while (in != bddfalse)
          {
            bdd one = bdd_satone(in);
            in -= one;

            res |= strip_neg(one);
          }

          return res;
        }


        bdd compute_sig_for_build(const state* src)
        {
          tgba_succ_iterator* sit = automata_->succ_iter(src);
          bdd res = bddfalse;

          for (sit->first(); !sit->done(); sit->next())
          {
            const state* dst = sit->current_state();
            bdd before_acc
              = strip_neg_acc(bdd_support(sit->current_acceptance_conditions()));

            // We want to have the information that the acceptance
            // condition is bdd false. But if you keep bddfalse, our
            // signature is meaningless.
            bdd acc = before_acc == bddfalse ? bddtrue : before_acc;

            // The rel_ is here to allow the bdd to know which class
            // dominates another class.
            bdd to_add = acc & sit->current_condition()
              & relation_[previous_it_class_[dst]];

            // Include the signature implied by this transition in the
            // signature of this state only if `to_add' is i-maximal.
            res |= to_add;
            dst->destroy();
          }

          delete sit;
          return res;
        }

        // Build the minimal resulting automaton
        tgba* build_result()
        {
          std::cout << "BUILD RESULT" << std::endl;

          // Now we need to create a state per partition. But the
          // problem is that we don't know exactly the class. We know
          // that it is a combination of the acceptance condition
          // contained in all_class_var_. So we need to make a little
          // workaround. We will create a map which will associate bdd
          // and unsigned.We will create a state each time the class
          // we have does not exist.
          std::map<bdd, unsigned, bdd_less_than> bdd2state;
          unsigned int current_max = 0;
          (void)current_max;
          (void)bdd2state;

          bdd all_acceptance_conditions
            = automata_->all_acceptance_conditions();

          // We have all the automata_ acceptance conditions
          // complemented.  So we need to complement it when adding a
          // transition.  We *must* keep the complemented because it
          // is easy to know if an acceptance condition is maximal or
          // not.
          AccCompl reverser(all_acceptance_conditions,
                            automata_->neg_acceptance_conditions());

          typedef tgba_explicit_number::transition trs;
          tgba_explicit_number* res
            = new tgba_explicit_number(automata_->get_dict());
          res->set_acceptance_conditions
            (all_acceptance_conditions);

          // print_partition();

          // For each partition, we will create a state, and create
          // all these transitions.
          for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
               it != bdd_lstate_.end();
               ++it)
          {
            bdd sig = compute_sig_for_build(*(it->second.begin()));//normalize(it->first);
            bdd sup_sig = bdd_support(sig);
            bdd sup_all_acc = bdd_support(all_acceptance_conditions);
            // Non atomic propositions variables (= acc and class)
            bdd nonapvars = sup_all_acc & bdd_support(all_class_var_);
            bdd sup_all_atomic_prop = bdd_exist(bdd_support(sig), nonapvars);
            bdd all_atomic_prop = bdd_exist(sig, nonapvars);

            std::cout << "\n\nNew state: "
                      << automata_->format_state(*it->second.begin())
                      << std::endl;

            std::cout << "sig: " << sig
//                      << "\nnonapvars: " << nonapvars
//                      << "\nall_atomic_prop: " << all_atomic_prop
                      << std::endl;

            while (all_atomic_prop != bddfalse)
            {
              bdd one = bdd_satoneset(all_atomic_prop,
                                      sup_all_atomic_prop,
                                      bddtrue);
              all_atomic_prop -= one;

              // std::cout << "sig & one: " << (sig & one)
              //           << std::endl;
              minato_isop isop(sig & one);

              bdd cond_acc_dest;
              while ((cond_acc_dest = isop.next()) != bddfalse)
              {
                bdd dest = bdd_existcomp(cond_acc_dest,
                                         all_class_var_);
                bdd acc
                  = strip_neg_acc(bdd_support(bdd_existcomp(cond_acc_dest, sup_all_acc)));
                bdd cond = bdd_existcomp(cond_acc_dest, sup_all_atomic_prop);

//                std::cout << "acc1: " << acc << std::endl;

                acc = reverser.reverse_complement(acc);

                // std::cout << "cond_acc_dest: " << cond_acc_dest << std::endl;
                // std::cout << "acc: " << acc << std::endl;
                // std::cout << "cond: " << cond << std::endl;
                // std::cout << "dest: " << dest << std::endl;

                // std::cout << "src: " << previous_it_class_[*it->second.begin()]
                //           << std::endl;

                int src = get_state(bdd2state,
                                    relation_[previous_it_class_[*it->second.begin()]],
                                    current_max,
                                    res);
                int dst = get_state(bdd2state,
                                    dest,
                                    current_max,
                                    res);

                std::cout << "src -> dst: " << src << " -> " << dst << std::endl;
                std::cout << "acc: " << acc << "; cond: " << cond
                          << std::endl <<  std::endl;

                tgba_explicit_number::transition* t
                  = res->create_transition(src , dst);
                res->add_conditions(t, cond);
                res->add_acceptance_conditions(t, acc);
              }
            }
          }

          res->set_init_state(get_state(bdd2state,
                                        previous_it_class_
                                          [automata_->get_init_state()],
                                        current_max,
                                        res));

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
            std::cout << "partition: " << it->first << std::endl;

            for (std::list<const state*>::iterator it_s = it->second.begin();
                 it_s != it->second.end();
                 ++it_s)
            {
              std::cout << "\t- "
                        << automata_->format_state(*it_s) << std::endl;
            }
          }

          std::cout << "\nPrevious iteration" << std::endl;

          for (map_state_bdd::const_iterator it = previous_it_class_.begin();
               it != previous_it_class_.end();
               ++it)
          {
            std::cout << automata_->format_state(it->first)
                      << " was in " << it->second << std::endl;
          }


        }


      private:
        // The automaton which is simulated.
        tgba* automata_;

        // The bdd which represents the domination relation between the
        // class. It is the po.
        bdd rel_;

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

        unsigned int size_automata_;

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
