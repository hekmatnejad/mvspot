// -*- coding: utf-8 -*-
// Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014, 2015 Laboratoire
// de Recherche et Développement de l'Epita (LRDE).
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

#include <set>
#include <map>
#include <deque>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "misc/bitvect.hh"
#include <bddx.h>
#include "misc/hash.hh"
#include "misc/bddlt.hh"
#include "twa/bdddict.hh"
#include "twa/twa.hh"
#include "misc/hashfunc.hh"
#include "ltlast/formula.hh"
#include "ltlast/constant.hh"
#include "tgbaalgos/dotty.hh"
#include "twa/twasafracomplement.hh"
#include "tgbaalgos/degen.hh"

namespace spot
{
  namespace
  {
    // forward decl.
    struct safra_tree;
    struct safra_tree_ptr_less_than:
      public std::binary_function<const safra_tree*, const safra_tree*, bool>
    {
      bool
      operator()(const safra_tree* left,
                 const safra_tree* right) const;
    };

    /// \brief Automaton with Safra's tree as states.
    struct safra_tree_automaton
    {
      safra_tree_automaton(const const_twa_graph_ptr& sba);
      ~safra_tree_automaton();
      typedef std::map<bdd, const safra_tree*, bdd_less_than> transition_list;
      typedef
      std::map<safra_tree*, transition_list, safra_tree_ptr_less_than>
      automaton_t;
      automaton_t automaton;

      /// \brief The number of acceptance pairs of this Rabin (Streett)
      /// automaton.
      int get_nb_acceptance_pairs() const;
      safra_tree* get_initial_state() const;
      void set_initial_state(safra_tree* s);
      const const_twa_graph_ptr& get_sba(void) const
      {
	return a_;
      }
    private:
      mutable int max_nb_pairs_;
      safra_tree* initial_state;
      const_twa_graph_ptr a_;
    };

    /// \brief A Safra tree, used as state during the determinization
    /// of a Büchi automaton
    ///
    /// It is the key structure of the construction.
    /// Each node of the tree has:
    ///  - A \a name,
    ///  - A subset of states of the original Büchi automaton (\a nodes),
    ///  - A flag that is \a marked to denote vertical merge of nodes,
    ///  - A list of \a children.
    ///
    /// This class implements operations:
    ///  - To compute the successor of this tree,
    ///  - To retrive acceptance condition on this tree.
    /// \see safra_determinisation.
    struct safra_tree
    {
      typedef std::set<const state*, state_ptr_less_than> subset_t;
      typedef std::list<safra_tree*> child_list;
      typedef std::multimap<bdd, const state*, bdd_less_than> tr_cache_t;
      typedef std::map<const state*, tr_cache_t,
                       state_ptr_less_than> cache_t;

      safra_tree();
      safra_tree(const safra_tree& other);
      safra_tree(const subset_t& nodes, safra_tree* p, int n);
      ~safra_tree();

      safra_tree& operator=(const safra_tree& other);
      int compare(const safra_tree* other) const;
      size_t hash() const;

      void add_node(const state* s);
      int max_name() const;

      // Operations to get successors of a tree.
      safra_tree* branch_accepting(const twa_graph& a);
      safra_tree* succ_create(const bdd& condition,
                              cache_t& cache_transition);
      safra_tree* normalize_siblings();
      safra_tree* remove_empty();
      safra_tree* mark();

      // To get Rabin/Streett acceptance conditions.
      void getL(bitvect& bitset) const;
      void getU(bitvect& bitset) const;

      /// \brief Is this node the root of the tree?
      bool is_root() const
      {
        return parent == 0;
      }

      bool marked;
      int name;
      subset_t nodes;
      child_list children;
    private:
      void remove_node_from_children(const state* state);
      int get_new_name() const;
      mutable std::deque<int> free_names_; // Some free names.
      safra_tree* parent;
    };

    safra_tree::safra_tree()
      : marked(false), name(0)
    {
      parent = 0;
    }

    /// \brief Copy the tree \a other, and set \c marked to false.
    safra_tree::safra_tree(const safra_tree& other)
      : marked(false), name(other.name), nodes(other.nodes)
    {
      parent = 0;
      for (auto i: other.children)
      {
        safra_tree* c = new safra_tree(*i);
        c->parent = this;
        children.push_back(c);
      }
    }

    safra_tree::safra_tree(const subset_t& nodes, safra_tree* p, int n)
      : marked(false), name(n), nodes(nodes)
    {
      parent = p;
    }

    safra_tree::~safra_tree()
    {
      for (auto c: children)
        delete c;
      for (auto n: nodes)
        n->destroy();
    }

    safra_tree&
    safra_tree::operator=(const safra_tree& other)
    {
      if (this != &other)
      {
        this->~safra_tree();
        new (this) safra_tree(other);
      }
      return *this;
    }

    /// \brief Compare two safra trees.
    ///
    /// \param other the tree to compare too.
    ///
    /// \return 0 if the trees are the same. Otherwise
    /// a signed value.
    int safra_tree::compare(const safra_tree* other) const
    {
      int res = name - other->name;
      if (res != 0)
	return res;

      if (marked != other->marked)
        return (marked) ? -1 : 1;

      res = nodes.size() - other->nodes.size();
      if (res != 0)
	return res;

      res = children.size() - other->children.size();
      if (res != 0)
      	return res;

      // Call compare() only as a last resort, because it takes time.

      subset_t::const_iterator in1 = nodes.begin();
      subset_t::const_iterator in2 = other->nodes.begin();
      for (; in1 != nodes.end(); ++in1, ++in2)
        if ((res = (*in1)->compare(*in2)) != 0)
	  return res;

      child_list::const_iterator ic1 = children.begin();
      child_list::const_iterator ic2 = other->children.begin();
      for (; ic1 != children.end(); ++ic1, ++ic2)
	if ((res = (*ic1)->compare(*ic2)) != 0)
	  return res;

      return 0;
    }


    /// \brief Hash a safra tree.
    size_t
    safra_tree::hash() const
    {
      size_t hash = 0;
      hash ^= wang32_hash(name);
      hash ^= wang32_hash(marked);

      for (auto n: nodes)
        hash ^= n->hash();
      for (auto c: children)
        hash ^= c->hash();

      return hash;
    }

    void
    safra_tree::add_node(const state* s)
    {
      nodes.insert(s);
    }

    /*---------------------------------.
    | Operations to compute successors |
    `---------------------------------*/

    int
    safra_tree::max_name() const
    {
      int max_name = name;
      for (auto c: children)
        max_name = std::max(max_name, c->max_name());
      return max_name;
    }

    /// \brief Get a unused name in the tree for a new node.
    ///
    /// The root of the tree maintains a list of unused names.
    /// When this list is empty, new names are computed.
    int
    safra_tree::get_new_name() const
    {
      if (parent == 0)
      {
        if (free_names_.empty())
        {
          std::set<int> used_names;
          std::deque<const safra_tree*> queue;
          queue.push_back(this);
          while (!queue.empty())
          {
            const safra_tree* current = queue.front();
            queue.pop_front();
            used_names.insert(current->name);
	    for (auto c: current->children)
              queue.push_back(c);
          }

          int l = 0;
          int nb_found = 0;
          std::set<int>::const_iterator i = used_names.begin();
          while (i != used_names.end() && nb_found != 3)
          {
            if (l != *i)
            {
              free_names_.push_back(l);
              ++nb_found;
            }
            else
              ++i;
            ++l;
          }

          while (nb_found++ < 3)
            free_names_.push_back(l++);
        }

        int result = free_names_.front();
        free_names_.pop_front();
        return result;
      }
      else
        return parent->get_new_name();
    }

    /// If the node has an accepting state in its label, a new child
    /// is inserted with the set of all accepting states of \c nodes
    /// as label and an unused name.
    safra_tree*
    safra_tree::branch_accepting(const twa_graph& a)
    {
      for (auto c: children)
        c->branch_accepting(a);

      subset_t subset;
      for (auto n: nodes)
        if (a.state_is_accepting(n))
          subset.insert(n);

      if (!subset.empty())
        children.push_back(new safra_tree(subset, this, get_new_name()));

      return this;
    }

    /// \brief A powerset construction.
    ///
    /// The successors of each state in \c nodes with \a condition
    /// as atomic property remplace the current \c nodes set.
    ///
    /// @param cache_transition is a map of the form: state -> bdd -> state.
    ///        Only states present in \c nodes are keys of the map.
    /// @param condition is an atomic property. We are looking for successors
    ///        of this atomic property.
    safra_tree*
    safra_tree::succ_create(const bdd& condition,
                            cache_t& cache_transition)
    {
      subset_t new_subset;

      for (auto n: nodes)
      {
	cache_t::const_iterator it = cache_transition.find(n);
        if (it == cache_transition.end())
          continue;

        const tr_cache_t& transitions = it->second;
        for (auto t: transitions)
        {
          if ((t.first & condition) != bddfalse)
          {
            if (new_subset.find(t.second) == new_subset.end())
            {
              const state* s = t.second->clone();
              new_subset.insert(s);
            }
          }
        }
      }
      std::swap(nodes, new_subset);

      for (auto c: children)
        c->succ_create(condition, cache_transition);

      return this;
    }

    /// \brief Horizontal Merge
    ///
    /// If many children share the same state in their labels, we must keep
    /// only one occurrence (in the older node).
    safra_tree*
    safra_tree::normalize_siblings()
    {
      std::set<const state*, state_ptr_less_than> node_set;
      for (auto c: children)
      {
        subset_t::iterator node_it = c->nodes.begin();
        while (node_it != c->nodes.end())
        {
	  if (!node_set.insert(*node_it).second)
	  {
            const state* s = *node_it;
            c->remove_node_from_children(*node_it);
            c->nodes.erase(node_it++);
            s->destroy();
          }
	  else
	  {
	    ++node_it;
	  }
        }

        c->normalize_siblings();
      }

      return this;
    }

    /// \brief Remove recursively all the occurrences of \c state in the label.
    void
    safra_tree::remove_node_from_children(const state* state)
    {
      for (auto c: children)
      {
        subset_t::iterator it = c->nodes.find(state);
        if (it != c->nodes.end())
        {
          const spot::state* s = *it;
	  c->nodes.erase(it);
          s->destroy();
        }
        c->remove_node_from_children(state);
      }
    }

    /// \brief Remove empty nodes
    ///
    /// If a child of the node has an empty label, we remove this child.
    safra_tree*
    safra_tree::remove_empty()
    {
      child_list::iterator child_it = children.begin();
      while (child_it != children.end())
      {
        if ((*child_it)->nodes.empty())
        {
          safra_tree* to_delete = *child_it;
          child_it = children.erase(child_it);
          delete to_delete;
        }
        else
        {
          (*child_it)->remove_empty();
          ++child_it;
        }
      }

      return this;
    }

    /// \brief Vertical merge
    ///
    /// If a parent has the same states as its childen in its label,
    /// All the children a deleted and the node is marked. This mean
    /// an accepting infinite run is found.
    safra_tree*
    safra_tree::mark()
    {
      std::set<const state*, state_ptr_less_than> node_set;
      for (auto c: children)
      {
        node_set.insert(c->nodes.begin(), c->nodes.end());
        c->mark();
      }

      char same = node_set.size() == nodes.size();

      if (same)
      {
        subset_t::const_iterator i = node_set.begin();
        subset_t::const_iterator j = nodes.begin();

        while (i != node_set.end() && j != nodes.end())
        {
          if ((*i)->compare(*j) != 0)
          {
            same = 0;
            break;
          }
          ++i;
          ++j;
        }
      }

      if (same)
      {
        marked = true;
        for (auto c: children)
          delete c;
        children = child_list();
      }

      return this;
    }

    /*-----------------------.
    | Acceptances conditions |
    `-----------------------*/

    /// Returns in which sets L (the semantic differs according to Rabin or
    /// Streett) the state-tree is included.
    ///
    /// \param bitset a bitset of size \c this->get_nb_acceptance_pairs()
    /// filled with FALSE bits.
    /// On return bitset[i] will be set if this state-tree is included in L_i.
    void
    safra_tree::getL(bitvect& bitset) const
    {
      assert(bitset.size() > static_cast<unsigned>(name));
      if (marked && !nodes.empty())
        bitset.set(name);
      for (auto c: children)
        c->getL(bitset);
    }

    /// Returns in which sets U (the semantic differs according to Rabin or
    /// Streett) the state-tree is included.
    ///
    /// \param bitset a bitset of size \c this->get_nb_acceptance_pairs()
    /// filled with TRUE bits.
    /// On return bitset[i] will be set if this state-tree is included in U_i.
    void
    safra_tree::getU(bitvect& bitset) const
    {
      assert(bitset.size() > static_cast<unsigned>(name));
      if (!nodes.empty())
        bitset.clear(name);
      for (auto c: children)
        c->getU(bitset);
    }

    bool
    safra_tree_ptr_less_than::operator()(const safra_tree* left,
                                         const safra_tree* right) const
    {
      assert(left);
      return left->compare(right) < 0;
    }

    struct safra_tree_ptr_equal:
      public std::unary_function<const safra_tree*, bool>
    {
      safra_tree_ptr_equal(const safra_tree* left)
        : left_(left) {}

      bool
      operator()(const safra_tree* right) const
      {
        return left_->compare(right) == 0;
      }
    private:
      const safra_tree* left_;
    };

    /// \brief Algorithm to determinize Büchi automaton.
    ///
    /// Determinization of a Büchi automaton into a Rabin automaton
    /// using the Safra's construction.
    ///
    /// This construction is presented in:
    /// @PhDThesis{      safra.89.phd,
    ///     author     = {Shmuel Safra},
    ///     title      = {Complexity of Automata on Infinite Objects},
    ///     school     = {The Weizmann Institute of Science},
    ///     year       = {1989},
    ///     address    = {Rehovot, Israel},
    ///     month      = mar
    /// }
    ///
    class safra_determinisation
    {
    public:
      static safra_tree_automaton*
      create_safra_automaton(const const_twa_graph_ptr& a);
    private:
      typedef std::set<int> atomic_list_t;
      typedef std::set<bdd, bdd_less_than> conjunction_list_t;
      static void retrieve_atomics(const safra_tree* node,
				   twa_graph_ptr sba_aut,
                                   safra_tree::cache_t& cache,
                                   atomic_list_t& atomic_list);
      static void set_atomic_list(atomic_list_t& list, bdd condition);
      static conjunction_list_t
      get_conj_list(const atomic_list_t& atomics);
    };

    /// \brief The body of Safra's construction.
    safra_tree_automaton*
    safra_determinisation::create_safra_automaton
    (const const_twa_graph_ptr& a)
    {
      // initialization.
      auto sba_aut = degeneralize(a);

      safra_tree_automaton* st = new safra_tree_automaton(sba_aut);

      std::deque<safra_tree*> queue;
      safra_tree* q0 = new safra_tree();
      q0->add_node(sba_aut->get_init_state());
      queue.push_back(q0);
      st->set_initial_state(q0);

      // main loop
      while (!queue.empty())
      {
        safra_tree* current = queue.front();
        // safra_tree* node = new safra_tree(*current);

        // Get conjunction list and save successors.
        safra_tree::cache_t cache;
        atomic_list_t atomic_list;
        retrieve_atomics(current, sba_aut, cache, atomic_list);
        conjunction_list_t conjunction = get_conj_list(atomic_list);

        // Create successors of the Safra's tree.
        safra_tree_automaton::transition_list transitions;
        for (auto i: conjunction)
        {
          safra_tree* successor = new safra_tree(*current);
          successor->branch_accepting(*sba_aut); // Step 2
          successor->succ_create(i, cache); // Step 3
          successor->normalize_siblings(); // Step 4
          successor->remove_empty(); // Step 5
          successor->mark(); // Step 6

          bool delete_this_successor = true;
          safra_tree_ptr_equal comparator(successor);
          if (st->automaton.find(successor) != st->automaton.end())
          {
            transitions[i] = st->automaton.find(successor)->first;
          }
          else
          {
            std::deque<safra_tree*>::iterator item_in_queue =
              std::find_if(queue.begin(), queue.end(), comparator);
            if (item_in_queue != queue.end())
            {
              transitions[i] = *item_in_queue;
            }
            else
            {
              delete_this_successor = false;
              transitions[i] = successor;
              queue.push_back(successor);
            }
          }
          if (delete_this_successor)
            delete successor;
        }

        if (st->automaton.find(current) == st->automaton.end())
          st->automaton[current] = transitions;

        queue.pop_front();

        for (auto i: cache)
          for (auto j: i.second)
            j.second->destroy();
        // delete node;
      }

      // delete sba_aut;
      return st;
    }

    /// Retrieve all atomics properties that are in successors formulae
    /// of the states in the label of the node.
    void
    safra_determinisation::retrieve_atomics(const safra_tree* node,
                                            twa_graph_ptr sba_aut,
                                            safra_tree::cache_t& cache,
                                            atomic_list_t& atomic_list)
    {
      for (auto n: node->nodes)
      {
        safra_tree::tr_cache_t transitions;
	for (auto iterator: sba_aut->succ(n))
        {
          bdd condition = iterator->current_condition();
          typedef std::pair<bdd, const state*> bdd_state;
          transitions.insert(bdd_state(condition, iterator->current_state()));
          set_atomic_list(atomic_list, condition);
        }
        cache[n] = transitions;
      }
    }

    /// Insert in \a list atomic properties of the formula \a c.
    void
    safra_determinisation::set_atomic_list(atomic_list_t& list, bdd c)
    {
      bdd current = bdd_satone(c);
      while (current != bddtrue && current != bddfalse)
      {
        list.insert(bdd_var(current));
        bdd high = bdd_high(current);
        if (high == bddfalse)
          current = bdd_low(current);
        else
          current = high;
      }
    }

    /// From the list of atomic properties \a atomics, create the list
    /// of all the conjunctions of properties.
    safra_determinisation::conjunction_list_t
    safra_determinisation::get_conj_list(const atomic_list_t& atomics)
    {
      conjunction_list_t list;
      unsigned atomics_size = atomics.size();

      assert(atomics_size < 32);
      for (unsigned i = 1; i <= static_cast<unsigned>(1 << atomics_size); ++i)
      {
        bdd result = bddtrue;
        unsigned position = 1;
        for (auto a: atomics)
        {
          bdd this_atomic;
          if (position & i)
            this_atomic = bdd_ithvar(a);
          else
            this_atomic = bdd_nithvar(a);
          result &= this_atomic;
	  position <<= 1;
        }
        list.insert(result);
      }

      return list;
    }

    // Safra's test part. Dot output.
    //////////////////////////////
    namespace test
    {
      typedef std::unordered_map<const state*, int,
				 state_ptr_hash, state_ptr_equal> stnum_t;

      void print_safra_tree(const safra_tree* this_node,
                            stnum_t& node_names,
                            int& current_node,
                            int nb_accepting_conditions)
      {
        std::string conditions;
        if (this_node->is_root())
        {
          bitvect* l = make_bitvect(nb_accepting_conditions);
	  bitvect* u = make_bitvect(nb_accepting_conditions);
          u->set_all();
          this_node->getL(*l);
          this_node->getU(*u);
          std::stringstream s;
          s << "\\nL:" << *l << ", U:" << *u;
          conditions = s.str();
	  delete u;
	  delete l;
        }

        std::cout << "node" << this_node << "[label=\"";
        std::cout << this_node->name << '|';
        for (auto j: this_node->nodes)
        {
	  stnum_t::const_iterator it = node_names.find(j);
	  int name;
          if (it == node_names.end())
	    name = node_names[j] = current_node++;
	  else
	    name = it->second;
          std::cout << name << ", ";
        }
        std::cout << conditions;
        if (this_node->marked)
          std::cout << "\", style=filled, fillcolor=\"gray";

        std::cout << "\"];" << std::endl;

        safra_tree::child_list::const_iterator i = this_node->children.begin();
        for (; i != this_node->children.end(); ++i)
        {
          print_safra_tree(*i, node_names, current_node,
                           nb_accepting_conditions);
          std::cout << "node" << this_node << " -> node" << *i
                    << "[color=\"red\", arrowhead=\"none\"];"
                    << std::endl;
        }
      }

      void print_safra_automaton(safra_tree_automaton* a)
      {
        typedef safra_tree_automaton::automaton_t::reverse_iterator
          automaton_cit;
        stnum_t node_names;
        int current_node = 0;
        int nb_accepting_conditions = a->get_nb_acceptance_pairs();

        std::cout << "digraph A {" << std::endl;

	/// GCC 3.3 complains if a const_reverse_iterator is used.
	/// error: no match for 'operator!='
        for (automaton_cit i = a->automaton.rbegin();
             i != a->automaton.rend();
             ++i)
        {
          std::cout << "subgraph sg" << i->first << '{' << std::endl;
          print_safra_tree(i->first, node_names, current_node,
                           nb_accepting_conditions);
          std::cout << "}\n";

          // Successors.
          for (const auto& j: i->second)
            std::cout << "node" << i->first << "->"
                      << "node" << j.second <<
              " [label=\"" << bddset << j.first << "\"];\n";
        }

	// Output the real name of all states.
	std::cout << "{ rank=sink; legend [shape=none,margin=0,label=<\n"
		  << "<TABLE BORDER='1' CELLBORDER='0' CELLSPACING='0'>\n";

	for (const auto& nn: node_names)
	  std::cout << "<TR><TD>" << nn.second << "</TD><TD>"
		    << a->get_sba()->format_state(nn.first)
		    << "</TD></TR>\n";
	std::cout << "</TABLE>\n>]}\n}\n";
      }
    } // test

    ////////////////////////////////////////
    // state_complement

    /// States used by spot::tgba_safra_complement.
    /// \ingroup twa_representation
    class state_complement : public state
    {
    public:
      state_complement(bitvect* U, bitvect* L, const safra_tree* tree,
                       bool use_bitset = true);
      state_complement(const state_complement& other);
      virtual ~state_complement();

      /// \return the safra tree associated to this state.
      const safra_tree* get_safra() const
      {
        return tree;
      }

      /// \return in which sets U this state is included.
      const bitvect& get_U() const
      {
        return *U;
      }

      /// \return in which sets L this state is included.
      const bitvect& get_L() const
      {
        return *L;
      }

      /// \return whether this state track an infinite run.
      bool get_use_bitset() const
      {
        return use_bitset;
      }

      virtual int compare(const state* other) const;
      virtual size_t hash() const;
      virtual state_complement* clone() const;

      std::string to_string() const;
      const state* get_state() const;
    private:
      bitvect* U;
      bitvect* L;
      const safra_tree* tree;
      bool use_bitset;
    };

    state_complement::state_complement(bitvect* L, bitvect* U,
                                       const safra_tree* tree,
                                       bool use_bitset)
      : state(), U(U), L(L), tree(tree), use_bitset(use_bitset)
    {
    }

    state_complement::state_complement(const state_complement& other)
      : state()
    {
      U = other.U->clone();
      L = other.L->clone();
      tree = other.tree;
      use_bitset = other.use_bitset;
    }

    state_complement::~state_complement()
    {
      delete U;
      delete L;
    }

    int
    state_complement::compare(const state* other) const
    {
      if (other == this)
        return 0;
      const state_complement* s = down_cast<const state_complement*>(other);
      assert(s);
#if TRANSFORM_TO_TBA
      // When we transform to TBA instead of TGBA, states depend on the U set.
      if (*U != *s->U)
        return (*U < *s->U) ? -1 : 1;
#endif
      if (*L != *s->L)
        return (*L < *s->L) ? -1 : 1;
      if (use_bitset != s->use_bitset)
        return use_bitset - s->use_bitset;
      return tree->compare(s->tree);
    }

    size_t
    state_complement::hash() const
    {
      size_t hash = tree->hash();
      hash ^= wang32_hash(use_bitset);
      hash ^= L->hash();
#if TRANSFORM_TO_TBA
      hash ^= U->hash();
#endif
      return hash;
    }

    state_complement*
    state_complement::clone() const
    {
      return new state_complement(*this);
    }

    const state*
    state_complement::get_state() const
    {
      return this;
    }

    std::string
    state_complement::to_string() const
    {
      std::stringstream ss;
      ss << tree;
      if (use_bitset)
      {
        ss << " - I:" << *L;
#if TRANSFORM_TO_TBA
        ss << " J:" << *U;
#endif
      }
      return ss.str();
    }

    /// Successor iterators used by spot::tgba_safra_complement.
    /// \ingroup twa_representation
    class twa_safra_complement_succ_iterator: public twa_succ_iterator
    {
    public:
      typedef std::multimap<bdd, state_complement*, bdd_less_than> succ_list_t;

      twa_safra_complement_succ_iterator(const succ_list_t& list,
                                          acc_cond::mark_t the_acceptance_cond)
        : list_(list), the_acceptance_cond_(the_acceptance_cond)
      {
      }

      virtual
      ~twa_safra_complement_succ_iterator()
      {
        for (auto& p: list_)
          delete p.second;
      }

      virtual bool first();
      virtual bool next();
      virtual bool done() const;
      virtual state_complement* current_state() const;
      virtual bdd current_condition() const;
      virtual acc_cond::mark_t current_acceptance_conditions() const;
    private:
      succ_list_t list_;
      acc_cond::mark_t the_acceptance_cond_;
      succ_list_t::const_iterator it_;
    };

    bool
    twa_safra_complement_succ_iterator::first()
    {
      it_ = list_.begin();
      return it_ != list_.end();
    }

    bool
    twa_safra_complement_succ_iterator::next()
    {
      ++it_;
      return it_ != list_.end();
    }

    bool
    twa_safra_complement_succ_iterator::done() const
    {
      return it_ == list_.end();
    }

    state_complement*
    twa_safra_complement_succ_iterator::current_state() const
    {
      assert(!done());
      return new state_complement(*(it_->second));
    }

    bdd
    twa_safra_complement_succ_iterator::current_condition() const
    {
      assert(!done());
      return it_->first;
    }

    acc_cond::mark_t
    twa_safra_complement_succ_iterator::current_acceptance_conditions() const
    {
      assert(!done());
      return the_acceptance_cond_;
    }

  } // anonymous

  // safra_tree_automaton
  ////////////////////////

  safra_tree_automaton::safra_tree_automaton(const const_twa_graph_ptr& a)
    : max_nb_pairs_(-1), initial_state(0), a_(a)
  {
    a->get_dict()->register_all_variables_of(a, this);
  }

  safra_tree_automaton::~safra_tree_automaton()
  {
    for (auto& p: automaton)
      delete p.first;
  }

  int
  safra_tree_automaton::get_nb_acceptance_pairs() const
  {
    if (max_nb_pairs_ != -1)
      return max_nb_pairs_;

    int max = -1;
    for (auto& p: automaton)
      max = std::max(max, p.first->max_name());
    return max_nb_pairs_ = max + 1;
  }

  safra_tree*
  safra_tree_automaton::get_initial_state() const
  {
    return initial_state;
  }

  void
  safra_tree_automaton::set_initial_state(safra_tree* s)
  {
    initial_state = s;
  }

  // End of the safra construction
  //////////////////////////////////////////

  // tgba_safra_complement
  //////////////////////////

  tgba_safra_complement::tgba_safra_complement(const const_twa_graph_ptr& a)
    : twa(a->get_dict()), automaton_(a),
      safra_(safra_determinisation::create_safra_automaton(a))
  {
    assert(safra_ || !"safra construction fails");

#if TRANSFORM_TO_TBA
    the_acceptance_cond_ = acc_.mark(acc_.add_set());
#endif

#if TRANSFORM_TO_TGBA
    unsigned nb_acc =
      static_cast<safra_tree_automaton*>(safra_)->get_nb_acceptance_pairs();

    acceptance_cond_vec_.reserve(nb_acc);
    for (unsigned i = 0; i < nb_acc; ++i)
      acceptance_cond_vec_.push_back(acc_.mark(acc_.add_set()));
#endif
    acc_.set_generalized_buchi();
  }

  tgba_safra_complement::~tgba_safra_complement()
  {
    get_dict()->unregister_all_my_variables(safra_);
    delete static_cast<safra_tree_automaton*>(safra_);
  }

  state*
  tgba_safra_complement::get_init_state() const
  {
    safra_tree_automaton* a = static_cast<safra_tree_automaton*>(safra_);
    bitvect* empty = make_bitvect(a->get_nb_acceptance_pairs());
    return new state_complement(empty->clone(), empty,
				a->get_initial_state(), false);
  }


  /// @brief Compute successors of the state @a local_state, and returns an
  /// iterator on the successors collection.
  ///
  /// The old algorithm is a Deterministic Streett to nondeterministic Büchi
  /// transformation, presented by Christof Löding in his Diploma thesis.
  ///
  /// @MastersThesis{	  loding.98,
  ///   author		= {Christof L\"oding},
  ///   title		= {Methods for the Transformation of omega-Automata:
  ///			  Complexity and Connection to Second Order Logic},
  ///   school		= {University of Kiel},
  ///   year		= {1998}
  /// }
  ///
  ///
  /// The new algorithm produce a TGBA instead of a TBA. This algorithm
  /// comes from:
  ///
  /// @InProceedings{   duret.09.atva,
  ///   author        = {Alexandre Duret-Lutz and Denis Poitrenaud and
  ///                    Jean-Michel Couvreur},
  ///   title         = {On-the-fly Emptiness Check of Transition-based
  ///                    {S}treett Automata},
  ///   booktitle     = {Proceedings of the 7th International Symposium on
  ///                   Automated Technology for Verification and Analysis
  ///                   (ATVA'09)},
  ///   year          = {2009},
  ///   editor        = {Zhiming Liu and Anders P. Ravn},
  ///   series        = {Lecture Notes in Computer Science},
  ///   publisher     = {Springer-Verlag}
  /// }
  twa_succ_iterator*
  tgba_safra_complement::succ_iter(const state* state) const
  {
    const safra_tree_automaton* a = static_cast<safra_tree_automaton*>(safra_);
    const state_complement* s = down_cast<const state_complement*>(state);
    assert(s);
    safra_tree_automaton::automaton_t::const_iterator tr =
      a->automaton.find(const_cast<safra_tree*>(s->get_safra()));

    assert(tr != a->automaton.end());

    acc_cond::mark_t condition = 0U;
    twa_safra_complement_succ_iterator::succ_list_t succ_list;
    int nb_acceptance_pairs = a->get_nb_acceptance_pairs();
    bitvect* e = make_bitvect(nb_acceptance_pairs);

    if (!s->get_use_bitset()) // if \delta'(q, a)
      {
        for (auto& p: tr->second)
	  {
	    state_complement* s1 = new state_complement(e->clone(), e->clone(),
							p.second, false);
	    state_complement* s2 = new state_complement(e->clone(), e->clone(),
							p.second, true);
	    succ_list.emplace(p.first, s1);
	    succ_list.emplace(p.first, s2);
	  }
      }
    else
      {
        bitvect* l = make_bitvect(nb_acceptance_pairs);
        bitvect* u = make_bitvect(nb_acceptance_pairs);
        u->set_all();
        s->get_safra()->getL(*l); // {i : q \in L_i}
        s->get_safra()->getU(*u); // {j : q \in U_i}
        state_complement* st;

#if TRANSFORM_TO_TBA
        bitvect* newI = s->get_L().clone();
	*newI |= *l; // {I' = I \cup {i : q \in L_i}}
        bitvect* newJ = s->get_U().clone();
	*newJ |= *u; // {J' = J \cup {j : q \in U_i}}

	// \delta'((q, I, J), a) if I'\subseteq J'
        if (newI->is_subset_of(*newJ))
	  {
	    for (auto& p: tr->second)
	      {
		st = new state_complement(e->clone(), e->clone(),
					  p.second, true);
		succ_list.emplace(p.first, st);
	      }
	    condition = the_acceptance_cond_;
	  }
        else  // \delta'((q, I, J), a)
	  {
	    for (auto& p: tr->second)
	      {
		st = new state_complement(newI, newJ, p.second, true);
		succ_list.emplace(p.first, st);
	      }
	  }
	delete newI;
	delete newJ;
#else
	// {pending = S \cup {i : q \in L_i} \setminus {j : q \in U_j})}
	const bitvect& S = s->get_L();
        bitvect* pending = S.clone();
	*pending |= *l;
	*pending -= *u;
        for (auto& p: tr->second)
	  {
	    st = new state_complement(pending->clone(), e->clone(),
				      p.second, true);
	    succ_list.emplace(p.first, st);
	  }
	delete pending;

        for (unsigned i = 0; i < l->size(); ++i)
          if (!S.get(i))
	    condition |= acceptance_cond_vec_[i];
#endif
	delete u;
	delete l;
      }
    delete e;
    return new twa_safra_complement_succ_iterator(succ_list, condition);
  }

  std::string
  tgba_safra_complement::format_state(const state* state) const
  {
    const state_complement* s =
      down_cast<const state_complement*>(state);
    assert(s);
    return s->to_string();
  }

  bdd
  tgba_safra_complement::compute_support_conditions(const state* state) const
  {
    const safra_tree_automaton* a = static_cast<safra_tree_automaton*>(safra_);
    const state_complement* s = down_cast<const state_complement*>(state);
    assert(s);
    typedef safra_tree_automaton::automaton_t::const_iterator auto_it;
    auto_it node(a->automaton.find(const_cast<safra_tree*>(s->get_safra())));

    if (node == a->automaton.end())
      return bddtrue;

    bdd res = bddtrue;
    for (auto& i: node->second)
      res |= i.first;
    return res;
  }

  // display_safra: debug routine.
  //////////////////////////////
  void display_safra(const const_tgba_safra_complement_ptr& a)
  {
    test::print_safra_automaton(static_cast<safra_tree_automaton*>
				(a->get_safra()));
  }
}