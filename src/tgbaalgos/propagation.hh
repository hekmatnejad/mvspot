// Copyright (C) 2011 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
// Copyright (C) 2004, 2005  Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
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

#ifndef SPOT_TGBAALGOS_PROPAGATION_HH
# define SPOT_TGBAALGOS_PROPAGATION_HH

#include "tgba/tgba.hh"

namespace spot
{
  /// \brief propagate acceptance conditions through the automata.
  /// If all output arcs of a state possess the same acceptance condition
  /// then it can be put on all input arcs.
  ///
  /// \param a the automata to reduce.
  /// \return the propagated automata
  const tgba* propagate_acceptance_conditions (const tgba* a);
  void propagate_acceptance_conditions_inplace (tgba* a);
}

#endif /// SPOT_TGBAALGOS_PROPAGATION_HH
