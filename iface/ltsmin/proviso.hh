// -*- coding: utf-8 -*-
// Copyright (C)  2015 Laboratoire de Recherche et
// Developpement de l'Epita (LRDE)
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

#include <string>
#include <vector>
#include <random>
#include <unordered_map>
#include "twa/twa.hh"
#include "misc/common.hh"
#include "dfs_inspector.hh"

namespace spot
{
  /// \brief This class represent a proviso.
  class SPOT_API proviso
  {
  public:
    /// \brief This method is called for every closing edge but sometimes
    /// we cannot ditinguish between closing and forward edge: in this
    /// the method is also called.
    /// Return the position in the DFS stack of the state to expand.
    /// Note that according to the cycle proviso, every cycle must
    /// contain an expanded state. If  the method return -1, no expansion
    /// is needed.
    virtual int maybe_closingedge(const state* src,
				  const state* dst,
				  const dfs_inspector& i) = 0;

    /// \brief Notify the proviso that a a new state has been pushed. This
    /// method return true only if the src has been expanded.
    virtual bool notify_push(const state* src, const dfs_inspector& i) = 0;
    virtual bool before_pop(const state* src, const dfs_inspector& i) = 0;

    virtual std::string name() = 0;
    virtual std::string dump() = 0;
    virtual std::string dump_csv() = 0;
    virtual ~proviso()
    { }
  };


  /// \brief Implementation of the source/destination family of
  /// provisos.
  template<bool BasicCheck, bool Delayed>
  class SPOT_API src_dst_provisos: public proviso
  {
  public:
    enum class strategy
    {
      None,			// Nothing is expanded (incorrect regarding MC)
      All,			// All states are expanded (i.e. no proviso)
      Source,			// Source is expanded
      Destination,		// Destination is expanded
      Random,			// Choose randomly between source and dest.
      MinEnMinusRed,		// Choose minimal overhead in transitions
      MinNewStates		// Choose minimal overhead in new states
    };

    src_dst_provisos(strategy strat): strat_(strat), generator_(0),
      source_(0), destination_(0)
    { }

    virtual std::string name()
    {
      std::string res = BasicCheck? "basiccheck_" : "";
      if (Delayed)
	res = "delayed_" + res;

      switch (strat_)
	{
	case strategy::None:
	  return res +  "none";
	case strategy::All:
	  return res +  "all";
	case strategy::Source:
	  return res +  "source";
	case strategy::Destination:
	  return res +  "destination";
	case strategy::Random:
	  return res +  "random";
	case strategy::MinEnMinusRed:
	  return res +  "min_en_minus_red";
	case strategy::MinNewStates:
	  return res +  "min_new_states";
	default:
	  assert(false);
	  break;
	};
      return res;
    }

    virtual int maybe_closingedge(const state* src,
				       const state* dst,
				       const dfs_inspector& i)
    {
      int src_pos = i.dfs_position(src);
      int dst_pos = i.dfs_position(dst);
      assert(src_pos != -1);

      // Delayed is activated,  we must update colors.
      if (Delayed)
	{
	  // The state is not on the DFS and Delayed is activated,
	  // we must propagate the dangerousness of successors
	  if (dst_pos == -1)
	    {
	      i.get_colors(src)[2] =
		i.get_colors(src)[2]  && i.get_colors(dst)[2];

	      // Nothing else todo in this case.
	      return -1;
	    }
	}

      // State not on the DFS
      if (dst_pos == -1)
	return -1;

      bool src_expanded = i.get_iterator(src_pos)->all_enabled();
      bool dst_expanded = i.get_iterator(dst_pos)->all_enabled();

      if (BasicCheck && (src_expanded || dst_expanded))
	return -1;

      return choose(src_pos, dst_pos, src, dst, i);
    }

    virtual bool notify_push(const state* src, const dfs_inspector& i)
    {
      int src_pos = i.dfs_position(src);
      if (strat_ == strategy::All)
	{
	  i.get_iterator(src_pos)->fire_all();
	  return true;
	}
      if (Delayed)
	{
	  //vector of bool : d, e, g.
	  auto& colors = i.get_colors(src);
	  auto* it = i.get_iterator(src_pos);
	  colors.push_back(it->enabled() != it->reduced());
	  colors.push_back(false);
	  colors.push_back(true);
	}
      return false;
    }
    virtual bool before_pop(const state* st,
			    const dfs_inspector& i)
    {
      // Some work must be done when delayed is used.
      if (Delayed)
	{

	  int st_pos =  i.dfs_position(st);
	  auto& colors = i.get_colors(st);
	  if (colors[1] && !colors[2])
	    {
	      i.get_iterator(st_pos)->fire_all();
	      colors[1] = false;
	      colors[2] = true;
	      return false;
	    }
	  else
	    {
	      // Here since we will return true, we know that
	      // we will pop state from DFS, so we can already
	      // update colors of predecessor if some exits.
	      if (st_pos != 0) // state is not the initial one
		{
		  const state* newtop = i.dfs_state(st_pos-1);
		  auto& colors_newtop = i.get_colors(newtop);
		  colors_newtop[2] = colors_newtop[2] && colors[2];
		}
	      return true;
	    }
	}

      // If Delayed is not activated , ther is nothing to do.
      return true;
    }

    virtual std::string dump()
    {
      return
	" source_expanded  : " + std::to_string(source_)           + '\n' +
	" dest_expanded    : " + std::to_string(destination_)      + '\n';
    }
    virtual std::string dump_csv()
    {
      return
	std::to_string(source_)           + ',' +
	std::to_string(destination_);
    }

  private:
    int choose(int src, int dst,
	       const state* src_st, const state* dst_st,
	       const dfs_inspector& i)
    {
      switch (strat_)
	{
	case strategy::None:
	  return -1;
	case strategy::All:
	  return -1; // already expanded.
	case strategy::Source:
	  ++source_;
	  update_delayed(src_st, src_st, i);
	  return src;
	case strategy::Destination:
	  ++destination_;
	  update_delayed(src_st, dst_st, i);
	  return dst;
	case strategy::Random:
	  if (generator_()%2)
	    {
	      ++destination_;
	      update_delayed(src_st, dst_st, i);
	      return dst;
	    }
	  ++source_;
	  update_delayed(src_st, src_st, i);
	  return src;
	case strategy::MinEnMinusRed:
	  {
	    unsigned enminred_src =
	      i.get_iterator(src)->enabled() - i.get_iterator(src)->reduced();
	    unsigned enminred_dst =
	      i.get_iterator(dst)->enabled() - i.get_iterator(dst)->reduced();
	    if (enminred_src < enminred_dst)
	      {
		++source_;
		update_delayed(src_st, src_st, i);
		return src;
	      }
	    ++destination_;
	    update_delayed(src_st, dst_st, i);
	    return dst;
	  }
	case strategy::MinNewStates:
	  {
	    // FIXME The use of static is an ugly hack but here we cannot
	    //  use lamdba capture [&] in expand_will_generate.
	    // I see two ways to resolve this hack:
	    //  - use std::function but it's too costly.
	    //  - replace expand_will_generate by a method ignored that
	    //    return an (auto)-iterator over all ignored states.
	    static unsigned new_src;
	    static unsigned new_dst;
	    static const dfs_inspector* i_ptr;
	    new_src = 0;
	    new_dst = 0;
	    i_ptr = &i;

	    i.get_iterator(src)->
	      expand_will_generate([](const state* s)
	    			{
				  if (i_ptr->visited(s))
	    			     ++new_src;
	    			});
	    i.get_iterator(dst)->
	      expand_will_generate([](const state* s)
	    			{
	    			  if (i_ptr->visited(s))
	    			    ++new_dst;
	    			});
	    if (new_src < new_dst)
	      {
		++source_;
		update_delayed(src_st, src_st, i);
		return src;
	      }
	    ++destination_;
	    update_delayed(src_st, dst_st, i);
	    return dst;
	  }
	default:
	  assert(false);
	  break;
	};
      return -1;
    }

    void update_delayed(const state* dfstop,
			const state* chosen,
			const dfs_inspector& i)
    {
      if (Delayed)
	{
	  i.get_colors(chosen)[1] = true;
	  i.get_colors(dfstop)[2] = false;
	}
    }

    strategy strat_;
    std::mt19937 generator_;
    unsigned source_;
    unsigned destination_;
  };


  /// \brief Implementation of the evangelista family of
  /// provisos.
  template<bool FullyColored>
  class SPOT_API evangelista10sttt: public proviso
  {

  public:
    evangelista10sttt()
      { }

    virtual int maybe_closingedge(const state* src,
				  const state* dst,
				  const dfs_inspector& i)
    {
      auto& src_colors = i.get_colors(src);
      auto& dst_colors = i.get_colors(dst);
      bool src_is_orange = src_colors[0] && !src_colors[1];
      bool dst_is_red = dst_colors[0] && dst_colors[1];

      // src is orange and dst is red  an expansion is required.
      if (src_is_orange && dst_is_red)
	{
	  ++expanded_;
	  src_colors[0] = false;
	  src_colors[1] = false;

	  int src_pos = i.dfs_position(src);

	  if (FullyColored)
	    {
	      // We can propagate the green color. Note that
	      // here the source is not yet expanded but has
	      // already the good color so we can propagate!
	      int p = src_pos-1;
	      while (0 <= p)
		{
		  const state* st = i.dfs_state(p);
		  auto& colors = i.get_colors(st);
		  bool is_orange = colors[0] && !colors[1];

		  if (is_orange && i.get_iterator(p)->done())
		    {
		      // Propagate green
		      colors[0] = false;
		      colors[1] = false;
		      --p;
		    }
		  else
		    break;
		}
	    }

	  // Require to expand the source of the edge.
	  ++source_;
	  return src_pos;
	}

      // This maybe closing-edge is safe.
      return -1;
    }

    virtual bool notify_push(const state* src, const dfs_inspector& i)
    {
      // Every state is associated to a color ORANGE, RED, and GREEN
      // We need two boolean to encode this information.
      // 10 -> ORANGE
      // 11 -> RED
      // 00 -> GREEN
      auto& colors = i.get_colors(src);
      colors.push_back(true);
      colors.push_back(false);

      // Actually Sami's algorithm do not have a weight but an expanded
      // field foreach state. We use the weight of the generic dfs to
      // store this information.
      i.get_weight(src) = expanded_;

      int src_pos = i.dfs_position(src);
      bool res = false;
      if (!is_c2cl(src, i))
	{
	  i.get_iterator(src_pos)->fire_all();
	  ++source_;
	  res = true;
	}

      // Set state to green
      if (i.get_iterator(src_pos)->all_enabled())
	{
	  ++expanded_;
	  colors[0] = false;
	  colors[1] = false; // Useless since this bit must already be false.
	}
      return res;
    }

    virtual bool before_pop(const state* src, const dfs_inspector& i)
    {
      int src_pos = i.dfs_position(src);
      if (i.get_iterator(src_pos)->all_enabled())
	--expanded_;

      auto& colors = i.get_colors(src);
      if (colors[0] && !colors[1]) // State is orange.
	{
	  auto* it = i.get_iterator(src_pos);

	  it->first();
	  bool isred = false;
	  while (!it->done())
	    {
	      auto* dst = it->current_state();
	      auto& dst_colors = i.get_colors(dst);
	      bool dst_is_green = !dst_colors[0] && !dst_colors[1];

	      if (!dst_is_green) //dst is not green
		isred = true;
	      dst->destroy();
	      it->next();
	    }
	  if (isred) // set color to red
	    {
	      colors[0] = true;
	      colors[0] = true;
	    }
	  else // set color to green
	    {
	      colors[0] = false;
	      colors[0] = false;
	    }
	}

      // Never avoid a POP in Sami Evangelista and Pajault
      return true;
    }

    virtual std::string name()
    {
      if (FullyColored)
	return "fullycolored_evangelista10sttt";
      else
	return "evangelista10sttt";
    }
    virtual std::string dump()
    {
      return
	" source_expanded  : " + std::to_string(source_)           + '\n' +
	" dest_expanded    : " + std::to_string(destination_)      + '\n';
    }
    virtual std::string dump_csv()
    {
      return
	std::to_string(source_)           + ',' +
	std::to_string(destination_);
    }

    virtual ~evangelista10sttt()
    { }


  private:

    // In the algorithm of Evangelista, the procedure c2cl check
    // for every new state if an expansion is required according
    // to its reducet set of successors.
    bool is_c2cl(const state* st, const dfs_inspector& i)
    {
      // FIXME find another way to not use std::function
      static const dfs_inspector* i_ptr;
      static bool res;
      static int st_weight;

      // Initalize
      i_ptr = &i;
      res = true;
      st_weight = i.get_weight(st);

      // Grab the position of st.
      int st_pos = i.dfs_position(st);
      assert(st_pos != -1);

      // Here reorder remaining will walk remaining transitions
      // Since is_c2cl is only call during the PUSH remaining will
      // consist of the Reduced set.
      i.get_iterator(st_pos)->
	reorder_remaining([](const state* sp)
			  {
			    if (i_ptr->visited(sp))
			      {
				auto& colors = i_ptr->get_colors(sp);
				bool sp_is_red = colors[0] && colors[1];
				bool sp_is_orange = colors[0] && !colors[1];
				if (sp_is_red ||
				    (sp_is_orange &&
				     st_weight == i_ptr->get_weight(sp)))
				  res = false;
			      }

			    // do not reorder.
			    return false;
			  });
      return res;
    }

    int expanded_ = 0;
    unsigned source_ = 0;
    unsigned destination_ = 0; ///< stay to zero but to have homogeneous csv.
  };
}