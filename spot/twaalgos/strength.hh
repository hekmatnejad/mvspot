// -*- coding: utf-8 -*-
// Copyright (C) 2010, 2011, 2013, 2014, 2015 Laboratoire de Recherche
// et Développement de l'Epita (LRDE)
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

#include <spot/twaalgos/sccinfo.hh>

namespace spot
{
  /// \brief Whether an automaton is terminal.
  ///
  /// An automaton is terminal if it is weak, and all accepting SCCs
  /// are complete.
  ///
  /// \param aut the automaton to check
  ///
  /// \param sm an scc_info object for the automaton if available (it
  /// will be built otherwise).
  SPOT_API bool
  is_terminal_automaton(const const_twa_graph_ptr& aut, scc_info* sm = nullptr);


  /// \brief Whether an automaton is weak.
  ///
  /// An automaton is weak if in any given SCC, all transitions belong
  /// to the same acceptance sets.
  ///
  /// \param aut the automaton to check
  ///
  /// \param sm an scc_info object for the automaton if available (it
  /// will be built otherwise).
  SPOT_API bool
  is_weak_automaton(const const_twa_graph_ptr& aut, scc_info* sm = nullptr);

  /// \brief Whether an automaton is inherently weak.
  ///
  /// An automaton is inherently weak if in any given SCC, there
  /// are only accepting cycles, or only rejecting cycles.
  ///
  /// \param aut the automaton to check
  ///
  /// \param sm an scc_info object for the automaton if available (it
  /// will be built otherwise).
  SPOT_API bool
  is_inherently_weak_automaton(const const_twa_graph_ptr& aut,
                               scc_info* sm = nullptr);

  /// \brief Whether a minimized WDBA represents a safety property.
  ///
  /// A minimized WDBA (as returned by a successful run of
  /// minimize_obligation()) represent safety property if it contains
  /// only accepting transitions.
  ///
  /// \param aut the automaton to check
  SPOT_API bool
  is_safety_mwdba(const const_twa_graph_ptr& aut);

  /// \brief Whether an automaton is weak or terminal.
  ///
  /// This sets the "weak" and "terminal" property as appropriate.
  ///
  /// \param aut the automaton to check
  ///
  /// \param sm an scc_info object for the automaton if available (it
  /// will be built otherwise).
  SPOT_API void
  check_strength(const twa_graph_ptr& aut, scc_info* sm = nullptr);


  /// \brief Extract a sub-automaton of a given strength
  ///
  /// The string \a keep should be a non-empty combination of
  /// the following letters:
  /// - 'w': keep only inherently weak SCCs (i.e., SCCs in which
  ///        all transitions belong to the same acceptance sets) that
  ///        are not terminal.
  /// - 't': keep terminal SCCs (i.e., inherently weak SCCs that are complete)
  /// - 's': keep strong SCCs (i.e., SCCs that are not inherently weak).
  ///
  /// This algorithm returns a subautomaton that contains all SCCs of the
  /// requested strength, plus any upstream SCC (but adjusted not to be
  /// accepting).
  ///
  /// The definition are basically those used in the following paper,
  /// except that we extra the "inherently weak" part instead of the
  /// weak part because we can now test for inherent weakness
  /// efficiently enough (not enumerating all cycles as suggested in
  /// the paper).
  /** \verbatim
      @inproceedings{renault.13.tacas,
        author = {Etienne Renault and Alexandre Duret-Lutz and Fabrice
                  Kordon and Denis Poitrenaud},
        title = {Strength-Based Decomposition of the Property {B\"u}chi
                  Automaton for Faster Model Checking},
        booktitle = {Proceedings of the 19th International Conference on Tools
                  and Algorithms for the Construction and Analysis of Systems
                  (TACAS'13)},
        editor = {Nir Piterman and Scott A. Smolka},
        year = {2013},
        month = mar,
        pages = {580--593},
        publisher = {Springer},
        series = {Lecture Notes in Computer Science},
        volume = {7795},
        doi = {10.1007/978-3-642-36742-7_42}
      }
      \endverbatim */
  ///
  /// \param aut the automaton to decompose
  /// \param keep a string specifying the strengths to keep: it should
  SPOT_API twa_graph_ptr
  decompose_strength(const const_twa_graph_ptr& aut, const char* keep);
}
