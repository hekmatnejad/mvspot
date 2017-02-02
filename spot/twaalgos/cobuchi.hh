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

#pragma once

#include <spot/misc/common.hh>
#include <spot/twa/fwd.hh>

#include <set>
#include <vector>

namespace spot
{
  typedef std::vector<std::pair<unsigned, std::set<unsigned>>> nca_acc_states;

  /// \brief Translates a Nondet Büchi Aut. to a Nondet. co-Büchi Aut.
  ///
  /// This function implements the augmented subset construction algorithm
  /// described on
  /// page 249 (Theorem 3.5) of:
  /** \verbatim
      @Article{boker.2009.lcs,
        author =  {Udi Boker and Orna Kupferman},
        title =   {Co-ing Büchi Made Tight and Useful},
        journal = {Logic In Computer Science},
        year =    {2009},
        volume =  {24},
        pages =   {245--254},
        month =   {August}
      }
      \endverbatim */
  /// It should be noted that this implementation is quite different from the
  /// algorithm. In the above paper, the acceptance condition of co-Büchi
  /// Automata indicates states that are not wanted to be seen finitely often.
  /// This is changed as Spot's acceptance condition of co-Büchi Aut considers
  /// states to visit finitely often
  ///
  /// If \a nm is supplied it will be filled with pairs (b, E) of accepting
  /// states (following the papers definition of acceptance condition) where
  /// b ∈ B (the set of NBA states) and E ⊂ 2^B.
  ///
  /// \a named_states name each state for easier debugging.
  SPOT_API twa_graph_ptr
  nba_to_nca(const const_twa_graph_ptr& aut, nca_acc_states* nm = nullptr,
             bool named_states = false);

  /// \brief Translates a Nondet. Büchi Aut. to a Det. co-Büchi Aut.
  ///
  /// This function implements the breakpoint construction algorithm described
  /// on page 252 (Theorem 4.1) of:
  /** \verbatim
      @Article{boker.2009.lcs,
        author =  {Udi Boker and Orna Kupferman},
        title =   {Co-ing Büchi Made Tight and Useful},
        journal = {Logic In Computer Science},
        year =    {2009},
        volume =  {24},
        pages =   {245--254},
        month =   {August}
      }
      \endverbatim */
  ///
  /// \a named_states name each state for easier debugging.
  SPOT_API twa_graph_ptr
  nba_to_dca(const const_twa_graph_ptr& aut, bool named_states = false);
}
