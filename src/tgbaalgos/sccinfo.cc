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

#include "sccinfo.hh"
#include <stack>
#include <algorithm>
#include <queue>
#include "tgba/bddprint.hh"
#include "misc/escape.hh"
#include "priv/accconv.hh"

namespace spot
{

  namespace
  {
    struct scc
    {
    public:
      scc(int index, bdd in_cond, bdd in_acc):
	index(index), in_cond(in_cond), in_acc(in_acc)
      {
      }

      int index;	 // Index of the SCC
      bdd in_cond;	 // Condition on incoming transition
      bdd in_acc;	 // Acceptance sets on the incoming transition
      scc_info::scc_node node;
    };
  }

  scc_info::scc_info(const tgba_digraph* aut)
    : aut_(aut)
  {
    unsigned n = aut->num_states();
    sccof_.resize(n, -1U);
    bdd all_acc = aut->all_acceptance_conditions();

    typedef std::list<scc> stack_type;
    stack_type root_;		// Stack of SCC roots.
    std::stack<std::pair<bdd, bdd>> arc_; // A stack of acceptance conditions
				// between each of these SCC.
    std::vector<int> h_(n, 0);
    // Map of visited states.  Values > 0 designate maximal SCC.
    // Values < 0 number states that are part of incomplete SCCs being
    // completed.  0 denotes non-visited states.

    int num_;			// Number of visited nodes, negated.

    typedef tgba_digraph::graph_t::const_iterator iterator;
    typedef std::pair<unsigned, iterator> pair_state_iter;
    std::stack<pair_state_iter> todo_; // DFS stack.  Holds (STATE,
				       // ITERATOR) pairs where
				       // ITERATOR is an iterator over
				       // the successors of STATE.
				       // ITERATOR should always be
				       // freed when TODO is popped,
				       // but STATE should not because
				       // it is used as a key in H.


    // Setup depth-first search from the initial state.
    {
      unsigned init = aut->get_init_state_number();
      num_ = -1;
      h_[init] = num_;
      root_.emplace_front(num_, bddfalse, bddfalse);
      todo_.emplace(init, aut->out(init).begin());
    }

    while (!todo_.empty())
      {
	// We are looking at the next successor in SUCC.
	iterator succ = todo_.top().second;

	// If there is no more successor, backtrack.
	if (!succ)
	  {
	    // We have explored all successors of state CURR.
	    unsigned curr = todo_.top().first;

	    // Backtrack TODO_.
	    todo_.pop();

	    // Fill STATES with any state removed, so that
	    // remove_component() does not have to traverse the SCC
	    // again.
	    root_.front().node.states.push_front(curr);

	    // When backtracking the root of an SCC, we must also
	    // remove that SCC from the ARC/ROOT stacks.  We must
	    // discard from H all reachable states from this SCC.
	    assert(!root_.empty());
	    if (root_.front().index == h_[curr])
	      {
		int num = node_.size();
		for (auto s: root_.front().node.states)
		  {
		    sccof_[s] = num;
		    h_[s] = num + 1;
		  }
		bdd cond = root_.front().in_cond;
		bdd acc = root_.front().node.acc;
		bool triv = root_.front().node.trivial;
		node_.emplace_back(acc, triv);
		std::swap(node_.back().succ, root_.front().node.succ);
		std::swap(node_.back().states, root_.front().node.states);
		node_.back().accepting = acc == all_acc;
		root_.pop_front();
		// Record the transition between the SCC being popped
		// and the previous SCC.
		if (!root_.empty())
		  root_.front().node.succ.emplace_back(cond, num);
	      }
	    continue;
	  }

	// We have a successor to look at.
	// Fetch the values we are interested in...
	unsigned dest = succ->dst;
	bdd acc = succ->acc;
	bdd cond = succ->cond;
	++todo_.top().second;

	// We do not need SUCC from now on.

	// Are we going to a new state?
	int spi = h_[dest];
	if (spi == 0)
	  {
	    // Yes.  Number it, stack it, and register its successors
	    // for later processing.
	    h_[dest] = --num_;
	    root_.emplace_front(num_, cond, acc);
	    todo_.emplace(dest, aut->out(dest).begin());
	    continue;
	  }

	// We already know the state.

	// Have we reached a maximal SCC?
	if (spi > 0)
	  {
	    --spi;
	    // Record that there is a transition from this SCC to the
	    // dest SCC labelled with cond.
	    auto& succ = root_.front().node.succ;
	    scc_succs::iterator i = std::find_if(succ.begin(), succ.end(),
						 [spi](const scc_trans& x) {
						   return (x.dst ==
							   (unsigned) spi);
						 });
	    if (i == succ.end())
	      succ.emplace_back(cond, spi);
	    else
	      i->cond |= cond;
	    continue;
	  }

	// Now this is the most interesting case.  We have reached a
	// state S1 which is already part of a non-dead SCC.  Any such
	// non-dead SCC has necessarily been crossed by our path to
	// this state: there is a state S2 in our path which belongs
	// to this SCC too.  We are going to merge all states between
	// this S1 and S2 into this SCC.
	//
	// This merge is easy to do because the order of the SCC in
	// ROOT is descending: we just have to merge all SCCs from the
	// top of ROOT that have an index lesser than the one of
	// the SCC of S2 (called the "threshold").
	int threshold = spi;
	std::list<unsigned> states;
	scc_succs succs;
	while (threshold > root_.front().index)
	  {
	    assert(!root_.empty());
	    acc |= root_.front().node.acc | root_.front().in_acc;
	    states.splice(states.end(), root_.front().node.states);

	    succs.insert(succs.end(),
			 root_.front().node.succ.begin(),
			 root_.front().node.succ.end());
	    root_.pop_front();
	  }

	// Note that we do not always have
	//  threshold == root_.front().index
	// after this loop, the SCC whose index is threshold might have
	// been merged with a higher SCC.

	// Accumulate all acceptance conditions, states, SCC
	// successors, and conditions into the merged SCC.
	root_.front().node.acc |= acc;
	root_.front().node.states.splice(root_.front().node.states.end(),
					 states);
	root_.front().node.succ.insert(root_.front().node.succ.end(),
				       succs.begin(), succs.end());
	// This SCC is no longer trivial.
	root_.front().node.trivial = false;
      }

    // An SCC is useful if it is accepting or it has a successor SCC
    // that is useful.
    unsigned scccount = scc_count();
    for (unsigned i = 0; i < scccount; i++)
      {
	bool useful = node_[i].accepting;
	for (auto j: node_[i].succ)
	  useful |= node_[j.dst].useful;
	node_[i].useful = useful;
      }
  }


  std::vector<bdd> scc_info::used_acc() const
  {
    unsigned n = aut_->num_states();
    std::vector<bdd> result(scc_count());
    acceptance_convertor conv(aut_->neg_acceptance_conditions());

    for (unsigned src = 0; src < n; ++src)
      {
	unsigned src_scc = scc_of(src);
	if (!is_accepting_scc(src_scc))
	  continue;
	for (auto& t: aut_->out(src))
	  {
	    if (scc_of(t.dst) != src_scc)
	      continue;
	    result[src_scc] |= conv.as_full_product(t.acc);
	  }
      }
    return result;
  }

  std::ostream&
  dump_scc_info_dot(std::ostream& out,
		    const tgba_digraph* aut, scc_info* sccinfo)
  {
    scc_info* m = sccinfo ? sccinfo : new scc_info(aut);

    bdd all_acc = aut->all_acceptance_conditions();

    out << "digraph G {\n  i [label=\"\", style=invis, height=0]\n";
    int start = m->scc_of(aut->get_init_state_number());
    out << "  i -> " << start << std::endl;

    std::vector<bool> seen(m->scc_count());
    seen[start] = true;

    std::queue<int> q;
    q.push(start);
    while (!q.empty())
      {
	int state = q.front();
	q.pop();

	out << "  " << state << " [shape=box,"
            << (m->acc(state) == all_acc ? "style=bold," : "")
            << "label=\"" << state;
	{
	  size_t n = m->states_of(state).size();
	  out << " (" << n << " state";
	  if (n > 1)
	    out << 's';
	  out << ')';
	}
	out << "\"]\n";

	for (auto& i: m->succ(state))
	  {
	    int dest = i.dst;
	    bdd label = i.cond;

	    out << "  " << state << " -> " << dest
		<< " [label=\"";
	    escape_str(out, bdd_format_formula(aut->get_dict(), label));
	    out << "\"]\n";

	    if (seen[dest])
	      continue;

	    seen[dest] = true;
	    q.push(dest);
	  }
      }

    out << "}\n";
    if (!sccinfo)
      delete m;
    return out;
  }


}