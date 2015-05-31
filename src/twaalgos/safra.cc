// -*- coding: utf-8 -*-
// Copyright (C) 2015 Laboratoire de Recherche et
// Développement de l'Epita.
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

#include <algorithm>
#include <deque>
#include <utility>
#include <unordered_map>

#include "safra.hh"
#include "twaalgos/degen.hh"

namespace spot
{
  namespace
  {
    using power_set = std::map<std::vector<safra_state>, int>;
    const char* const sub[10] =
    {
      "\u2080",
      "\u2081",
      "\u2082",
      "\u2083",
      "\u2084",
      "\u2085",
      "\u2086",
      "\u2087",
      "\u2088",
      "\u2089",
    };

    std::string subscript(unsigned start)
    {
      std::string res;
      do
        {
          res = sub[start % 10] + res;
          start /= 10;
        }
      while (start);
      return res;
    }

    struct acc_pool
    {
      acc_pool(unsigned num_set)
      : max(num_set)
      { }

      unsigned count()
      {
        return next_;
      }

      acc_cond::mark_t get_acc(unsigned set, unsigned acc_num)
      {
        if (acc_num >= max[set])
          {
            for (unsigned i = max[set]; i <= acc_num; ++i)
              {
                map_[std::make_pair(set, i)] = next_++;
              }
            max[set] = acc_num + 1;
          }
        acc_cond::mark_t res = 1 << map_[std::make_pair(set, acc_num)];
        return res;
      }

      unsigned next_ = 0;
      std::vector<unsigned> max;
      // maps a (set, acc_num) to a unique id.
      std::map<std::pair<unsigned, unsigned>, unsigned> map_;
    };

    acc_cond::acc_code gen_parity(acc_pool& pool, unsigned num_sets)
    {
      acc_cond::acc_code res = acc_cond::acc_code::t();
      for (unsigned i = 0; i < num_sets; ++i)
        {
          acc_cond::acc_code tmp = acc_cond::acc_code::f();
          unsigned odd = -1U;
          for (auto it = pool.map_.rbegin(); it != pool.map_.rend(); ++it)
            {
              auto& p = *it;
              if (p.first.first == i)
                {
                  if (p.first.second % 2 == 0)
                    {
                      // Only emit red if a grenn comes after
                      if (odd != -1U)
                        tmp.append_and(res.fin({odd}));
                      tmp.append_or(res.inf({p.second}));
                      odd = -1U;
                    }
                  else
                    odd = p.second;
                }
            }
          res.append_and(tmp);
        }
      return res;
    }

    void print(unsigned start, const safra_state::nodes_t& nodes,
               std::ostringstream& os, std::vector<unsigned>& idx)
    {
      std::string s = subscript(start);
      os << '[' << s;
      std::vector<unsigned> new_idx;
      std::vector<unsigned> todo;
      unsigned next = -1U;
      bool first = true;
      for (auto& i: idx)
        {
          auto it = std::lower_bound(nodes.at(i).cbegin(), nodes.at(i).cend(),
                                     start + 1);
          if (it == nodes.at(i).cend())
            {
              if (first)
                {
                  os << i;
                  first = false;
                }
              else
                os << ' ' << i;
            }
          else if (*it == (start + 1))
            new_idx.push_back(i);
          else
            {
              todo.push_back(i);
              next = std::min(next, *it);
            }
        }
      if (!new_idx.empty())
        print(start + 1, nodes, os, new_idx);
      if (next != -1U)
        {
          std::vector<unsigned> todo2;
          std::vector<unsigned> todo_next;
          unsigned new_next = -1U;
          while (!todo.empty())
            {
              for (auto& i: todo)
                {
                  auto it = std::lower_bound(nodes.at(i).cbegin(),
                                             nodes.at(i).cend(), next);
                  if (*it == next)
                    todo_next.push_back(i);
                  else
                    {
                      todo2.push_back(i);
                      next = std::min(new_next, *it);
                    }
                }
              print(next, nodes, os, todo_next);

              next = new_next;
              new_next = -1;
              todo = todo2;
              todo2.clear();
              todo_next.clear();
            }
        }
      os << s << ']';
    }

    // Returns true if lhs has a smaller nesting pattern than rhs
    // If lhs and rhs are the same, return false.
    bool nesting_cmp(const std::vector<node_helper::brace_t>& lhs,
                     const std::vector<node_helper::brace_t>& rhs)
    {
      size_t m = std::min(lhs.size(), rhs.size());
      size_t i = 0;
      for (; i < m; ++i)
        {
          if (lhs[i] != rhs[i])
            return lhs[i] < rhs[i];
        }
      return lhs.size() > rhs.size();
    }

    // Used to remove all acceptance whos value is above max_acc
    // Returns the minimum amount of acc_sets needed
    unsigned remove_dead_acc(twa_graph_ptr& aut, acc_pool& pool)
    {
      unsigned res = 0;
      unsigned m = 0;
      for (unsigned i = 0; i < pool.max.size(); ++i)
        {
          unsigned max = 0;
          unsigned to_rem = 0;
          for (auto p : pool.map_)
            {
              if (p.first.first == i)
                {
                  if (max < p.first.second)
                    {
                      max = p.first.second;
                      to_rem = p.second;
                    }
                }
            }
          // Remove Red with too high a value
          if (max % 2 == 1)
            {
              std::cerr << "Removing: " << to_rem << std::endl;
              unsigned mask = ~(1 << to_rem);
              for (auto& t: aut->transitions())
                {
                  t.acc &= mask;
                }
            }
          if (res < to_rem)
            {
              res = to_rem;
              m = max;
            }
        }
      return m % 2 ? res : res + 1;
    }

    std::vector<std::string>*
    print_debug(std::map<std::vector<safra_state>, int>& states)
    {
      std::vector<std::string>* res = nullptr;
      res = new std::vector<std::string>(states.size());
      std::vector<unsigned> idx;
      for (auto& p: states)
        {
          std::ostringstream os;
          // TODO multi printer
          for (auto& state: p.first)
          {
            for (auto& n: state.nodes_)
              idx.push_back(n.first);
            print(0, state.nodes_, os, idx);
            os << '\n';
            (*res)[p.second] = os.str();
            idx.clear();
          }
        }
      return res;
    }

  }
  auto
  safra_state::compute_succs(const const_twa_graph_ptr& aut,
                             const std::vector<unsigned>& bddnums,
                             std::unordered_map<bdd,
                                               std::pair<unsigned, unsigned>,
                                               bdd_hash>& deltas,
                             unsigned acc_num) const -> succs_t
  {
    succs_t res;
    // Given a bdd returns index of associated safra_state in res
    std::map<unsigned, unsigned> bdd2num;
    for (auto& node: nodes_)
      {
        for (auto& t: aut->out(node.first))
          {
            auto p = deltas[t.cond];
            for (unsigned j = p.first; j < p.second; ++j)
              {
                auto i = bdd2num.insert(std::make_pair(bddnums[j], res.size()));
                unsigned idx;
                if (!i.second)
                  idx = i.first->second;
                else
                  {
                    // Each new node starts out with same number of nodes as src
                    idx = res.size();
                    res.emplace_back(safra_state(nb_braces_.size()),
                                     bddnums[j]);
                  }
                safra_state& ss = res[idx].first;
                acc_cond::mark_t mask = 1 << acc_num;
                ss.update_succ(node.second, t.dst, t.acc & mask);
                assert(ss.nb_braces_.size() == ss.is_green_.size());
              }
          }
      }
    for (auto& s: res)
      {
        safra_state& tmp = s.first;
        s.first.color_ = tmp.finalize_construction();
      }
    return res;
  }

  unsigned safra_state::finalize_construction()
  {
    unsigned red = -1U;
    unsigned green = -1U;
    std::vector<unsigned> rem_succ_of;
    assert(is_green_.size() == nb_braces_.size());
    for (unsigned i = 0; i < is_green_.size(); ++i)
      {
        if (nb_braces_[i] == 0)
          {
            // It is impossible to emit red == -1 as those transitions would
            // lead us in a sink states which are not created here.
            assert(i >= 1);
            red = std::min(red, 2 * i - 1);
            // Step A3: Brackets that do not contain any nodes emit red
            is_green_[i] = false;
          }
        else if (is_green_[i])
          {
            green = std::min(green, 2 * i);
            // Step A4 Emit green
            rem_succ_of.emplace_back(i);
          }
      }
    for (auto& n: nodes_)
      {
        // Step A4 Remove all brackets inside each green pair
        node_helper::truncate_braces(n.second, rem_succ_of, nb_braces_);
      }

    // Step A5 define the rem variable
    std::vector<unsigned> decr_by(nb_braces_.size());
    unsigned decr = 0;
    for (unsigned i = 0; i < nb_braces_.size(); ++i)
      {
        // Step A5 renumber braces
        nb_braces_[i - decr] = nb_braces_[i];
        if (nb_braces_[i] == 0)
          {
            ++decr;
          }
        // Step A5, renumber braces
        decr_by[i] = decr;
      }
    nb_braces_.resize(nb_braces_.size() - decr);
    for (auto& n: nodes_)
      {
        node_helper::renumber(n.second, decr_by);
      }
    return std::min(red, green);
  }

  void node_helper::renumber(std::vector<brace_t>& braces,
                             const std::vector<unsigned>& decr_by)
  {
    for (unsigned idx = 0; idx < braces.size(); ++idx)
      {
        braces[idx] -= decr_by[braces[idx]];
      }
  }

  void
  node_helper::truncate_braces(std::vector<brace_t>& braces,
                                const std::vector<unsigned>& rem_succ_of,
                                std::vector<size_t>& nb_braces)
  {
    for (unsigned idx = 0; idx < braces.size(); ++idx)
      {
        bool found = false;
        // find first brace that matches rem_succ_of
        for (auto s: rem_succ_of)
          {
            found |= braces[idx] == s;
          }
        if (found)
          {
            assert(idx < braces.size() - 1);
            // For each deleted brace, decrement elements of nb_braces
            // This corresponds to A4 step
            for (unsigned i = idx + 1; i < braces.size(); ++i)
              {
                --nb_braces[braces[i]];
              }
            braces.resize(idx + 1);
            break;
          }
      }
  }

  void safra_state::update_succ(const std::vector<node_helper::brace_t>& braces,
                                unsigned dst, const acc_cond::mark_t acc)
  {
    std::vector<node_helper::brace_t> copy = braces;
    // TODO handle multiple accepting sets
    if (acc)
      {
        // Accepting transition generate new braces: step A1
        copy.emplace_back(nb_braces_.size());
        // nb_braces_ gets updated later so put 0 for now
        nb_braces_.emplace_back(0);
        // Newly created braces cannot emit green as they won't have
        // any braces inside them (by construction)
        is_green_.push_back(false);
      }
    auto i = nodes_.emplace(dst, copy);
    if (!i.second)
      {
        // Step A2: Only keep the smallest nesting pattern (i-e  braces_) for
        // identical nodes.  Nesting_cmp returnes true if copy is smaller
        if (nesting_cmp(copy, i.first->second))
          {
            // Remove brace count of replaced node
            for (auto b: i.first->second)
              --nb_braces_[b];
            i.first->second = std::move(copy);
          }
        else
          // Node already exists and has bigger nesting pattern value
          return;
      }
    // After inserting new node, update the brace count per node
    for (auto b: i.first->second)
      ++nb_braces_[b];
    // Step A4: For a brace to emit green it must surround other braces.
    // Hence, the last brace cannot emit green as it is the most inside brace.
    is_green_[i.first->second.back()] = false;
  }

  // Called only to initialize first state
  safra_state::safra_state(unsigned val, bool init_state)
  {
    if (init_state)
      {
        unsigned state_num = val;
        // One brace set
        is_green_.push_back(true);
        // First brace has init_state hence one state inside the first braces.
        nb_braces_.push_back(1);
        std::vector<node_helper::brace_t> braces = { 0 };
        nodes_.emplace(state_num, std::move(braces));
      }
    else
      {
        unsigned nb_braces = val;
        // One brace set
        is_green_.assign(nb_braces, true);
        // First brace has init_state hence one state inside the first braces.
        nb_braces_.assign(nb_braces, 0);
      }
  }

  bool
  safra_state::operator<(const safra_state& other) const
  {
    return nodes_ < other.nodes_;
  }

  twa_graph_ptr
  tgba_determinisation(const const_twa_graph_ptr& a, bool pretty_print)
  {
    // Degeneralize
    const_twa_graph_ptr aut;
    //if (a->acc().is_generalized_buchi())
    //  aut = spot::degeneralize_tba(a);
    //else
      aut = a;


    bdd allap = bddtrue;
    {
      typedef std::set<bdd, bdd_less_than> sup_map;
      sup_map sup;
      // Record occurrences of all guards
      for (auto& t: aut->transitions())
        sup.emplace(t.cond);
      for (auto& i: sup)
        allap &= bdd_support(i);
    }

    // Preprocessing
    // Used to convert atomic bdd to id
    std::unordered_map<bdd, unsigned, bdd_hash> bdd2num;
    std::vector<bdd> num2bdd;
    // Nedded for compute succs
    // Used to convert large bdd to indexes
    std::unordered_map<bdd, std::pair<unsigned, unsigned>, bdd_hash> deltas;
    std::vector<unsigned> bddnums;
    for (auto& t: aut->transitions())
      {
        auto it = deltas.find(t.cond);
        if (it == deltas.end())
          {
            bdd all = t.cond;
            unsigned prev = bddnums.size();
            while (all != bddfalse)
              {
                bdd one = bdd_satoneset(all, allap, bddfalse);
                all -= one;
                auto p = bdd2num.emplace(one, num2bdd.size());
                if (p.second)
                  num2bdd.push_back(one);
                bddnums.emplace_back(p.first->second);
              }
            deltas[t.cond] = std::make_pair(prev, bddnums.size());
          }
      }

    auto res = make_twa_graph(aut->get_dict());
    res->copy_ap_of(aut);
    res->prop_copy(aut,
                   { false, // state based
                   true, // inherently_weak
                   false, // deterministic
                   true // stutter inv
                   });

    unsigned num_sets = std::max(1U, aut->acc().num_sets());
    // Given a safra_state get its associated state in output automata.
    // Required to create new transitions from 2 safra-state
    power_set seen;
    safra_state init(aut->get_init_state_number(), true);
    unsigned num = res->new_state();
    res->set_init_state(num);
    seen.insert(std::make_pair(std::vector<safra_state>(num_sets, init), num));
    std::deque<std::vector<safra_state>> todo;
    todo.push_back(std::vector<safra_state>(num_sets, init));
    unsigned sets = 0;
    using succs_t = safra_state::succs_t;
    std::vector<succs_t> v_succs(num_sets);
    acc_pool pool(num_sets);
    while (!todo.empty())
      {
        auto curr = todo.front();
        unsigned src_num = seen.find(curr)->second;
        todo.pop_front();
        for (unsigned i = 0; i < num_sets; ++i)
          v_succs[i] = curr[i].compute_succs(aut, bddnums, deltas, i);
        for (unsigned i = 0; i < v_succs[0].size(); ++i)
          {
            std::vector<safra_state> s;
            unsigned bdd_id = v_succs[0][i].second;
            for (unsigned j = 0; j < v_succs.size(); ++j)
              {
                // TODO maybe better to pass by reference
                s.push_back(v_succs[j][i].first);
                assert(bdd_id == v_succs[j][i].second);
              }
            auto succ = seen.find(s);
            unsigned dst_num;
            if (succ != seen.end())
              {
                dst_num = succ->second;
              }
            else
              {
                dst_num = res->new_state();
                // once again passing a copy ...
                todo.push_back(s);
                seen.insert(std::make_pair(s, dst_num));
              }

            acc_cond::mark_t acc = 0;
            for (unsigned k = 0; k < s.size(); ++k)
              {
                if (s[k].color_ != -1U)
                    acc |= pool.get_acc(k, s[k].color_);
              }
            res->new_transition(src_num, dst_num, num2bdd[bdd_id], acc);
            // We only care about green acc
            if (acc % 2 == 0)
              sets = std::max(s[0].color_ + 1, sets);
          }
      }
    unsigned num_acc = remove_dead_acc(res, pool);
    res->set_acceptance(num_acc, gen_parity(pool, num_sets));
    res->prop_deterministic(true);
    res->prop_state_based_acc(false);
    if (pretty_print)
      res->set_named_prop("state-names", print_debug(seen));
    return res;
  }
}
