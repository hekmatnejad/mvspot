// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement de
// l'Epita.
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

#ifndef SPOT_GRAPH_NGRAPH_HH
# define SPOT_GRAPH_NGRAPH_HH

#include <unordered_map>
#include <vector>
#include "graph.hh"

namespace spot
{
  template <typename Graph,
	    typename State_Name,
	    typename Name_Hash = std::hash<State_Name>,
	    typename Name_Equal = std::equal_to<State_Name>>
  class SPOT_API named_graph
  {
  protected:
    Graph& g_;
  public:

    typedef typename Graph::state state;
    typedef typename Graph::transition transition;
    typedef State_Name name;

    typedef std::unordered_map<name, state,
			       Name_Hash, Name_Equal> name_to_state_t;
    name_to_state_t name_to_state;
    typedef std::vector<name> state_to_name_t;
    state_to_name_t state_to_name;

    named_graph(Graph& g)
      : g_(g)
    {
    }

    Graph& graph()
    {
      return g_;
    }

    Graph& graph() const
    {
      return g_;
    }

    template <typename... Args>
    state new_state(name n, Args&&... args)
    {
      auto p = name_to_state.emplace(n, 0U);
      if (p.second)
	{
	  unsigned s = g_.new_state(std::forward<Args>(args)...);
	  p.first->second = s;
	  if (state_to_name.size() < s + 1)
	    state_to_name.resize(s + 1);
	  state_to_name[s] = n;
	  return s;
	}
      return p.first->second;
    }

    /// \brief Give an alternate name to a state.
    /// \return true iff the newname was already used.
    bool alias_state(state s, name newname)
    {
      return !name_to_state.emplace(newname, s).second;
    }

    state get_state(name n) const
    {
      return name_to_state.at(n);
    }

    name get_name(state s) const
    {
      return state_to_name.at(s);
    }

    bool has_state(name n) const
    {
      return name_to_state.find(n) != name_to_state.end();
    }

    state_to_name_t& names()
    {
      return state_to_name;
    }

    template <typename... Args>
    transition
    new_transition(name src, name dst, Args&&... args)
    {
      return g_.new_transition(get_state(src), get_state(dst),
			       std::forward<Args>(args)...);
    }

    template <typename... Args>
    transition
    new_transition(name src,
		   const std::vector<State_Name>& dst, Args&&... args)
    {
      std::vector<State_Name> d;
      d.reserve(dst.size());
      for (auto n: dst)
	d.push_back(get_state(n));
      return g_.new_transition(get_state(src), d, std::forward<Args>(args)...);
    }

    template <typename... Args>
    transition
    new_transition(name src,
		   const std::initializer_list<State_Name>& dst, Args&&... args)
    {
      std::vector<state> d;
      d.reserve(dst.size());
      for (auto n: dst)
	d.push_back(get_state(n));
      return g_.new_transition(get_state(src), d, std::forward<Args>(args)...);
    }
  };

}

#endif // SPOT_GRAPH_NGRAPH_HH