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

#pragma once

#include <spot/misc/common.hh>
#include <spot/twa/fwd.hh>

namespace spot
{
  /// \ingroup twa_algorithms
  /// \brief Parity kind type
  enum parity_kind
  {
    parity_kind_max,      /// The new acceptance will be a parity max
    parity_kind_min,      /// The new acceptance will be a parity min
    parity_kind_same,     /// The new acceptance will keep the kind
    parity_kind_any       /// The new acceptance may change the kind
  };

  /// \brief Parity  style type
  enum parity_style
  {
    parity_style_odd,      /// The new acceptance will be a parity odd
    parity_style_even,     /// The new acceptance will be a parity even
    parity_style_same,     /// The new acceptance will keep the style
    parity_style_any       /// The new acceptance may change the style
  };

  /// \brief Change the parity acceptance of an automaton
  ///
  /// The parity acceptance condition of an automaton is characterized by
  ///    - The priority kind of the acceptance (min or max).
  ///    - The parity style, i.e. parity of the sets seen infinitely often
  //       (odd or even).
  ///    - The number of acceptance sets.
  ///
  /// The output will be an equivalent automaton with the new parity acceptance.
  /// The automaton must have a parity acceptance. The number of acceptance sets
  /// may be increased by one. Every transition will belong to at most one
  /// acceptance set.
  ///
  /// The parity kind is defined only if the number of acceptance sets
  /// is strictly greater than 1. The parity_style is defined only if the number
  /// of acceptance sets is non-zero. Some values of kind and style may result
  /// in equivalent outputs if the number of acceptance sets of the input
  /// automaton is not great enough.
  ///
  /// \param aut the input automaton
  ///
  /// \param kind the parity kind of the output automaton
  ///
  /// \param style the parity style of the output automaton
  ///
  /// \return the automaton with the new acceptance
  SPOT_API twa_graph_ptr
  change_parity(const const_twa_graph_ptr& aut,
                parity_kind kind, parity_style style);

  /// \brief Remove useless acceptance sets
  ///
  /// If two sets with the same parity are separated by unused sets, then these
  /// two sets are merged.
  ///
  /// \param aut the input automaton
  ///
  /// \param keep_style whether the style of the parity acc is kept.
  ///
  /// \return the automaton without useless acceptance sets.
  /// @{
  SPOT_API twa_graph_ptr
  cleanup_parity_acceptance(const const_twa_graph_ptr& aut,
                            bool keep_style = false);

  SPOT_API twa_graph_ptr
  cleanup_parity_acceptance_here(twa_graph_ptr aut, bool keep_style = false);
  /// @}

  /// \brief Colorize automaton
  ///
  /// \param aut the input automaton
  ///
  /// \param keep_style whether the style of the parity acc is kept.
  ///
  /// \return the colorized automaton
  /// @{
  SPOT_API twa_graph_ptr
  colorize_parity(const const_twa_graph_ptr& aut, bool keep_style = false);

  SPOT_API twa_graph_ptr
  colorize_parity_here(twa_graph_ptr aut, bool keep_style = false);
  /// @}

  /// \brief Construct a product keeping the parity of two parity automata
  ///
  /// \param left the first automaton
  ///
  /// \param right the second automaton
  ///
  /// \result the product, which is a parity automaton
  SPOT_API twa_graph_ptr
  parity_product(const const_twa_graph_ptr& left,
                 const const_twa_graph_ptr& right);
}
