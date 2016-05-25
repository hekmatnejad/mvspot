// -*- coding: utf-8 -*-
// Copyright (C) 2014, 2015, 2016 Laboratoire de Recherche et Développement
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

#pragma once
#include <spot/graph/graph.hh>
#include <spot/twa/twagraph.hh>
#include <spot/tl/formula.hh>
#include <memory>

namespace spot
{
  class SPOT_API twalt
  {
  public:
    typedef digraph<void, twa_graph_edge_data, true> graph_t;
    typedef unsigned state_num;
  protected:
    graph_t g_;
    bdd_dict_ptr dict_;
    unsigned init_number_;

  public:
    twalt(const bdd_dict_ptr& dict)
      : dict_(dict)
      {
      }

    ~twalt()
      {
	dict_->unregister_all_my_variables(this);
      }

    unsigned num_states() const
    {
      return g_.num_states();
    }

    unsigned num_edges() const
    {
      return g_.num_edges();
    }

    void set_init_state(state_num s)
    {
      assert(s < num_states());
      init_number_ = s;
    }

    state_num get_init_state_number() const
    {
      return init_number_;
    }

    unsigned new_state()
    {
      return g_.new_state();
    }

    unsigned new_states(unsigned n)
    {
      return g_.new_states(n);
    }

    unsigned new_edge(unsigned src, unsigned dst,
                            bdd cond, acc_cond::mark_t acc = 0U)
    {
      return g_.new_edge(src, dst, cond, acc);
    }

    unsigned new_edge(unsigned src, unsigned* dst_list_start,
                      unsigned* dst_list_end,
                      bdd cond, acc_cond::mark_t acc = 0U)
    {
      return g_.new_edge(src, dst_list_start, dst_list_end, cond, acc);
    }

    unsigned new_edge(unsigned src,
                      std::initializer_list<unsigned> dst_list,
                      bdd cond, acc_cond::mark_t acc = 0U)
    {
      return g_.new_edge(src, std::begin(dst_list), std::end(dst_list),
                         cond, acc);
    }

    int register_ap(formula ap)
    {
      int res = dict_->has_registered_proposition(ap, this);
      if (res < 0)
        {
          //aps_.push_back(ap);
          res = dict_->register_proposition(ap, this);
          //bddaps_ &= bdd_ithvar(res);
        }
      return res;
    }

    int register_ap(std::string ap)
    {
      return register_ap(formula::ap(ap));
    }
  };

  typedef std::shared_ptr<twalt> twalt_ptr;
  typedef std::shared_ptr<const twalt> const_twalt_ptr;

  inline twalt_ptr make_twalt(const bdd_dict_ptr& dict)
  {
    return std::make_shared<twalt>(dict);
  }
}
