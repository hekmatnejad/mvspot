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

#include "acccompl.hh"

namespace spot
{

  // The algorithm of this method is simple. We explain this in
  // bottom/up.
  // Internally, the bdd is represented like a tree. There is two
  // output: high(1) and low (0). An acceptance condition is like
  // the following:
  // if all has three acceptance conditions, all is equal to
  // "a!b!c + !ab!c + !a!bc".
  // So, to compute the negation of an acceptance condition, say "a!b!c"
  // we need to know wich one is go to one when true. So we are looping
  // through the conditions until bdd_high is true.
  // Once found, we keep only it.
  bdd AccCompl::complement(const bdd acc)
  {
    bdd_cache_t::const_iterator it = cache_.find(acc);
    if (it != cache_.end())
      return it->second;

    bdd res = bddtrue;

    bdd n = all_ - acc;

    // This means, if acc == all, the opposite is bddfalse, and not
    // bddtrue.
    if (n == bddfalse)
      res = bddfalse;

    while (n != bddfalse)
    {
      bdd cond = bdd_satone(n);
      bdd oldcond = cond;
      n -= cond;

      while (bdd_high(cond) == bddfalse)
        cond = bdd_low(cond);
      // Here we want to keep only the current one.
      // tmp is only useful to keep the value and cache it.
      bdd tmp = bdd_ithvar(bdd_var(cond));
      res &= tmp;
      cache_[oldcond] = tmp;
    }

    cache_[acc] = res;

    return res;
  }




} // End namespace spot.
