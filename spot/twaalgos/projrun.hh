// -*- coding: utf-8 -*-
// Copyright (C) 2013, 2014 Laboratoire de Recherche et Developpement
// de l'Epita (LRDE).
// Copyright (C) 2004  Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
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
#include <iosfwd>
#include <spot/twa/fwd.hh>
#include <spot/twaalgos/emptiness.hh>

namespace spot
{
  struct twa_run;

  /// \ingroup twa_run
  /// \brief Project a twa_run on a tgba.
  ///
  /// If a twa_run has been generated on a product, or any other
  /// on-the-fly algorithm with tgba operands,
  ///
  /// \param run the run to replay
  /// \param a_run the automata on which the run was generated
  /// \param a_proj the automata on which to project the run
  /// \return true iff the run could be completed
  SPOT_API twa_run_ptr
  project_twa_run(const const_twa_ptr& a_run,
		   const const_twa_ptr& a_proj,
		   const const_twa_run_ptr& run);
}