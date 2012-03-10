// Copyright (C) 2011 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE)
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


#ifndef SPOT_MISC_ACCCOMPL_HH_
# define SPOT_MISC_ACCCOMPL_HH_

#include <bdd.h>
#include "misc/hash.hh"
#include "misc/bddlt.hh"

namespace spot
{
  /// \brief Help class to convert a bdd of an automaton into
  /// its complement.
  /// This is used when you need to complement all the acceptance
  /// conditions in an automaton. For example in the simulation.
  class AccCompl
  {
    public:
      AccCompl(bdd all)
        : all_(all)
      {
      }


      bdd complement(const bdd acc);

    protected:
      bdd all_;
      typedef Sgi::hash_map<bdd, bdd, bdd_hash> bdd_cache_t;
      bdd_cache_t cache_;
  };
} // End namespace Spot

#endif // !SPOT_MISC_ACCCOMPL_HH_
