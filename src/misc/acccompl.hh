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
#include "tgbaalgos/reachiter.hh"
#include "tgba/tgbaexplicit.hh"

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


  class AccComplAutomaton:
      public tgba_reachable_iterator_depth_first
  {
    public:
      AccComplAutomaton(tgba* a)
        : tgba_reachable_iterator_depth_first(a),
          size(0),
          ea_(down_cast<tgba_explicit*> (a)),
          ac_(ea_->all_acceptance_conditions())
      {
      }

      void process_link(const state* in_s,
                        int in,
                        const state* out_s,
                        int out,
                        const tgba_succ_iterator* si)
      {
        (void) in_s;
        (void) in;
        (void) out;
        (void) out_s;

        const tgba_explicit_succ_iterator* tmpit =
          down_cast<const tgba_explicit_succ_iterator*> (si);

        tgba_explicit::transition* t = ea_->get_transition(tmpit);

        t->acceptance_conditions
          = ac_.complement(t->acceptance_conditions);
      }

      void process_state(const state*, int, tgba_succ_iterator*)
      {
        ++size;
      }

    public:
      size_t size;

    private:
      tgba_explicit* ea_;
      AccCompl ac_;
  };

} // End namespace Spot

#endif // !SPOT_MISC_ACCCOMPL_HH_
