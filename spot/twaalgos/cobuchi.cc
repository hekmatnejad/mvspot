// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement
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

#include <spot/twaalgos/cobuchi.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/powerset.hh>
#include <spot/twaalgos/product.hh>
#include <spot/twaalgos/sccinfo.hh>
#include <spot/twaalgos/copy.hh>
#include <spot/misc/minato.hh>

#include <spot/twa/bddprint.hh>

#include <stack>
#include <sstream>

#define DEBUG 0
#if DEBUG
#define debug std::cerr
#else
#define debug while (0) std::cout
#endif

namespace spot
{
  typedef std::pair<unsigned, unsigned> pair_state_nca;

  twa_graph_ptr
  nba_to_nca(const const_twa_graph_ptr& ref,
             nca_acc_states* nca_acc_states, bool named_states)
  {
    if (!ref->acc().is_buchi())
      throw std::runtime_error(
          "nba_to_nca only works with Büchi automata");

    // Check if state_based.
    bool state_based = (bool)ref->prop_state_acc();

    // Get the power_set construction of the given ref.
    struct power_map pmap;
    const const_twa_graph_ptr pow_ref = tgba_powerset(ref, pmap);

    // Get the product ref. of the given ref. and his power_set cons.
    twa_graph_ptr res = product(ref, pow_ref);
    const product_states* res_map = res->get_named_prop
      <product_states>("product-states");

    // Prepare vector names for purposes of display.
    auto v = new std::vector<std::string>;
    res->set_named_prop("state-names", v);

    // Get the right display of pair_state_nca.
    auto get_st_name =
    [&](const pair_state_nca& x)
    {
      std::stringstream res;
      res << x.first << ",{";

      for (auto& a : pmap.states_of(x.second))
        res << a << ',';

      res.seekp(-1, res.cur);
      res << '}';
      return res.str();
    };

    if (named_states)
    {
      unsigned num_states = res->num_states();
      for (unsigned i = 0; i < num_states; ++i)
        v->emplace_back(get_st_name((*res_map)[i]));
    }

    scc_info si(res);
    unsigned nb_scc = si.scc_count();
    std::set<unsigned> tmp_set;
    for (unsigned scc = 0; scc < nb_scc; ++scc)
    {
      if (si.is_accepting_scc(scc))
        for (unsigned s: si.states_of(scc))
        {
          for (auto& e : res->out(s))
          {
            if ((nca_acc_states && e.acc && si.scc_of(e.dst) == scc)
                || state_based)
              tmp_set.emplace(s);
            e.acc = 0;
          }

          if (nca_acc_states)
            for (unsigned elt : tmp_set)
            {
              const pair_state_nca& st = (*res_map)[elt];
              nca_acc_states->push_back(std::make_pair
                            (st.first, pmap.states_of(st.second)));
            }
          tmp_set.clear();
        }

      else
        for (unsigned s: si.states_of(scc))
          for (auto& e : res->out(s))
            if (si.scc_of(e.dst) == scc || state_based)
              e.acc = 1;
    }

    // FIXME: Set properties of the new automaton ??
    res->set_co_buchi();
    return res;
  }


  namespace
  {
    typedef std::vector<unsigned> state_set_v;

    class det_cobuchi_converter final
    {
      protected:
        const_twa_graph_ptr aut_;
        scc_info si_;
        bdd ap_;
        bdd all_states_;
        std::vector<int> state_to_var_;
        std::map<int, unsigned> var_to_state_;
        nca_acc_states nca_acc_states_;

        bool is_in_nca_acc_states(unsigned st)
        {
          for (auto& elt : nca_acc_states_)
            {
              if (elt.first == st)
                return true;
            }
          return false;
        }

        void allocate_state_vars()
        {
          auto d = aut_->get_dict();

          // One BDD variable per possible output state.
          unsigned ns = aut_->num_states();
          state_to_var_.reserve(ns);
          bdd all_states = bddtrue;
          for (unsigned s = 0; s < ns; ++s)
            {
              if (!si_.reachable_state(s))
                {
                  state_to_var_.push_back(0);
                  continue;
                }
              int v = d->register_anonymous_variables(1, this);
              state_to_var_.push_back(v);
              var_to_state_[v] = s;
              all_states &= bdd_ithvar(v);
            }
          all_states_ = all_states;
        }

        std::map<unsigned, bdd> state_as_bdd_cache_;
        bdd state_as_bdd(unsigned s)
        {
          auto p = state_as_bdd_cache_.emplace(s, bddfalse);
          if (!p.second)
            return p.first->second;

          if ((int)s < 0)
            s = ~s;

          bdd res = bddfalse;
          for (auto& e: aut_->out(s))
              res |= e.cond & bdd_ithvar(state_to_var_[e.dst]);

          p.first->second = res;
          return res;
        }

        void bdd_to_state(bdd b, std::vector<unsigned>& s)
        {
          while (b != bddtrue)
            {
              assert(bdd_low(b) == bddfalse);
              int v = bdd_var(b);
              auto it = var_to_state_.find(v);
              if (it != var_to_state_.end())
                s.push_back(it->second);

              b = bdd_high(b);
            }
        }

        bool has_seen_nca_acc(const std::vector<unsigned>& v,
                              std::vector<unsigned>& st_seen_acc)
        {
          for (const auto& acc_state : nca_acc_states_)
            {
              if (acc_state.second.size() != v.size())
                continue;

              bool st_found = true;
              for (unsigned elt : v)
                {
                  if (acc_state.second.find(elt)
                      == acc_state.second.end())
                  {
                    st_found = false;
                    break;
                  }
                }

              if (!v.empty() && st_found)
                st_seen_acc.push_back(acc_state.first);
            }
          return !st_seen_acc.empty();
        }

        void match_and_tilde(std::vector<unsigned>& s, unsigned state)
        {
          size_t size = s.size();
          for (unsigned i = 0; i < size; ++i)
            if (s[i] == state)
              s[i] = ~state;
        }

        void set_next_state(std::vector<unsigned>& s, bdd b, bdd b_marked,
                            bool has_mark)
        {
          bdd_to_state(b, s);

          std::vector<unsigned> marked;
          if (has_mark)
            bdd_to_state(b_marked, marked);

          std::vector<unsigned> st_seen_acc;
          if (has_seen_nca_acc(s, st_seen_acc))
            for (unsigned buchi_st : st_seen_acc)
              {
                if (has_mark)
                  {
                    for (unsigned m : marked)
                      if (m == buchi_st)
                        match_and_tilde(s, buchi_st);
                  }
                else
                  {
                    match_and_tilde(s, buchi_st);
                  }
              }
        }

        void set_accepting_edges(const std::vector<state_set_v>& s_to_ss,
                                 std::map<state_set_v, unsigned>& ss_to_s,
                                 twa_graph_ptr& res)
        {
          for (const auto& ss : s_to_ss)
            {
              bool has_mark = false;
              for (unsigned se: ss)
                {
                  if ((int)se < 0)
                    {
                      has_mark = true;
                      break;
                    }
                }
              if (!has_mark)
                {
                  for (auto& edge : res->out(ss_to_s[ss]))
                    edge.acc = 1;
                }
            }
        }


      public:
        det_cobuchi_converter(const const_twa_graph_ptr ref)
          : aut_(ref), si_(ref), ap_(ref->ap_vars())
        {
          // Get first's algorithm acceptance set.
          nba_to_nca(aut_, &nca_acc_states_);
        }

        ~det_cobuchi_converter()
        {
          aut_->get_dict()->unregister_all_my_variables(this);
        }

        twa_graph_ptr run(bool named_states)
        {
          // FIXME properties to preserve ??
          // Prepare result automata.
          twa_graph_ptr res = make_twa_graph(aut_->get_dict());
          res->copy_ap_of(aut_);
          res->set_co_buchi();

          // Declare variables representing states.
          allocate_state_vars();

          // Conversion between state-sets and states.
          std::vector<std::vector<unsigned>> s_to_ss;
          std::map<state_set_v, unsigned> ss_to_s;
          std::stack<unsigned> todo;

          // Renname states of resulting automata (for display purposes).
          std::vector<std::string>* state_name = nullptr;
          if (named_states)
            {
              state_name = new std::vector<std::string>();
              res->set_named_prop("state-names", state_name);
            }

          auto new_state = [&](state_set_v& ss) -> unsigned
          {
            auto p = ss_to_s.emplace(ss, 0);
            if (!p.second)
              return p.first->second;

            unsigned s = res->new_state();
            assert(s == s_to_ss.size());
            p.first->second = s;
            s_to_ss.emplace_back(ss);
            todo.emplace(s);

            if (named_states)
              {
                std::ostringstream os;
                bool not_first = false;
                os << '{';
                for (unsigned s: ss)
                  {
                    if (not_first)
                      os << ',';
                    else
                      not_first = true;

                    if ((int)s < 0)
                      {
                        os << '~';
                        s = ~s;
                      }
                    os << s;
                  }
                os << '}';
                state_name->emplace_back(os.str());
              }
            return s;
          };

          state_set_v is;
          is.push_back(aut_->get_init_state_number());
          res->set_init_state(new_state(is));

          state_set_v v;
          while (!todo.empty())
            {
              unsigned s = todo.top();
              todo.pop();

              bdd bs = bddfalse;
              bdd bs_marked = bddfalse;
              bool has_mark = false;
              for (unsigned se: s_to_ss[s])
              {
                // Compute possible paths
                bs = bs | state_as_bdd(se);

                // Check for marked (~)
                if ((int)se < 0)
                {
                  bs_marked = bs_marked | state_as_bdd(se);
                  if (!has_mark)
                    has_mark = true;
                }
              }
#if DEBUG
              debug << "STATE:" << s << " has_mark:" << has_mark;
              debug << "  bs:" << bdd_format_formula(aut_->get_dict(), bs)
                << '\n';
#endif
              // First loop over all possible valuations atomic properties
              bdd all_letters = bddtrue;
              while (all_letters != bddfalse)
                {
                  bdd oneletter = bdd_satoneset(all_letters, ap_, bddfalse);
                  all_letters -= oneletter;
                  bdd bs_letter = bs & oneletter;

                  if (bs_letter == bddfalse)
                    continue;

                  minato_isop isop(bs & oneletter);
                  bdd cube;
                  bdd dest = bddtrue;
                  while ((cube=isop.next()) != bddfalse)
                    dest &= bdd_existcomp(cube, all_states_);

                  bdd dest_marked = bddtrue;
                  if (has_mark)
                  {
                    minato_isop isop_marked(bs_marked & oneletter);
                    while ((cube=isop_marked.next()) != bddfalse)
                      dest_marked &= bdd_existcomp(cube, all_states_);
                  }

                  v.clear();
                  set_next_state(v, dest, dest_marked, has_mark);
#if DEBUG
                  debug << "\tletter:"
                    << bdd_format_formula(aut_->get_dict(), oneletter);
                  debug << "  bs_&_letter:"
                    << bdd_format_formula(aut_->get_dict(), bs & oneletter);
                  debug << "  dest:"
                    << bdd_format_formula(aut_->get_dict(), dest);
                  debug << "  dest_marked:"
                    << bdd_format_formula(aut_->get_dict(), dest_marked)
                    << '\n';
                  debug << "\t\tdest_set:{";
                  for (unsigned elt: v)
                    {
                      if ((int)elt < 0)
                        debug << '~' << ~elt << ',';
                      else
                        debug << elt << ',';
                    }
                  debug << "}\n";
#endif
                  unsigned d = new_state(v);
                  res->new_edge(s, d, oneletter);
                }
            }
          set_accepting_edges(s_to_ss, ss_to_s, res);
          res->merge_edges();
          return res;
        }

    };
  }

  twa_graph_ptr
  nba_to_dca(const const_twa_graph_ptr& aut, bool named_states)
  {
    if (!aut->acc().is_buchi())
      throw std::runtime_error(
          "nba_to_dca only works with Büchi automata");

    det_cobuchi_converter dcc(aut);
    return dcc.run(named_states);
  }
}
