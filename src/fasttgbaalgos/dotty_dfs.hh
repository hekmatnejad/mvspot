// Copyright (C) 2012 Laboratoire de Recherche et Développement
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

#ifndef SPOT_FASTTGBAALGOS_DOTTY_DFS_HH
# define SPOT_FASTTGBAALGOS_DOTTY_DFS_HH

#include <stack>
#include "misc/hash.hh"
#include "fasttgba/fasttgba.hh"
#include "fasttgba/faststate.hh"

namespace spot
{
  class dotty_dfs
  {
  public:
    dotty_dfs(const fasttgba* a);

    virtual ~dotty_dfs();

    void run();

    virtual bool want_state(const faststate* s) const;

    virtual void start();

    virtual void end();

    virtual void add_state(const faststate* s);

    virtual const faststate* next_state();

    virtual void  process_state(const faststate* s, int n,
				fasttgba_succ_iterator* si);

    virtual void  process_link(const faststate* in_s, int in,
			       const faststate* out_s, int out,
			       const fasttgba_succ_iterator* si);


  protected:
    const fasttgba* a_;
    std::stack<const faststate*> todo; ///< A stack of states yet to explore.
    typedef Sgi::hash_map<const faststate*, int,
			  faststate_ptr_hash, faststate_ptr_equal> seen_map;
    seen_map seen;
  };
}

#endif // SPOT_FASTTGBAALGOS_DOTTY_DFS_HH