// -*- coding: utf-8 -*-
// Copyright (C) 2013 Laboratoire de Recherche et Développement
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

#include <iostream>
#include <fstream>
#include <sstream>
#include "dtgbasat.hh"
#include "reachiter.hh"
#include <map>
#include <utility>
#include "scc.hh"
#include "tgba/bddprint.hh"
#include "ltlast/constant.hh"
#include "stats.hh"
#include "ltlenv/defaultenv.hh"
#include "misc/tmpfile.hh"
#include "misc/satsolver.hh"

// If the following DEBUG macro is set to 1, the temporary files used
// to communicate with the SAT-solver will be left in the current
// directory.  (The files dtgba-sat.cnf and dtgba-sat.out contain the
// input and output for the last successful minimization attempted, or
// for the only failed attempt if the minimization failed.)
//
// Additionally, the CNF file will be output with a comment before
// each clause, and an additional output file (dtgba-sat.dbg) will be
// created with a list of all positive variables in the result and
// their meaning.
//
// Note that the code use unique temporary filenames, so it is safe to
// run several such minimizations in parallel.  It only when DEBUG=1
// that some of these files will be renamed to the above hard-coded
// names, possibly causing confusion if multiple minimizations are
// debugged in parallel and in the same directory.

#define DEBUG 0
#if DEBUG
#define dout out << "c "
#define trace std::cerr
#else
#define dout while (0) std::cout
#define trace dout
#endif

namespace spot
{
  namespace
  {
    static bdd_dict* debug_dict = 0;

    struct transition
    {
      int src;
      bdd cond;
      int dst;

      transition(int src, bdd cond, int dst)
	: src(src), cond(cond), dst(dst)
      {
      }

      bool operator<(const transition& other) const
      {
	if (this->src < other.src)
	  return true;
	if (this->src > other.src)
	  return false;
	if (this->dst < other.dst)
	  return true;
	if (this->dst > other.dst)
	  return false;
	return this->cond.id() < other.cond.id();
      }

      bool operator==(const transition& other) const
      {
	return (this->src == other.src
		&& this->dst == other.dst
		&& this->cond.id() == other.cond.id());
      }
    };

    struct transition_acc
    {
      int src;
      bdd cond;
      bdd acc;
      int dst;

      transition_acc(int src, bdd cond, bdd acc, int dst)
	: src(src), cond(cond), acc(acc), dst(dst)
      {
      }

      bool operator<(const transition_acc& other) const
      {
	if (this->src < other.src)
	  return true;
	if (this->src > other.src)
	  return false;
	if (this->dst < other.dst)
	  return true;
	if (this->dst > other.dst)
	  return false;
	if (this->cond.id() < other.cond.id())
	  return true;
	if (this->cond.id() > other.cond.id())
	  return false;
	return this->acc.id() < other.acc.id();
      }

      bool operator==(const transition_acc& other) const
      {
	return (this->src == other.src
		&& this->dst == other.dst
		&& this->cond.id() == other.cond.id()
		&& this->acc.id() == other.acc.id());
      }
    };

    struct state_pair
    {
      int a;
      int b;

      state_pair(int a, int b)
	: a(a), b(b)
      {
      }

      bool operator<(const state_pair& other) const
      {
	if (this->a < other.a)
	  return true;
	if (this->a > other.a)
	  return false;
	if (this->b < other.b)
	  return true;
	if (this->b > other.b)
	  return false;
	return false;
      }
    };

    struct path
    {
      int src_cand;
      int src_ref;
      int dst_cand;
      int dst_ref;
      bdd acc_cand;
      bdd acc_ref;

      path(int src_cand, int src_ref,
	   int dst_cand, int dst_ref,
	   bdd acc_cand, bdd acc_ref)
	: src_cand(src_cand), src_ref(src_ref),
	  dst_cand(dst_cand), dst_ref(dst_ref),
	  acc_cand(acc_cand), acc_ref(acc_ref)
      {
      }

      bool operator<(const path& other) const
      {
	if (this->src_cand < other.src_cand)
	  return true;
	if (this->src_cand > other.src_cand)
	  return false;
	if (this->src_ref < other.src_ref)
	  return true;
	if (this->src_ref > other.src_ref)
	  return false;
	if (this->dst_cand < other.dst_cand)
	  return true;
	if (this->dst_cand > other.dst_cand)
	  return false;
	if (this->dst_ref < other.dst_ref)
	  return true;
	if (this->dst_ref > other.dst_ref)
	  return false;
	if (this->acc_ref.id() < other.acc_ref.id())
	  return true;
	if (this->acc_ref.id() > other.acc_ref.id())
	  return false;
	if (this->acc_cand.id() < other.acc_cand.id())
	  return true;
	if (this->acc_cand.id() > other.acc_cand.id())
	  return false;

	return false;
      }

    };

    std::ostream& operator<<(std::ostream& os, const state_pair& p)
    {
      os << "<" << p.a << "," << p.b << ">";
      return os;
    }

    std::ostream& operator<<(std::ostream& os, const transition& t)
    {
      os << "<" << t.src << ","
	 << bdd_format_formula(debug_dict, t.cond)
	 << "," << t.dst << ">";
      return os;
    }


    std::ostream& operator<<(std::ostream& os, const transition_acc& t)
    {
      os << "<" << t.src << ","
	 << bdd_format_formula(debug_dict, t.cond) << ","
	 << bdd_format_accset(debug_dict, t.acc)
	 << "," << t.dst << ">";
      return os;
    }

    std::ostream& operator<<(std::ostream& os, const path& p)
    {
      os << "<"
	 << p.src_cand << ","
	 << p.src_ref << ","
	 << p.dst_cand << ","
	 << p.dst_ref << ", "
	 << bdd_format_accset(debug_dict, p.acc_cand) << ", "
	 << bdd_format_accset(debug_dict, p.acc_ref) << ">";
      return os;
    }

    struct dict
    {
      dict(const tgba* a)
	: aut(a)
      {
      }

      const tgba* aut;
      typedef std::map<transition, int> trans_map;
      typedef std::map<transition_acc, int> trans_acc_map;
      trans_map transid;
      trans_acc_map transaccid;
      typedef std::map<int, transition> rev_map;
      typedef std::map<int, transition_acc> rev_acc_map;
      rev_map revtransid;
      rev_acc_map revtransaccid;

      std::map<state_pair, int> prodid;
      std::map<path, int> pathid;
      int nvars;
      typedef Sgi::hash_map<const state*, int,
			    state_ptr_hash, state_ptr_equal> state_map;
      typedef Sgi::hash_map<int, const state*> int_map;
      state_map state_to_int;
      int_map int_to_state;
      int cand_size;
      unsigned int cand_nacc;
      std::vector<bdd> cand_acc; // size cand_nacc

      std::vector<bdd> all_cand_acc;
      std::vector<bdd> all_ref_acc;

      bdd cand_all_acc;
      bdd ref_all_acc;

      ~dict()
      {
	state_map::const_iterator s = state_to_int.begin();
	while (s != state_to_int.end())
	  // Always advance the iterator before deleting the key.
	  s++->first->destroy();

	aut->get_dict()->unregister_all_my_variables(this);
      }
    };


    class filler_dfs: public tgba_reachable_iterator_depth_first
    {
    protected:
      dict& d;
      int size_;
      bdd ap_;
      bool state_based_;
    public:
      filler_dfs(const tgba* aut, dict& d, bdd ap, bool state_based)
	: tgba_reachable_iterator_depth_first(aut), d(d), ap_(ap),
	  state_based_(state_based)
      {
	d.nvars = 0;

	bdd_dict* bd = aut->get_dict();
	ltl::default_environment& env = ltl::default_environment::instance();

	d.cand_acc.resize(d.cand_nacc);
	d.all_cand_acc.push_back(bddfalse);

	bdd allneg = bddtrue;
	for (unsigned n = 0; n < d.cand_nacc; ++n)
	  {
	    std::ostringstream s;
	    s << n;
	    const ltl::formula* af = env.require(s.str());
	    int v = bd->register_acceptance_variable(af, &d);
	    af->destroy();
	    d.cand_acc[n] = bdd_ithvar(v);
	    allneg &= bdd_nithvar(v);
	  }
	for (unsigned n = 0; n < d.cand_nacc; ++n)
	  {
	    bdd c = bdd_exist(allneg, d.cand_acc[n]) & d.cand_acc[n];
	    d.cand_acc[n] = c;

	    size_t s = d.all_cand_acc.size();
	    for (size_t i = 0; i < s; ++i)
	      d.all_cand_acc.push_back(d.all_cand_acc[i] | c);
	  }
	d.cand_all_acc = bdd_support(allneg);
	d.ref_all_acc = bdd_support(aut->all_acceptance_conditions());

	bdd refall = d.ref_all_acc;
	bdd refnegall = aut->neg_acceptance_conditions();

	d.all_ref_acc.push_back(bddfalse);
	while (refall != bddtrue)
	  {
	    bdd v = bdd_ithvar(bdd_var(refall));
	    bdd c = bdd_exist(refnegall, v) & v;

	    size_t s = d.all_ref_acc.size();
	    for (size_t i = 0; i < s; ++i)
	      d.all_ref_acc.push_back(d.all_ref_acc[i] | c);

	    refall = bdd_high(refall);
	  }
      }

      int size()
      {
	return size_;
      }

      void end()
      {
	size_ = seen.size();

	if (d.cand_size == -1)
	  d.cand_size = size_ - 1;

	int seen_size = seen.size();
	for (int i = 1; i <= seen_size; ++i)
	  {
	    for (int j = 1; j <= d.cand_size; ++j)
	      {
		d.prodid[state_pair(j, i)] = ++d.nvars;


		for (int k = 1; k <= seen_size; ++k)
		  for (int l = 1; l <= d.cand_size; ++l)
		    {
		      size_t sf = d.all_cand_acc.size();
		      for (size_t f = 0; f < sf; ++f)
			{
			  size_t sfp = d.all_ref_acc.size();
			  for (size_t fp = 0; fp < sfp; ++fp)
			    {
			      path p(j, i, l, k,
				     d.all_cand_acc[f],
				     d.all_ref_acc[fp]);
			      d.pathid[p] = ++d.nvars;
			    }

			}
		    }
	      }
	  }

	for (dict::state_map::const_iterator i = seen.begin();
	     i != seen.end(); ++i)
	  d.int_to_state[i->second] = i->first;

	std::swap(d.state_to_int, seen);

	if (!state_based_)
	  {
	    for (int i = 1; i <= d.cand_size; ++i)
	      for (int j = 1; j <= d.cand_size; ++j)
		{
		  bdd all = bddtrue;
		  while (all != bddfalse)
		    {
		      bdd one = bdd_satoneset(all, ap_, bddfalse);
		      all -= one;

		      transition t(i, one, j);
		      d.transid[t] = ++d.nvars;
		      d.revtransid.insert(dict::rev_map::
					  value_type(d.nvars, t));

		      // Create the variable for the accepting transition
		      // immediately afterwards.  It helps parsing the
		      // result.
		      for (unsigned n = 0; n < d.cand_nacc; ++n)
			{
			  transition_acc ta(i, one, d.cand_acc[n], j);
			  d.transaccid[ta] = ++d.nvars;
			  d.revtransaccid.insert(dict::rev_acc_map::
						 value_type(d.nvars, ta));
			}
		    }
		}
	  }
	else // state based
	  {
	    for (int i = 1; i <= d.cand_size; ++i)
	      for (unsigned n = 0; n < d.cand_nacc; ++n)
		{
		  ++d.nvars;
		  for (int j = 1; j <= d.cand_size; ++j)
		    {
		      bdd all = bddtrue;
		      while (all != bddfalse)
			{
			  bdd one = bdd_satoneset(all, ap_, bddfalse);
			  all -= one;

			  transition_acc ta(i, one, d.cand_acc[n], j);
			  d.transaccid[ta] = d.nvars;
			  d.revtransaccid.insert(dict::rev_acc_map::
						 value_type(d.nvars, ta));
			}
		    }
		}
	    for (int i = 1; i <= d.cand_size; ++i)
	      for (int j = 1; j <= d.cand_size; ++j)
		{
		  bdd all = bddtrue;
		  while (all != bddfalse)
		    {
		      bdd one = bdd_satoneset(all, ap_, bddfalse);
		      all -= one;

		      transition t(i, one, j);
		      d.transid[t] = ++d.nvars;
		      d.revtransid.insert(dict::rev_map::
					  value_type(d.nvars, t));
		    }
		}
	  }
      }
    };

    static
    void dtgba_to_sat(std::ostream& out, const tgba* ref, dict& d,
		      bool state_based)
    {
      int nclauses = 0;
      int ref_size = 0;

      scc_map sm(ref);
      sm.build_map();
      bdd ap = sm.aprec_set_of(sm.initial());


      // Number all the SAT variable we may need.
      {
	filler_dfs f(ref, d, ap, state_based);
	f.run();
	ref_size = f.size();
      }

#if DEBUG
      debug_dict = ref->get_dict();
      dout << "ref_size: " << ref_size << "\n";
      dout << "cand_size: " << d.cand_size << "\n";
#endif

      // empty automaton is impossible
      if (d.cand_size == 0)
	{
	  out << "p cnf 1 2\n-1 0\n1 0\n";
	  return;
	}

      // An empty line for the header
      out << "                                                 \n";

      dout << "(8) the candidate automaton is complete\n";
      for (int q1 = 1; q1 <= d.cand_size; ++q1)
	{
	  bdd all = bddtrue;
	  while (all != bddfalse)
	    {
	      bdd s = bdd_satoneset(all, ap, bddfalse);
	      all -= s;

#if DEBUG
	      dout;
	      for (int q2 = 1; q2 <= d.cand_size; q2++)
		{
		  transition t(q1, s, q2);
		  out << t << "δ";
		  if (q2 != d.cand_size)
		    out << " ∨ ";
		}
	      out << "\n";
#endif

	      for (int q2 = 1; q2 <= d.cand_size; q2++)
		{
		  transition t(q1, s, q2);
		  int ti = d.transid[t];

		  out << ti << " ";
		}
	      out << "0\n";
	      ++nclauses;
	    }
	}

      dout << "(9) the initial state is reachable\n";
      dout << state_pair(1, 1) << "\n";
      out << d.prodid[state_pair(1, 1)] << " 0\n";
      ++nclauses;

      for (std::map<state_pair, int>::const_iterator pit = d.prodid.begin();
	   pit != d.prodid.end(); ++pit)
	{
	  int q1 = pit->first.a;
	  int q1p = pit->first.b;

	  dout << "(9) states Cand[" << q1 << "] and Ref[" << q1p
	       << "] are 0-length paths\n";
	  path p(q1, q1p, q1, q1p, bddfalse, bddfalse);
	  dout << pit->first << " → " << p << "\n";
	  out << -pit->second << " " << d.pathid[p] <<" 0\n";
	  ++nclauses;

	  dout << "(10) augmenting paths based on Cand[" << q1
	       << "] and Ref[" << q1p << "]\n";
	  tgba_succ_iterator* it = ref->succ_iter(d.int_to_state[q1p]);
	  for (it->first(); !it->done(); it->next())
	    {
	      const state* dps = it->current_state();
	      int dp = d.state_to_int[dps];
	      dps->destroy();

	      bdd all = it->current_condition();
	      while (all != bddfalse)
		{
		  bdd s = bdd_satoneset(all, ap, bddfalse);
		  all -= s;

		  for (int q2 = 1; q2 <= d.cand_size; q2++)
		    {
		      transition t(q1, s, q2);
		      int ti = d.transid[t];

		      state_pair p2(q2, dp);
		      int succ = d.prodid[p2];

		      if (pit->second == succ)
			continue;

		      dout << pit->first << " ∧ " << t << "δ → " << p2 << "\n";
		      out << -pit->second << " " << -ti << " "
			  << succ << " 0\n";
		      ++nclauses;
		    }
		}
	    }
	  delete it;
	}

      bdd all_acc = ref->all_acceptance_conditions();

      // construction of constraints (11,12,13)
      for (int q1 = 1; q1 <= d.cand_size; ++q1)
	for (int q1p = 1; q1p <= ref_size; ++q1p)
	  for (int q2 = 1; q2 <= d.cand_size; ++q2)
	    for (int q2p = 1; q2p <= ref_size; ++q2p)
	      {
		size_t sf = d.all_cand_acc.size();
		size_t sfp = d.all_ref_acc.size();
		for (size_t f = 0; f < sf; ++f)
		  for (size_t fp = 0; fp < sfp; ++fp)
		    {
		      path p(q1, q1p, q2, q2p,
			     d.all_cand_acc[f], d.all_ref_acc[fp]);

		      dout << "(11&12&13) paths from " << p << "\n";

		      int pid = d.pathid[p];

		      tgba_succ_iterator* it =
			ref->succ_iter(d.int_to_state[q2p]);

		      for (it->first(); !it->done(); it->next())
			{
			  const state* dps = it->current_state();
			  int dp = d.state_to_int[dps];
			  dps->destroy();

			  for (int q3 = 1; q3 <= d.cand_size; ++q3)
			    {
			      bdd all = it->current_condition();
			      bdd curacc = it->current_acceptance_conditions();

			      while (all != bddfalse)
				{
				  bdd l = bdd_satoneset(all, ap, bddfalse);
				  all -= l;

				  transition t(q2, l, q3);
				  int ti = d.transid[t];

				  if (dp == q1p && q3 == q1) // (11,12) loop
				    {
				      bdd unio =  curacc | d.all_ref_acc[fp];
				      if (unio != all_acc)
					{
#if DEBUG
					  dout << "(11) " << p << " ∧ "
					       << t << "δ → ¬(";

					  bdd all_ = d.all_cand_acc.back();
					  all_ -= d.all_cand_acc[f];
					  bool notfirst = false;
					  while (all_ != bddfalse)
					    {
					      bdd one = bdd_satone(all_);
					      all_ -= one;

					      transition_acc ta(q2, l,
								one, q1);
					      if (notfirst)
						out << " ∧ ";
					      else
						notfirst = true;
					      out << ta << "FC";
					    }
					  out << ")\n";
#endif // DEBUG
					  out << -pid << " " << -ti;

					  // 11
					  bdd all_f = d.all_cand_acc.back();
					  all_f -= d.all_cand_acc[f];
					  while (all_f != bddfalse)
					    {
					      bdd one = bdd_satone(all_f);
					      all_f -= one;

					      transition_acc ta(q2, l,
								one, q1);
					      int tai = d.transaccid[ta];
					      assert(tai != 0);
					      out << " " << -tai;
					    }
					  out << " 0\n";
					  ++nclauses;
					}
				      else
					{
#if DEBUG
					  dout << "(12) " << p << " ∧ "
					       << t << "δ → (";

					  bdd all_ = d.all_cand_acc.back();
					  all_ -= d.all_cand_acc[f];
					  bool notfirst = false;
					  while (all_ != bddfalse)
					    {
					      bdd one = bdd_satone(all_);
					      all_ -= one;

					      transition_acc ta(q2, l,
								one, q1);
					      if (notfirst)
						out << " ∧ ";
					      else
						notfirst = true;
					      out << ta << "FC";
					    }
					  out << ")\n";
#endif // DEBUG
					  // 12
					  bdd all_f = d.all_cand_acc.back();
					  all_f -= d.all_cand_acc[f];
					  while (all_f != bddfalse)
					    {
					      bdd one = bdd_satone(all_f);
					      all_f -= one;

					      transition_acc ta(q2, l,
								one, q1);
					      int tai = d.transaccid[ta];
					      assert(tai != 0);

					      out << -pid << " " << -ti
						  << " " << tai << " 0\n";
					      ++nclauses;
					    }
					  // out << -pid << " " << -ti
					  //     << " " << pcompid << " 0\n";
					  // ++nclauses;
					}
				    }
				  // (13) augmenting paths (always).
				  {
				    size_t sf = d.all_cand_acc.size();
				    for (size_t f = 0; f < sf; ++f)
				      {

					bdd f2 = p.acc_cand |
					  d.all_cand_acc[f];
					bdd f2p = p.acc_ref | curacc;

					path p2(p.src_cand, p.src_ref,
						q3, dp, f2, f2p);
					int p2id = d.pathid[p2];
					if (pid == p2id)
					  continue;
#if DEBUG
					dout << "(13) " << p << " ∧ "
					     << t << "δ ";

					bdd biga_ = d.all_cand_acc[f];
					while (biga_ != bddfalse)
					  {
					    bdd a = bdd_satone(biga_);
					    biga_ -= a;

					    transition_acc ta(q2, l, a, q3);
					    out <<  " ∧ " << ta << "FC";
					  }
					biga_ = d.all_cand_acc.back()
					  - d.all_cand_acc[f];
					while (biga_ != bddfalse)
					  {
					    bdd a = bdd_satone(biga_);
					    biga_ -= a;

					    transition_acc ta(q2, l, a, q3);
					    out << " ∧ ¬" << ta << "FC";
					  }
					out << " → " << p2 << "\n";
#endif
					out << -pid << " " << -ti << " ";
					bdd biga = d.all_cand_acc[f];
					while (biga != bddfalse)
					  {
					    bdd a = bdd_satone(biga);
					    biga -= a;

					    transition_acc ta(q2, l, a, q3);
					    int tai = d.transaccid[ta];
					    out << -tai << " ";
					  }
					biga = d.all_cand_acc.back()
					  - d.all_cand_acc[f];
					while (biga != bddfalse)
					  {
					    bdd a = bdd_satone(biga);
					    biga -= a;

					    transition_acc ta(q2, l, a, q3);
					    int tai = d.transaccid[ta];
					    out << tai << " ";
					  }

					out << p2id << " 0\n";
					++nclauses;
				      }
				  }
				}
			    }
			}
		      delete it;
		    }
	      }

      out.seekp(0);
      out << "p cnf " << d.nvars << " " << nclauses;
    }

    static tgba_explicit_number*
    sat_build(const sat_solution& solution, dict& satdict, const tgba* aut,
	      bool state_based)
    {
      bdd_dict* autdict = aut->get_dict();
      tgba_explicit_number* a = new tgba_explicit_number(autdict);
      autdict->register_all_variables_of(aut, a);
      autdict->unregister_all_typed_variables(bdd_dict::acc, aut);
      a->set_acceptance_conditions(satdict.all_cand_acc.back());

      for (int s = 1; s < satdict.cand_size; ++s)
	a->add_state(s);

      state_explicit_number::transition* last_aut_trans = 0;
      const transition* last_sat_trans = 0;

#if DEBUG
      std::fstream out("dtgba-sat.dbg",
		       std::ios_base::trunc | std::ios_base::out);
      std::set<int> positive;
#endif

      dout << "--- transition variables ---\n";
      std::map<int, bdd> state_acc;
      for (sat_solution::const_iterator i = solution.begin();
	   i != solution.end(); ++i)
	{
	  int v = *i;

	  if (v < 0)  // FIXME: maybe we can have (v < NNN)?
	    continue;

#if DEBUG
	  positive.insert(v);
#endif

	  dict::rev_map::const_iterator t = satdict.revtransid.find(v);

	  if (t != satdict.revtransid.end())
	    {
	      last_aut_trans = a->create_transition(t->second.src,
						    t->second.dst);
	      last_aut_trans->condition = t->second.cond;
	      last_sat_trans = &t->second;

	      dout << v << "\t" << t->second << "δ\n";

	      if (state_based)
		{
		  std::map<int, bdd>::const_iterator i =
		    state_acc.find(t->second.src);
		  if (i != state_acc.end())
		    last_aut_trans->acceptance_conditions = i->second;
		}
	    }
	  else
	    {
	      dict::rev_acc_map::const_iterator ta;
	      ta = satdict.revtransaccid.find(v);
	      // This assumes that the sat solvers output variables in
	      // increasing order.
	      if (ta != satdict.revtransaccid.end())
		{
		  dout << v << "\t" << ta->second << "F\n";

		  if (last_sat_trans &&
		      ta->second.src == last_sat_trans->src &&
		      ta->second.cond == last_sat_trans->cond &&
		      ta->second.dst == last_sat_trans->dst)
		    {
		      assert(!state_based);
		      last_aut_trans->acceptance_conditions |= ta->second.acc;
		    }
		  else if (state_based)
		    {
		      state_acc[ta->second.src] |= ta->second.acc;
		    }
		}
	    }
	}
#if DEBUG
      dout << "--- state_pair variables ---\n";
      for (std::map<state_pair, int>::const_iterator pit =
	     satdict.prodid.begin(); pit != satdict.prodid.end(); ++pit)
	if (positive.find(pit->second) != positive.end())
	  dout << pit->second << "\t" << pit->first << "\n";

      dout << "--- pathit variables ---\n";
      for (std::map<path, int>::const_iterator pit =
	     satdict.pathid.begin();
	   pit != satdict.pathid.end(); ++pit)
	if (positive.find(pit->second) != positive.end())
	  dout << pit->second << "\t" << pit->first << "C\n";
#endif

      a->merge_transitions();

      return a;
    }

    static bool
    xrename(const char* from, const char* to)
    {
      if (!rename(from, to))
	return false;
      std::ostringstream msg;
      msg << "cannot rename " << from << " to " << to;
      perror(msg.str().c_str());
      return true;
    }
  }

  tgba_explicit_number*
  dtgba_sat_synthetize(const tgba* a, unsigned target_acc_number,
		       int target_state_number, bool state_based)
  {
    trace << "dtgba_sat_synthetize(..., acc = " << target_acc_number
	  << ", states = " << target_state_number
	  << ", state_based = " << state_based << ")\n";
    dict* current = 0;
    temporary_file* cnf = 0;
    temporary_file* out = 0;

    current = new dict(a);
    current->cand_size = target_state_number;
    current->cand_nacc = target_acc_number;

    cnf = create_tmpfile("dtgba-sat-", ".cnf");
    std::fstream cnfs(cnf->name(),
		      std::ios_base::trunc | std::ios_base::out);
    dtgba_to_sat(cnfs, a, *current, state_based);
    cnfs.close();

    out = create_tmpfile("dtgba-sat-", ".out");
    satsolver(cnf, out);

    sat_solution solution = satsolver_get_solution(out->name());

    tgba_explicit_number* res = 0;
    if (!solution.empty())
      res = sat_build(solution, *current, a, state_based);

    delete current;

    if (DEBUG)
      {
	xrename(out->name(), "dtgba-sat.out");
	xrename(cnf->name(), "dtgba-sat.cnf");
      }

    delete out;
    delete cnf;
    trace << "dtgba_sat_synthetize(...) = " << res << "\n";
    return res;
  }

  tgba_explicit_number*
  dtgba_sat_minimize(const tgba* a, unsigned target_acc_number,
		     bool state_based)
  {
    int n_states = stats_reachable(a).states;

    tgba_explicit_number* prev = 0;
    for (;;)
      {
	tgba_explicit_number* next =
	  dtgba_sat_synthetize(prev ? prev : a, target_acc_number,
			       --n_states, state_based);
	if (next == 0)
	  break;
	delete prev;
	prev = next;
      }
    return prev;
  }

  tgba_explicit_number*
  dtgba_sat_minimize_dichotomy(const tgba* a, unsigned target_acc_number,
			       bool state_based)
  {
    int max_states = stats_reachable(a).states - 1;
    int min_states = 1;

    tgba_explicit_number* prev = 0;
    while (min_states <= max_states)
      {
	int target = (max_states + min_states) / 2;
	tgba_explicit_number* next =
	  dtgba_sat_synthetize(prev ? prev : a, target_acc_number, target,
			       state_based);
	if (next == 0)
	  {
	    min_states = target + 1;
	  }
	else
	  {
	    delete prev;
	    prev = next;
	    max_states = target - 1;
	  }
      }
    return prev;
  }

}