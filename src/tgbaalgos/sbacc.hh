// -*- coding: utf-8 -*-
// Copyright (C) 2015 Laboratoire de Recherche et Développement
// de l'Epita.
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

#ifndef SPOT_TGBAALGOS_SBACC_HH
# define SPOT_TGBAALGOS_SBACC_HH

#include "tgba/tgbagraph.hh"

namespace spot
{
  /// \brief Transform an automaton to use state-based acceptance
  ///
  /// This is independent on the acceptance condition used.
  SPOT_API tgba_digraph_ptr sbacc(tgba_digraph_ptr& aut);
}

#endif // SPOT_TGBAALGOS_COMPLETE_HH