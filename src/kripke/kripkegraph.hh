// -*- coding: utf-8 -*-
// Copyright (C) 2011, 2012, 2013, 2014, 2015 Laboratoire de Recherche
// et Développement de l'Epita (LRDE)
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

#include <iosfwd>
#include "kripke.hh"
#include "graph/graph.hh"
#include "tl/formula.hh"

namespace spot
{
  /// \brief Concrete class for kripke_graph states.
  struct SPOT_API kripke_graph_state: public spot::state
  {
  public:
    kripke_graph_state(bdd cond = bddfalse) noexcept
      : cond_(cond)
    {
    }

    virtual ~kripke_graph_state() noexcept
    {
    }

    virtual int compare(const spot::state* other) const
    {
      auto o = down_cast<const kripke_graph_state*>(other);
      assert(o);

      // Do not simply return "other - this", it might not fit in an int.
      if (o < this)
	return -1;
      if (o > this)
	return 1;
      return 0;
    }

    virtual size_t hash() const
    {
      return
	reinterpret_cast<const char*>(this) - static_cast<const char*>(nullptr);
    }

    virtual kripke_graph_state*
    clone() const
    {
      return const_cast<kripke_graph_state*>(this);
    }

    virtual void destroy() const
    {
    }

    bdd cond() const
    {
      return cond_;
    }

    void cond(bdd c)
    {
      cond_ = c;
    }

  private:
    bdd cond_;
  };

  template<class Graph>
  class SPOT_API kripke_graph_succ_iterator final: public kripke_succ_iterator
  {
  private:
    typedef typename Graph::edge edge;
    typedef typename Graph::state_data_t state;
    const Graph* g_;
    edge t_;
    edge p_;
  public:
    kripke_graph_succ_iterator(const Graph* g,
			       const typename Graph::state_storage_t* s):
      kripke_succ_iterator(s->cond()),
      g_(g),
      t_(s->succ)
    {
    }

    ~kripke_graph_succ_iterator()
    {
    }

    virtual void recycle(const typename Graph::state_storage_t* s)
    {
      cond_ = s->cond();
      t_ = s->succ;
    }

    virtual bool first()
    {
      p_ = t_;
      return p_;
    }

    virtual bool next()
    {
      p_ = g_->edge_storage(p_).next_succ;
      return p_;
    }

    virtual bool done() const
    {
      return !p_;
    }

    virtual kripke_graph_state* dst() const
    {
      assert(!done());
      return const_cast<kripke_graph_state*>
	(&g_->state_data(g_->edge_storage(p_).dst));
    }
  };


  /// \class kripke_graph
  /// \brief Kripke Structure.
  class SPOT_API kripke_graph : public kripke
  {
  public:
    typedef digraph<kripke_graph_state, void> graph_t;
    typedef graph_t::edge_storage_t edge_storage_t;
  protected:
    graph_t g_;
    mutable unsigned init_number_;
  public:
    kripke_graph(const bdd_dict_ptr& d)
      : kripke(d), init_number_(0)
    {
    }

    virtual ~kripke_graph()
    {
      get_dict()->unregister_all_my_variables(this);
    }

    unsigned num_states() const
    {
      return g_.num_states();
    }

    unsigned num_edges() const
    {
      return g_.num_edges();
    }

    void set_init_state(graph_t::state s)
    {
      assert(s < num_states());
      init_number_ = s;
    }

    graph_t::state get_init_state_number() const
    {
      if (num_states() == 0)
	const_cast<graph_t&>(g_).new_state();
      return init_number_;
    }

    // FIXME: The return type ought to be const.
    virtual kripke_graph_state* get_init_state() const
    {
      if (num_states() == 0)
	const_cast<graph_t&>(g_).new_state();
      return const_cast<kripke_graph_state*>(state_from_number(init_number_));
    }

    /// \brief Allow to get an iterator on the state we passed in
    /// parameter.
    virtual kripke_graph_succ_iterator<graph_t>*
    succ_iter(const spot::state* st) const
    {
      auto s = down_cast<const typename graph_t::state_storage_t*>(st);
      assert(s);
      assert(!s->succ || g_.valid_trans(s->succ));

      if (this->iter_cache_)
	{
	  auto it =
	    down_cast<kripke_graph_succ_iterator<graph_t>*>(this->iter_cache_);
	  it->recycle(s);
	  this->iter_cache_ = nullptr;
	  return it;
	}
      return new kripke_graph_succ_iterator<graph_t>(&g_, s);

    }

    graph_t::state
    state_number(const state* st) const
    {
      auto s = down_cast<const typename graph_t::state_storage_t*>(st);
      assert(s);
      return s - &g_.state_storage(0);
    }

    const kripke_graph_state*
    state_from_number(graph_t::state n) const
    {
      return &g_.state_data(n);
    }

    kripke_graph_state*
    state_from_number(graph_t::state n)
    {
      return &g_.state_data(n);
    }

    std::string format_state(unsigned n) const
    {
      std::stringstream ss;
      ss << n;
      return ss.str();
    }

    virtual std::string format_state(const state* st) const
    {
      return format_state(state_number(st));
    }

    /// \brief Get the condition on the state
    virtual bdd state_condition(const state* s) const
    {
      return down_cast<const kripke_graph_state*>(s)->cond();
    }

    edge_storage_t& edge_storage(unsigned t)
    {
      return g_.edge_storage(t);
    }

    const edge_storage_t edge_storage(unsigned t) const
    {
      return g_.edge_storage(t);
    }

    unsigned new_state(bdd cond)
    {
      return g_.new_state(cond);
    }

    unsigned new_states(unsigned n, bdd cond)
    {
      return g_.new_states(n, cond);
    }

    unsigned new_edge(unsigned src, unsigned dst)
    {
      return g_.new_edge(src, dst);
    }
  };

  typedef std::shared_ptr<kripke_graph> kripke_graph_ptr;

  inline kripke_graph_ptr
  make_kripke_graph(const bdd_dict_ptr& d)
  {
    return std::make_shared<kripke_graph>(d);
  }
}