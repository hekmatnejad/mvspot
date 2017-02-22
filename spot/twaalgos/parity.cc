// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et Développement
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

#include <spot/twaalgos/parity.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/copy.hh>
#include <spot/twaalgos/product.hh>
#include <vector>
#include <utility>
#include <functional>
#include <queue>

namespace spot
{
  namespace
  {
    unsigned change_set(unsigned x, const unsigned num_sets,
                        const bool change_kind, const bool change_style)
    {
      if (change_kind)
        {
          // If the parity acceptance kind is changed,
          // then the index of the sets are reversed
          x = num_sets - x - 1;
        }
      if (change_style)
        {
          // If the parity style is changed, then all the existing acceptance
          // sets are shifted
          ++x;
        }
      return x;
    }

    void
    change_acc(twa_graph_ptr& aut, unsigned num_sets, const bool change_kind,
               const bool change_style, const bool output_max,
               const bool input_max)
    {
      for (auto& e: aut->edge_vector())
        if (e.acc)
          {
            unsigned msb = 0U;
            if (input_max)
              msb = e.acc.max_set() - 1;
            else
              for (auto i = 0U; i < num_sets; ++i)
                if (e.acc.has(i))
                  {
                    msb = i;
                    break;
                  }
            e.acc = acc_cond::mark_t();
            e.acc.set(change_set(msb, num_sets, change_kind, change_style));
          }
        else if (output_max && change_style)
          {
            // If the parity is changed, a new set is introduced.
            // A parity max acceptance will mark the transitions which do not
            // belong to any set with this new set.
            // A parity min acceptance will introduce a unused acceptance set.
            e.acc.set(0);
          }
    }
  }

  twa_graph_ptr
  change_parity(const const_twa_graph_ptr& aut,
                parity_kind kind, parity_style style)
  {
    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd, true))
      throw new std::invalid_argument("change_parity_acceptance: The first"
                                      "argument aut must have a parity "
                                      "acceptance.");
    auto result = copy(aut, twa::prop_set::all());
    auto old_num_sets = result->num_sets();

    bool output_max = true;
    switch (kind)
      {
        case parity_kind_max:
          output_max = true;
          break;
        case parity_kind_min:
          output_max = false;
          break;
        case parity_kind_same:
          output_max = current_max;
          break;
        case parity_kind_any:
          // If we need to change the style we may change the kind not to
          // introduce new accset.
          output_max = (((style == parity_style_odd && !current_odd)
                         || (style == parity_style_even && current_odd))
                        && old_num_sets % 2 == 0) != current_max;
      }

    bool change_kind = current_max != output_max;
    bool toggle_style = change_kind && (old_num_sets % 2 == 0);

    bool output_odd = true;
    switch (style)
      {
        case parity_style_odd:
          output_odd = true;
          break;
        case parity_style_even:
          output_odd = false;
          break;
        case parity_style_same:
          output_odd = current_odd;
          break;
        case parity_style_any:
          output_odd = current_odd != toggle_style;
          // If we need to change the kind we may change the style not to
          // introduce new accset.
          break;
      }

    current_odd = current_odd != toggle_style;
    bool change_style = false;
    auto num_sets = old_num_sets;
    // If the parity neeeds to be changed, then a new acceptance set is created.
    // The old acceptance sets are shifted
    if (output_odd != current_odd)
      {
        change_style = true;
        ++num_sets;
      }

    if (change_kind || change_style)
      {
        auto new_acc = acc_cond::acc_code::parity(output_max,
                                                  output_odd, num_sets);
        result->set_acceptance(num_sets, new_acc);
      }
    change_acc(result, old_num_sets, change_kind,
               change_style, output_max, current_max);
    return result;
  }

  twa_graph_ptr
  cleanup_parity_acceptance(const const_twa_graph_ptr& aut, bool keep_style)
  {
    auto result = copy(aut, twa::prop_set::all());
    return cleanup_parity_acceptance_here(result, keep_style);
  }

  twa_graph_ptr
  cleanup_parity_acceptance_here(twa_graph_ptr aut, bool keep_style)
  {
    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd, true))
      throw new std::invalid_argument("colorize_parity: The first argument aut "
                                      "must have a parity acceptance.");
    auto used_in_aut = acc_cond::mark_t();
    // Compute all the used sets
    if (aut->num_sets() > 0)
      {
        for (auto& t: aut->edges())
          {
            if (current_max)
              {
                auto maxset = t.acc.max_set();
                if (maxset)
                  {
                    t.acc = acc_cond::mark_t();
                    t.acc.set(maxset - 1);
                  }
              }
            else
              t.acc = t.acc.lowest();
            used_in_aut |= t.acc;
          }
      }
    if (used_in_aut)
    {
      // Never remove the least significant acceptance set, and mark the
      // acceptance set 0 to keep the style if needed.
      if (aut->num_sets() > 0)
        {
          if (current_max || keep_style)
            used_in_aut.set(0);
          if (!current_max)
            used_in_aut.set(aut->num_sets() - 1);
        }

      // Fill the vector shift with the new acceptance sets
      std::vector<unsigned> shift(aut->acc().num_sets());
      int prev_used = -1;
      bool change_style = false;
      unsigned new_index = 0;
      for (auto i = 0U; i < shift.size(); ++i)
        if (used_in_aut.has(i))
          {
            if (prev_used == -1)
              change_style = i % 2 != 0;
            else if ((i + prev_used) % 2 != 0)
              ++new_index;
            shift[i] = new_index;
            prev_used = i;
          }

      // Update all the transitions with the vector shift
      for (auto& t: aut->edges())
        {
          auto maxset = t.acc.max_set();
          if (maxset)
            {
              t.acc = acc_cond::mark_t();
              t.acc.set(shift[maxset - 1]);
            }
        }
      auto new_num_sets = new_index + 1;
      if (new_num_sets < aut->num_sets())
        {
          auto new_acc = acc_cond::acc_code::parity(current_max,
                                                    current_odd != change_style,
                                                    new_num_sets);
          aut->set_acceptance(new_num_sets, new_acc);
        }
    }
    else if (aut->num_sets() > 0U)
      {
        if ((current_max && current_odd)
           || (!current_max && current_odd != (aut->num_sets() % 2 == 0)))
          aut->set_acceptance(0, acc_cond::acc_code::t());
        else
          aut->set_acceptance(0, acc_cond::acc_code::f());
      }
    return aut;
  }

  twa_graph_ptr
  colorize_parity(const const_twa_graph_ptr& aut, bool keep_style)
  {
    return colorize_parity_here(copy(aut, twa::prop_set::all()), keep_style);
  }

  twa_graph_ptr
  colorize_parity_here(twa_graph_ptr aut, bool keep_style)
  {
    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd, true))
      throw new std::invalid_argument("colorize_parity: The first argument aut "
                                      "must have a parity acceptance.");

    bool has_empty = false;
    for (const auto& e: aut->edges())
      if (!e.acc)
        {
          has_empty = true;
          break;
        }
    auto num_sets = aut->num_sets();
    unsigned incr = 0U;
    if (has_empty)
      {
        // If the automaton has a transition that belong to any set, we need to
        // introduce a new acceptance set.
        if (keep_style && current_max)
          {
            // We may want to add a second acceptance set to keep the style of
            // the parity acceptance
            incr = 2;
          }
        else
          incr = 1;
        num_sets += incr;
        bool new_style = current_odd == (keep_style || !current_max);
        auto new_acc = acc_cond::acc_code::parity(current_max,
                                                  new_style, num_sets);
        aut->set_acceptance(num_sets, new_acc);
      }
    if (current_max)
      for (auto& e: aut->edges())
        {
          auto maxset = e.acc.max_set();
          e.acc = acc_cond::mark_t();
          if (maxset == 0)
            e.acc.set(incr - 1);
          else
            e.acc.set(maxset + incr - 1);
        }
    else
      for (auto& e: aut->edges())
        {
          if (e.acc)
            e.acc = e.acc.lowest();
          else
            {
              e.acc = acc_cond::mark_t();
              e.acc.set(num_sets - incr);
            }
        }
    return aut;
  }
}
