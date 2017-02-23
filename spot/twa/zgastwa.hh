// -*- coding: utf-8 -*-
// Copyright (C) 2014-2017 Laboratoire de Recherche et Développement de l'Epita.
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

#include <spot/twa/twa.hh>
#include <spot/kripke/kripke.hh>

// from TChecker via libtchecker.la
#include <semantics/zg.hh>

namespace spot
{

  struct SPOT_API zg_as_twa_state: public spot::kripke_graph_state,
                                   public semantics::zg::state_t
  {
  public:
    zg_as_twa_state(bdd cond = bddfalse)
      : spot::kripke_graph_state(cond), syntax::state_t(),
        _zone(nullptr)
    {
    }

    ~zg_as_twa_state()
    {
      delete _zone();
    }

    virtual int compare(const spot::state* other) const override
    {
      // CHECK IT
      auto o = down_dast<const zg_as_twa_state*>(other);

      SPOT_ASSERT(_zone != nullptr);
      SPOT_ASSERT(o->_zone != nullptr);

      if (this->syntax::state_t::operator==(*o) &&
          (*_zone == *(s->_zone)))
        return 0;
      else if (o < this)
        return -1;
      else
        return 1;
    }

    virtual size_t hash() const override
    {
      // CHECK IT
      SPOT_ASSERT(_zone != nullptr);
      size_t seed = this->syntax::state::hash();
      boost::hash_combine(seed, _zone->hash());
      return seed;
    }

    virtual zg_as_twa_state* clone() const override
    {
      // FIXME ?
      return const_cast<zg_as_twa_state*>(this);
    }

    virtual void destroy() const override
    {
    }
  };

  template <class OPERATIONAL_ZONE>
  class SPOT_API zg_as_twa_succ_iterator: public kripke_succ_iterator
  {
  private:
    semantics::zg::zg_t <OPERATIONAL_ZONE> zone_graph_;
    zg_as_twa_state src_state_;
    edge_range_t edge_list_;

  public:
    virtual zg_as_twa_succ_iterator(syntax::model_t& model,
                                    zg_as_twa_state& state,
                                    const bdd& cond)
      : kripke_succ_iterator(cond),
        zone_graph_(model), src_state_(state)
    {
      edge_list_ = zone_graph_.outgoing_edges(src_state_);
    }

    virtual zg_as_twa_succ_iterator(semantics::zg::zg_t<OPERATIONAL_ZONE>& z_g,
                                    zg_as_twa_state& state, const bdd& cond)
      : kripke_succ_iterator(cond), zone_graph_(z_g), src_state_(state)
    {
      edge_list_ = zone_graph_.outgoing_edges(src_state_);
    }

    virtual ~twa_succ_iterator() override
    {
    }

    virtual bool first() override
    {
      // CHECK IT Need to include TChecker
      edge_list_ = zg_t.outgoing_edges(src_state_);
      return edge_list_;
    }

    virtual bool next() override
    {
      // FIXME Need to include TChecker
      /*
        In TChecker, there is a test to verify if the next status
        is valid or not, to skip it during exploring.
        Maybe it's just for the DST or maybe it's necessary ?
        CHECK IT
      */
      return edge_list_.advance();
    }

    virtual bool done() const override
    {
      // CHECK IT
      return edge_list_.at_end();
    }

    virtual const zg_as_twa_state* dst() const override
    {
      // CHECK IT
      SPOT_ASSERT(!done());
      auto edge = edge_list_.current();
      return zone_graph_.next(src_state_, edge);
    }
  };

  class SPOT_API zg_as_twa: public kripke
  {
  private:
   semantics::zg::zg_t <OPERATIONAL_ZONE> zone_graph_;
   zg_as_twa_state init_state_;

  public:
    zg_as_twa(const bdd_dict_ptr& d)
      : kripke(d)
    {
    }

    virtual const zg_as_twa_state* get_init_state() const override
    {
      // CHECK IT
      if (!init_state_)
        zone_graph_.initial(init_state_);
      return new zg_as_twa_state(init_state_);
    }

    virtual zg_as_twa_succ_iterator*
    succ_iter(const state* st) const override
    {
      // CHECK IT
      return new zg_as_twa_succ_iterator(zone_graph_, *st);
    }

    virtual bdd state_condition(const state* s) const override
    {
      // CHECK IT
      auto zg_s = down_cast<const zg_as_twa_state*>(s);
      return zg_s->cond();
    }

    virtual std::string format_state(const state* s) const override
    {
      // CHECK IT
      std::stringstream ss;
      auto zg_s = down_cast<const zg_as_twa_state*>(s);
      zg_s->output(ss);
      return ss.str();
    }
  };
}
