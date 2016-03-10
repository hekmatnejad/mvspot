// -*- coding: utf-8 -*-
// Copyright (C) 2011, 2012, 2014, 2015, 2016 Laboratoire de Recherche et
// Developpement de l'Epita (LRDE).
// Copyright (C) 2003, 2004  Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
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

#include <ostream>
#include <sstream>
#include <cstring>
#include <map>
#include <spot/twa/twa.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twaalgos/reachiter.hh>
#include <spot/misc/escape.hh>
#include <spot/misc/bddlt.hh>
#include <spot/misc/minato.hh>
#include <spot/twa/formula2bdd.hh>
#include <spot/tl/formula.hh>
#include <spot/kripke/fairkripke.hh>

namespace spot
{
  namespace
  {
    struct metadata final
    {
      // Assign a number to each atomic proposition.
      typedef std::map<int, unsigned> ap_map;
      ap_map ap;
      typedef std::vector<int> vap_t;
      vap_t vap;

      std::vector<bool> common_acc;
      bool has_state_acc;
      bool is_complete;
      bool is_deterministic;
      bool is_colored;
      bool use_implicit_labels;
      bool use_state_labels = true;
      bdd all_ap;

      // Label support: the set of all conditions occurring in the
      // automaton.
      typedef std::map<bdd, std::string, bdd_less_than> sup_map;
      sup_map sup;

      metadata(const const_twa_graph_ptr& aut, bool implicit,
               bool state_labels)
      {
        check_det_and_comp(aut);
        use_implicit_labels = implicit && is_deterministic && is_complete;
        use_state_labels &= state_labels;
        number_all_ap();
      }

      std::ostream&
      emit_acc(std::ostream& os, acc_cond::mark_t b)
      {
        // FIXME: We could use a cache for this.
        if (b == 0U)
          return os;
        os << " {";
        bool notfirst = false;
        for (auto v: b.sets())
          {
            if (notfirst)
              os << ' ';
            else
              notfirst = true;
            os << v;
          }
        os << '}';
        return os;
      }

      void check_det_and_comp(const const_twa_graph_ptr& aut)
      {
        std::string empty;

        unsigned ns = aut->num_states();
        bool deterministic = true;
        bool complete = true;
        bool state_acc = true;
        bool nodeadend = true;
        bool colored = aut->num_sets() >= 1;
        for (unsigned src = 0; src < ns; ++src)
          {
            bdd sum = bddfalse;
            bdd available = bddtrue;
            bool st_acc = true;
            bool notfirst = false;
            acc_cond::mark_t prev = 0U;
            bool has_succ = false;
            bdd lastcond = bddfalse;
            for (auto& t: aut->out(src))
              {
                if (has_succ)
                  use_state_labels &= lastcond == t.cond;
                else
                  lastcond = t.cond;
                if (complete)
                  sum |= t.cond;
                if (deterministic)
                  {
                    if (!bdd_implies(t.cond, available))
                      deterministic = false;
                    else
                      available -= t.cond;
                  }
                sup.insert(std::make_pair(t.cond, empty));
                if (st_acc)
                  {
                    if (notfirst && prev != t.acc)
                      {
                        st_acc = false;
                      }
                    else
                      {
                        notfirst = true;
                        prev = t.acc;
                      }
                  }
                if (colored)
                  {
                    auto a = t.acc;
                    if (!a || a.remove_some(1))
                      colored = false;
                  }
                has_succ = true;
              }
            nodeadend &= has_succ;
            if (complete)
              complete &= sum == bddtrue;
            common_acc.push_back(st_acc);
            state_acc &= st_acc;
          }
        is_deterministic = deterministic;
        is_complete = complete;
        has_state_acc = state_acc;
        // If the automaton has state-based acceptance and contain
        // some states without successors do not declare it as
        // colored.
        is_colored = colored && (!has_state_acc || nodeadend);
        // If the automaton declares that it is deterministic or
        // state-based, make sure that it really is.
        assert(deterministic || aut->prop_deterministic() != true);
        assert(state_acc || aut->prop_state_acc() != true);
      }

      void number_all_ap()
      {
        bdd all = bddtrue;
        for (auto& i: sup)
          all &= bdd_support(i.first);
        all_ap = all;

        while (all != bddtrue)
          {
            int v = bdd_var(all);
            all = bdd_high(all);
            ap.insert(std::make_pair(v, vap.size()));
            vap.push_back(v);
          }

        if (use_implicit_labels)
          return;

        for (auto& i: sup)
          {
            bdd cond = i.first;
            if (cond == bddtrue)
              {
                i.second = "t";
                continue;
              }
            if (cond == bddfalse)
              {
                i.second = "f";
                continue;
              }
            std::ostringstream s;
            bool notfirstor = false;

            minato_isop isop(cond);
            bdd cube;
            while ((cube = isop.next()) != bddfalse)
              {
                if (notfirstor)
                  s << " | ";
                bool notfirstand = false;
                while (cube != bddtrue)
                  {
                    if (notfirstand)
                      s << '&';
                    else
                      notfirstand = true;
                    bdd h = bdd_high(cube);
                    if (h == bddfalse)
                      {
                        s << '!' << ap[bdd_var(cube)];
                        cube = bdd_low(cube);
                      }
                    else
                      {
                        s << ap[bdd_var(cube)];
                        cube = h;
                      }
                  }
                notfirstor = true;
              }
            i.second = s.str();
          }
      }
    };

  }

  enum hoa_acceptance
    {
      Hoa_Acceptance_States,    /// state-based acceptance if
                                /// (globally) possible
                                /// transition-based acceptance
                                /// otherwise.
      Hoa_Acceptance_Transitions, /// transition-based acceptance globally
      Hoa_Acceptance_Mixed    /// mix state-based and transition-based
    };

  static std::ostream&
  print_hoa(std::ostream& os,
                const const_twa_graph_ptr& aut,
                const char* opt)
  {
    bool newline = true;
    hoa_acceptance acceptance = Hoa_Acceptance_States;
    bool implicit_labels = false;
    bool verbose = false;
    bool state_labels = false;

    if (opt)
      while (*opt)
        {
          switch (char c = *opt++)
            {
            case 'i':
              implicit_labels = true;
              break;
            case 'k':
              state_labels = true;
              break;
            case 'l':
              newline = false;
              break;
            case 'm':
              acceptance = Hoa_Acceptance_Mixed;
              break;
            case 's':
              acceptance = Hoa_Acceptance_States;
              break;
            case 't':
              acceptance = Hoa_Acceptance_Transitions;
              break;
            case 'v':
              verbose = true;
              break;
            default:
              throw std::runtime_error
                (std::string("unknown option for print_hoa(): ") + c);
            }
        }

    // Calling get_init_state_number() may add a state to empty
    // automata, so it has to be done first.
    unsigned init = aut->get_init_state_number();

    metadata md(aut, implicit_labels, state_labels);

    if (acceptance == Hoa_Acceptance_States && !md.has_state_acc)
      acceptance = Hoa_Acceptance_Transitions;

    unsigned num_states = aut->num_states();

    const char nl = newline ? '\n' : ' ';
    os << "HOA: v1" << nl;
    auto n = aut->get_named_prop<std::string>("automaton-name");
    if (n)
      escape_str(os << "name: \"", *n) << '"' << nl;
    unsigned nap = md.vap.size();
    os << "States: " << num_states << nl
       << "Start: " << init << nl
       << "AP: " << nap;
    auto d = aut->get_dict();
    for (auto& i: md.vap)
      escape_str(os << " \"", d->bdd_map[i].f.ap_name()) << '"';
    os << nl;

    unsigned num_acc = aut->num_sets();
    acc_cond::acc_code acc_c = aut->acc().get_acceptance();
    if (aut->acc().is_generalized_buchi())
      {
        if (aut->acc().is_all())
          os << "acc-name: all";
        else if (aut->acc().is_buchi())
          os << "acc-name: Buchi";
        else
          os << "acc-name: generalized-Buchi " << num_acc;
        os << nl;
      }
    else if (aut->acc().is_generalized_co_buchi())
      {
        if (aut->acc().is_none())
          os << "acc-name: none";
        else if (aut->acc().is_co_buchi())
          os << "acc-name: co-Buchi";
        else
          os << "acc-name: generalized-co-Buchi " << num_acc;
        os << nl;
      }
    else
      {
        int r = aut->acc().is_rabin();
        assert(r != 0);
        if (r > 0)
          {
            os << "acc-name: Rabin " << r << nl;
            // Force the acceptance to remove any duplicate sets, and
            // make sure it is correctly ordered.
            acc_c = acc_cond::acc_code::rabin(r);
          }
        else
          {
            r = aut->acc().is_streett();
            assert(r != 0);
            if (r > 0)
              {
                os << "acc-name: Streett " << r << nl;
                // Force the acceptance to remove any duplicate sets, and
                // make sure it is correctly ordered.
                acc_c = acc_cond::acc_code::streett(r);
              }
            else
              {
                std::vector<unsigned> pairs;
                if (aut->acc().is_generalized_rabin(pairs))
                  {
                    os << "acc-name: generalized-Rabin " << pairs.size();
                    for (auto p: pairs)
                      os << ' ' << p;
                    os << nl;
                    // Force the acceptance to remove any duplicate
                    // sets, and make sure it is correctly ordered.
                    acc_c = acc_cond::acc_code::generalized_rabin(pairs.begin(),
                                                                  pairs.end());
                  }
                else
                  {
                    bool max = false;
                    bool odd = false;
                    if (aut->acc().is_parity(max, odd))
                      os << "acc-name: parity "
                         << (max ? "max " : "min ")
                         << (odd ? "odd " : "even ")
                         << num_acc << nl;
                  }
              }
          }
      }
    os << "Acceptance: " << num_acc << ' ';
    os << acc_c;
    os << nl;
    os << "properties:";
    // Make sure the property line is not too large,
    // otherwise our test cases do not fit in 80 columns...
    unsigned prop_len = 60;
    auto prop = [&](const char* str)
      {
        if (newline)
          {
            auto l = strlen(str);
            if (prop_len < l)
              {
                prop_len = 60;
                os << "\nproperties:";
              }
            prop_len -= l;
          }
        os << str;
      };
    // We do not support alternating automata so far, and it's
    // probable that nobody cares about the "no-univ-branch"
    // properties.  The "univ-branch" properties seems more important
    // to announce that the automaton might not be parsable by tools
    // that do not support alternating automata.
    if (verbose)
      prop(" no-univ-branch");
    implicit_labels = md.use_implicit_labels;
    state_labels = md.use_state_labels;
    if (implicit_labels)
      prop(" implicit-labels");
    else if (state_labels)
      prop(" state-labels explicit-labels");
    else
      prop(" trans-labels explicit-labels");
    if (acceptance == Hoa_Acceptance_States)
      prop(" state-acc");
    else if (acceptance == Hoa_Acceptance_Transitions)
      prop(" trans-acc");
    if (md.is_colored)
      prop(" colored");
    if (md.is_complete)
      prop(" complete");
    if (md.is_deterministic)
      prop(" deterministic");
    // Deterministic automata are also unambiguous, so writing both
    // properties seems redundant.  People working on unambiguous
    // automata are usually concerned about non-deterministic
    // unambiguous automata.  So do not mention "unambiguous"
    // in the case of deterministic automata.
    if (aut->prop_unambiguous() && (verbose || !md.is_deterministic))
      prop(" unambiguous");
    if (aut->prop_stutter_invariant())
      prop(" stutter-invariant");
    if (!aut->prop_stutter_invariant())
      prop(" stutter-sensitive");
    if (aut->prop_terminal())
      prop(" terminal");
    if (aut->prop_weak() && (verbose || aut->prop_terminal() != true))
      prop(" weak");
    if (aut->prop_inherently_weak() && (verbose || aut->prop_weak() != true))
      prop(" inherently-weak");
    os << nl;

    // If we want to output implicit labels, we have to
    // fill a vector with all destinations in order.
    std::vector<unsigned> out;
    std::vector<acc_cond::mark_t> outm;
    if (implicit_labels)
      {
        out.resize(1UL << nap);
        if (acceptance != Hoa_Acceptance_States)
          outm.resize(1UL << nap);
      }

    os << "--BODY--" << nl;
    auto sn = aut->get_named_prop<std::vector<std::string>>("state-names");
    for (unsigned i = 0; i < num_states; ++i)
      {
        hoa_acceptance this_acc = acceptance;
        if (this_acc == Hoa_Acceptance_Mixed)
          this_acc = (md.common_acc[i] ?
                      Hoa_Acceptance_States : Hoa_Acceptance_Transitions);

        os << "State: ";
        if (state_labels)
          {
            bool output = false;
            for (auto& t: aut->out(i))
              {
                os << '[' << md.sup[t.cond] << "] ";
                output = true;
                break;
              }
            if (!output)
              os << "[f] ";
          }
        os << i;
        if (sn && i < sn->size() && !(*sn)[i].empty())
          os << " \"" << (*sn)[i] << '"';
        if (this_acc == Hoa_Acceptance_States)
          {
            acc_cond::mark_t acc = 0U;
            for (auto& t: aut->out(i))
              {
                acc = t.acc;
                break;
              }
            md.emit_acc(os, acc);
          }
        os << nl;

        if (!implicit_labels && !state_labels)
          {

            for (auto& t: aut->out(i))
              {
                os << '[' << md.sup[t.cond] << "] " << t.dst;
                if (this_acc == Hoa_Acceptance_Transitions)
                  md.emit_acc(os, t.acc);
                os << nl;
              }
          }
        else if (state_labels)
          {
            unsigned n = 0;
            for (auto& t: aut->out(i))
              {
                os << t.dst;
                if (this_acc == Hoa_Acceptance_Transitions)
                  {
                    md.emit_acc(os, t.acc);
                    os << nl;
                  }
                else
                  {
                    ++n;
                    os << (((n & 15) && t.next_succ) ? ' ' : nl);
                  }
              }
          }
        else
          {
            for (auto& t: aut->out(i))
              {
                bdd cond = t.cond;
                while (cond != bddfalse)
                  {
                    bdd one = bdd_satoneset(cond, md.all_ap, bddfalse);
                    cond -= one;
                    unsigned level = 1;
                    unsigned pos = 0U;
                    while (one != bddtrue)
                      {
                        bdd h = bdd_high(one);
                        if (h == bddfalse)
                          {
                            one = bdd_low(one);
                          }
                        else
                          {
                            pos |= level;
                            one = h;
                          }
                        level <<= 1;
                      }
                    out[pos] = t.dst;
                    if (this_acc != Hoa_Acceptance_States)
                      outm[pos] = t.acc;
                  }
              }
            unsigned n = out.size();
            for (unsigned i = 0; i < n;)
              {
                os << out[i];
                if (this_acc != Hoa_Acceptance_States)
                  {
                    md.emit_acc(os, outm[i]) << nl;
                    ++i;
                  }
                else
                  {
                    ++i;
                    os << (((i & 15) && i < n) ? ' ' : nl);
                  }
              }
          }
      }
    os << "--END--";            // No newline.  Let the caller decide.
    return os;
  }

  std::ostream&
  print_hoa(std::ostream& os,
            const const_twa_ptr& aut,
            const char* opt)
  {

    auto a = std::dynamic_pointer_cast<const twa_graph>(aut);
    if (!a)
      a = make_twa_graph(aut, twa::prop_set::all());

    // for Kripke structures, automatically append "k" to the options.
    char* tmpopt = nullptr;
    if (std::dynamic_pointer_cast<const fair_kripke>(aut))
      {
        unsigned n = opt ? strlen(opt) : 0;
        tmpopt = new char[n + 2];
        if (opt)
          strcpy(tmpopt, opt);
        tmpopt[n] = 'k';
        tmpopt[n + 1] = 0;
      }
    print_hoa(os, a, tmpopt ? tmpopt : opt);
    delete[] tmpopt;
    return os;
  }

}
