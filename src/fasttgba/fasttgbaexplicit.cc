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

#include <sstream>
#include "fasttgbaexplicit.hh"

namespace spot
{
  // ----------------------------------------------------------------------
  // fast_explicit_state code here
  // ----------------------------------------------------------------------

  void
  fast_explicit_state::destroy() const
  {

  }

  fast_explicit_state::fast_explicit_state(int label):
    label_(label)
  {
  }

  int
  fast_explicit_state::compare(const faststate* other) const
  {
    return label_ - ((const fast_explicit_state*)other)->label_;
  }

  size_t
  fast_explicit_state::hash() const
  {
    return label_;
  }

  faststate*
  fast_explicit_state::clone() const
  {
    return const_cast<fast_explicit_state *>(this);
  }

  void*
  fast_explicit_state::external_information() const
  {
    assert(false);
  }

  int
  fast_explicit_state::label() const
  {
    return label_;
  }

  void
  fast_explicit_state::add_successor(const struct transition t)
  {
    successors.insert(successors.end(), t);
  }

  // ----------------------------------------------------------------------
  // fast_explicit_iterator code here
  // ----------------------------------------------------------------------

  fast_explicit_iterator::fast_explicit_iterator
  (const fast_explicit_state* state):
    start_(state)
  {
  }

  fast_explicit_iterator::~fast_explicit_iterator()
  {
  }

  void
  fast_explicit_iterator::first()
  {
    it_ = start_->successors.begin();
  }

  void
  fast_explicit_iterator::next()
  {
    ++it_;
  }

  bool
  fast_explicit_iterator::done() const
  {
    return it_ == start_->successors.end();
  }

  faststate*
  fast_explicit_iterator::current_state() const
  {
    assert(!done());
    return const_cast<fast_explicit_state*>(it_->dst);
  }

  cube
  fast_explicit_iterator::current_condition() const
  {
    return it_->conditions;
  }

  markset
  fast_explicit_iterator::current_acceptance_conditions() const
  {
    return it_->acceptance_conditions;
  }

  // ----------------------------------------------------------------------
  // fasttgbaexplicit code here
  // ----------------------------------------------------------------------

  fasttgbaexplicit::fasttgbaexplicit(std::vector<std::string> aps,
				     std::vector<std::string> acc):
    all_cond_ (acc.size()),
    all_cond_neg_ (acc.size()),
    aps_(aps),
    acc_(acc),
    init_(0)
  {
    num_acc_ = acc.size();

    // Allocate the bitset and fix all_cond to avoid
    // multiple computations
    all_cond_neg_ = ~all_cond_;
  }

  fasttgbaexplicit::~fasttgbaexplicit()
  {
    // Delete all states
    sm::iterator i = state_map_.begin();

    while (i != state_map_.end())
      {
	i->second->destroy();
	++i;
      }
  }

  faststate*
  fasttgbaexplicit::get_init_state() const
  {
    return init_->clone();
  }

  fasttgba_succ_iterator*
  fasttgbaexplicit::succ_iter(const faststate* state) const
  {
    const fast_explicit_state* s =
      down_cast<const fast_explicit_state*>(state);
    assert(s);

    return new fast_explicit_iterator(s);
  }

  std::vector<std::string>
  fasttgbaexplicit::get_dict() const
  {
    return aps_;
  }

  std::string
  fasttgbaexplicit::format_state(const faststate* s) const
  {
    std::ostringstream oss;
    oss << down_cast<const fast_explicit_state*> (s)->label();
    return oss.str();
  }


  std::string
  fasttgbaexplicit::transition_annotation(const fasttgba_succ_iterator* t) const
  {
    std::ostringstream oss;
    oss << t->current_condition().dump(aps_);

    if (!t->current_acceptance_conditions().empty())
      oss << " \\nAcc { " << t->current_acceptance_conditions().dump(acc_)
	  << "}";
    return oss.str();
  }

  faststate*
  fasttgbaexplicit::project_state(const faststate* ,
				  const fasttgba*) const
  {
    assert(false);
  }

  markset
  fasttgbaexplicit::all_acceptance_conditions() const
  {
    return all_cond_;
  }

  unsigned int
  fasttgbaexplicit::number_of_acceptance_conditions() const
  {
    return num_acc_;
  }

  markset
  fasttgbaexplicit::neg_acceptance_conditions() const
  {
    return all_cond_neg_;
  }

  fast_explicit_state*
  fasttgbaexplicit::add_state(int s)
  {
    sm::iterator available = state_map_.find(s);
    if (available == state_map_.end())
      {
	fast_explicit_state *the_state = new fast_explicit_state(s);
	state_map_.insert(std::make_pair (s, the_state));

	// Initial state not yet fixed
	if (init_ == 0)
	  init_ = the_state;

	return the_state;
      }
    return available->second;
  }

  void
  fasttgbaexplicit::add_transition(int src, int dst,
				   cube cond, markset acc)
  {
    fast_explicit_state* source = 0;
    fast_explicit_state* destination = 0;
    source =  add_state(src);
    destination = add_state(dst);

    // Now we just have to create condition and acceptance over
    // the transition
    spot::transition t = {cond, acc, destination};
    source->add_successor(t);
  }
}