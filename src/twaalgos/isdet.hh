// -*- coding: utf-8 -*-
// Copyright (C) 2012, 2013, 2014, 2015 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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

#include "twa/twagraph.hh"

namespace spot
{
  /// \addtogroup twa_misc
  /// @{

  /// \brief Count the number of non-deterministic states in \a aut.
  ///
  /// The automaton is deterministic if it has 0 nondeterministic states,
  /// but it is more efficient to call is_deterministic() if you do not
  /// care about the number of nondeterministic states.
  SPOT_API unsigned
  count_nondet_states(const const_twa_graph_ptr& aut);

  /// \brief Return true iff \a aut is deterministic.
  ///
  /// This function is more efficient than count_nondet_states() when
  /// the automaton is nondeterministic, because it can return before
  /// the entire automaton has been explored.
  SPOT_API bool
  is_deterministic(const const_twa_graph_ptr& aut);

  /// \brief Return true iff \a aut is complete.
  ///
  /// An automaton is complete if its translation relation is total,
  /// i.e., each state as a successor for any possible configuration.
  SPOT_API bool
  is_complete(const const_twa_graph_ptr& aut);

  /// @}
}