// -*- coding: utf-8 -*-
// Copyright (C) 2013, 2014, 2015, 2016 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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

#include "common_sys.hh"

#include <string>
#include <iostream>
#include <limits>
#include <set>
#include <memory>

#include <argp.h>
#include "error.h"

#include "common_setup.hh"
#include "common_finput.hh"
#include "common_cout.hh"
#include "common_aoutput.hh"
#include "common_range.hh"
#include "common_post.hh"
#include "common_conv.hh"
#include "common_hoaread.hh"

#include <spot/twaalgos/product.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/stutter.hh>
#include <spot/twaalgos/isunamb.hh>
#include <spot/misc/optionmap.hh>
#include <spot/misc/timer.hh>
#include <spot/misc/random.hh>
#include <spot/parseaut/public.hh>
#include <spot/tl/exclusive.hh>
#include <spot/twaalgos/remprop.hh>
#include <spot/twaalgos/randomize.hh>
#include <spot/twaalgos/are_isomorphic.hh>
#include <spot/twaalgos/canonicalize.hh>
#include <spot/twaalgos/mask.hh>
#include <spot/twaalgos/sepsets.hh>
#include <spot/twaalgos/stripacc.hh>
#include <spot/twaalgos/remfin.hh>
#include <spot/twaalgos/cleanacc.hh>
#include <spot/twaalgos/dtwasat.hh>
#include <spot/twaalgos/complement.hh>
#include <spot/twaalgos/strength.hh>

static const char argp_program_doc[] ="\
Convert, transform, and filter omega-automata.\v\
Exit status:\n\
  0  if some automata were output\n\
  1  if no automata were output (no match)\n\
  2  if any error has been reported";

// Keep this list sorted
enum {
  OPT_ACC_SETS = 256,
  OPT_ACC_WORD,
  OPT_ARE_ISOMORPHIC,
  OPT_CLEAN_ACC,
  OPT_CNF_ACC,
  OPT_COMPLEMENT,
  OPT_COMPLEMENT_ACC,
  OPT_COUNT,
  OPT_DECOMPOSE_STRENGTH,
  OPT_DESTUT,
  OPT_DNF_ACC,
  OPT_EDGES,
  OPT_EXCLUSIVE_AP,
  OPT_GENERIC,
  OPT_INSTUT,
  OPT_INTERSECT,
  OPT_IS_COMPLETE,
  OPT_IS_DETERMINISTIC,
  OPT_IS_EMPTY,
  OPT_IS_UNAMBIGUOUS,
  OPT_IS_TERMINAL,
  OPT_IS_WEAK,
  OPT_IS_INHERENTLY_WEAK,
  OPT_KEEP_STATES,
  OPT_MASK_ACC,
  OPT_MERGE,
  OPT_PRODUCT_AND,
  OPT_PRODUCT_OR,
  OPT_RANDOMIZE,
  OPT_REM_AP,
  OPT_REM_DEAD,
  OPT_REM_UNREACH,
  OPT_REM_FIN,
  OPT_SAT_MINIMIZE,
  OPT_SEED,
  OPT_SEP_SETS,
  OPT_SIMPLIFY_EXCLUSIVE_AP,
  OPT_STATES,
  OPT_STRIPACC,
};

static const argp_option options[] =
  {
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Input:", 1 },
    { "file", 'F', "FILENAME", 0,
      "process the automaton in FILENAME", 0 },
    /**************************************************/
    { "count", 'c', nullptr, 0, "print only a count of matched automata", 3 },
    { "max-count", 'n', "NUM", 0, "output at most NUM automata", 3 },
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Transformations:", 5 },
    { "merge-transitions", OPT_MERGE, nullptr, 0,
      "merge transitions with same destination and acceptance", 0 },
    { "product", OPT_PRODUCT_AND, "FILENAME", 0,
      "build the product with the automaton in FILENAME "
      "to intersect languages", 0 },
    { "product-and", 0, nullptr, OPTION_ALIAS, nullptr, 0 },
    { "product-or", OPT_PRODUCT_OR, "FILENAME", 0,
      "build the product with the automaton in FILENAME "
      "to sum languages", 0 },
    { "randomize", OPT_RANDOMIZE, "s|t", OPTION_ARG_OPTIONAL,
      "randomize states and transitions (specify 's' or 't' to "
      "randomize only states or transitions)", 0 },
    { "instut", OPT_INSTUT, "1|2", OPTION_ARG_OPTIONAL,
      "allow more stuttering (two possible algorithms)", 0 },
    { "destut", OPT_DESTUT, nullptr, 0, "allow less stuttering", 0 },
    { "mask-acc", OPT_MASK_ACC, "NUM[,NUM...]", 0,
      "remove all transitions in specified acceptance sets", 0 },
    { "strip-acceptance", OPT_STRIPACC, nullptr, 0,
      "remove the acceptance condition and all acceptance sets", 0 },
    { "keep-states", OPT_KEEP_STATES, "NUM[,NUM...]", 0,
      "only keep specified states.  The first state will be the new "\
      "initial state.  Implies --remove-unreachable-states.", 0 },
    { "dnf-acceptance", OPT_DNF_ACC, nullptr, 0,
      "put the acceptance condition in Disjunctive Normal Form", 0 },
    { "cnf-acceptance", OPT_CNF_ACC, nullptr, 0,
      "put the acceptance condition in Conjunctive Normal Form", 0 },
    { "remove-fin", OPT_REM_FIN, nullptr, 0,
      "rewrite the automaton without using Fin acceptance", 0 },
    { "cleanup-acceptance", OPT_CLEAN_ACC, nullptr, 0,
      "remove unused acceptance sets from the automaton", 0 },
    { "complement", OPT_COMPLEMENT, nullptr, 0,
      "complement each automaton (currently support only deterministic "
      "automata)", 0 },
    { "complement-acceptance", OPT_COMPLEMENT_ACC, nullptr, 0,
      "complement the acceptance condition (without touching the automaton)",
      0 },
    { "decompose-strength", OPT_DECOMPOSE_STRENGTH, "t|w|s", 0,
      "extract the (t) terminal, (w) weak, or (s) strong part of an automaton"
      " (letters may be combined to combine more strengths in the output)", 0 },
    { "exclusive-ap", OPT_EXCLUSIVE_AP, "AP,AP,...", 0,
      "if any of those APs occur in the automaton, restrict all edges to "
      "ensure two of them may not be true at the same time.  Use this option "
      "multiple times to declare independent groups of exclusive "
      "propositions.", 0 },
    { "simplify-exclusive-ap", OPT_SIMPLIFY_EXCLUSIVE_AP, nullptr, 0,
      "if --exclusive-ap is used, assume those AP groups are actually exclusive"
      " in the system to simplify the expression of transition labels (implies "
      "--merge-transitions)", 0 },
    { "remove-ap", OPT_REM_AP, "AP[=0|=1][,AP...]", 0,
      "remove atomic propositions either by existential quantification, or "
      "by assigning them 0 or 1", 0 },
    { "remove-unreachable-states", OPT_REM_UNREACH, nullptr, 0,
      "remove states that are unreachable from the initial state", 0 },
    { "remove-dead-states", OPT_REM_DEAD, nullptr, 0,
      "remove states that are unreachable, or that cannot belong to an "
      "infinite path", 0 },
    { "separate-sets", OPT_SEP_SETS, nullptr, 0,
      "if both Inf(x) and Fin(x) appear in the acceptance condition, replace "
      "Fin(x) by a new Fin(y) and adjust the automaton", 0 },
    { "sat-minimize", OPT_SAT_MINIMIZE, "options", OPTION_ARG_OPTIONAL,
      "minimize the automaton using a SAT solver (only works for deterministic"
      " automata)", 0 },
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Filtering options:", 6 },
    { "are-isomorphic", OPT_ARE_ISOMORPHIC, "FILENAME", 0,
      "keep automata that are isomorphic to the automaton in FILENAME", 0 },
    { "isomorphic", 0, nullptr, OPTION_ALIAS | OPTION_HIDDEN, nullptr, 0 },
    { "unique", 'u', nullptr, 0,
      "do not output the same automaton twice (same in the sense that they "\
      "are isomorphic)", 0 },
    { "is-complete", OPT_IS_COMPLETE, nullptr, 0,
      "keep complete automata", 0 },
    { "is-deterministic", OPT_IS_DETERMINISTIC, nullptr, 0,
      "keep deterministic automata", 0 },
    { "is-empty", OPT_IS_EMPTY, nullptr, 0,
      "keep automata with an empty language", 0 },
    { "is-unambiguous", OPT_IS_UNAMBIGUOUS, nullptr, 0,
      "keep only unambiguous automata", 0 },
    { "is-terminal", OPT_IS_TERMINAL, nullptr, 0,
      "keep only terminal automata", 0 },
    { "is-weak", OPT_IS_WEAK, nullptr, 0,
      "keep only weak automata", 0 },
    { "is-inherently-weak", OPT_IS_INHERENTLY_WEAK, nullptr, 0,
      "keep only inherently weak automata", 0 },
    { "intersect", OPT_INTERSECT, "FILENAME", 0,
      "keep automata whose languages have an non-empty intersection with"
      " the automaton from FILENAME", 0 },
    { "invert-match", 'v', nullptr, 0, "select non-matching automata", 0 },
    { "states", OPT_STATES, "RANGE", 0,
      "keep automata whose number of states are in RANGE", 0 },
    { "edges", OPT_EDGES, "RANGE", 0,
      "keep automata whose number of edges are in RANGE", 0 },
    { "acc-sets", OPT_ACC_SETS, "RANGE", 0,
      "keep automata whose number of acceptance sets are in RANGE", 0 },
    { "accept-word", OPT_ACC_WORD, "WORD", 0,
      "keep automata that recognize WORD", 0 },
    RANGE_DOC_FULL,
    { nullptr, 0, nullptr, 0,
      "If any option among --small, --deterministic, or --any is given, "
      "then the simplification level defaults to --high unless specified "
      "otherwise.  If any option among --low, --medium, or --high is given, "
      "then the simplification goal defaults to --small unless specified "
      "otherwise.  If none of those options are specified, then autfilt "
      "acts as is --any --low were given: these actually disable the "
      "simplification routines.", 22 },
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Miscellaneous options:", -1 },
    { "extra-options", 'x', "OPTS", 0,
      "fine-tuning options (see spot-x (7))", 0 },
    { "seed", OPT_SEED, "INT", 0,
      "seed for the random number generator (0)", 0 },
    { nullptr, 0, nullptr, 0, nullptr, 0 }
  };

static const struct argp_child children[] =
  {
    { &hoaread_argp, 0, nullptr, 0 },
    { &aoutput_argp, 0, nullptr, 0 },
    { &aoutput_io_format_argp, 0, nullptr, 4 },
    { &post_argp_disabled, 0, nullptr, 0 },
    { &misc_argp, 0, nullptr, -1 },
    { nullptr, 0, nullptr, 0 }
  };

typedef spot::twa_graph::graph_t::edge_storage_t tr_t;
typedef std::set<std::vector<tr_t>> unique_aut_t;
static long int match_count = 0;
static spot::option_map extra_options;
static bool randomize_st = false;
static bool randomize_tr = false;
static int opt_seed = 0;

// We want all these variable to be destroyed when we exit main, to
// make sure it happens before all other global variables (like the
// atomic propositions maps) are destroyed.  Otherwise we risk
// accessing deleted stuff.
static struct opt_t
{
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr product_and = nullptr;
  spot::twa_graph_ptr product_or = nullptr;
  spot::twa_graph_ptr intersect = nullptr;
  spot::twa_graph_ptr are_isomorphic = nullptr;
  std::unique_ptr<spot::isomorphism_checker>
                         isomorphism_checker = nullptr;
  std::unique_ptr<unique_aut_t> uniq = nullptr;
}* opt;

static bool opt_merge = false;
static bool opt_is_complete = false;
static bool opt_is_deterministic = false;
static bool opt_is_unambiguous = false;
static bool opt_is_terminal = false;
static bool opt_is_weak = false;
static bool opt_is_inherently_weak = false;
static bool opt_invert = false;
static range opt_states = { 0, std::numeric_limits<int>::max() };
static range opt_edges = { 0, std::numeric_limits<int>::max() };
static range opt_accsets = { 0, std::numeric_limits<int>::max() };
static std::vector<std::string> opt_acc_words;
static int opt_max_count = -1;
static bool opt_destut = false;
static char opt_instut = 0;
static bool opt_is_empty = false;
static bool opt_stripacc = false;
static bool opt_dnf_acc = false;
static bool opt_cnf_acc = false;
static bool opt_rem_fin = false;
static bool opt_clean_acc = false;
static bool opt_complement = false;
static bool opt_complement_acc = false;
static char* opt_decompose_strength = nullptr;
static spot::acc_cond::mark_t opt_mask_acc = 0U;
static std::vector<bool> opt_keep_states = {};
static unsigned int opt_keep_states_initial = 0;
static spot::exclusive_ap excl_ap;
static spot::remove_ap rem_ap;
static bool opt_simplify_exclusive_ap = false;
static bool opt_rem_dead = false;
static bool opt_rem_unreach = false;
static bool opt_sep_sets = false;
static const char* opt_sat_minimize = nullptr;

static int
parse_opt(int key, char* arg, struct argp_state*)
{
  // This switch is alphabetically-ordered.
  switch (key)
    {
    case 'c':
      automaton_format = Count;
      break;
    case 'F':
      jobs.emplace_back(arg, true);
      break;
    case 'n':
      opt_max_count = to_pos_int(arg);
      break;
    case 'u':
      opt->uniq =
        std::unique_ptr<unique_aut_t>(new std::set<std::vector<tr_t>>());
      break;
    case 'v':
      opt_invert = true;
      break;
    case 'x':
      {
	const char* opt = extra_options.parse_options(arg);
	if (opt)
	  error(2, 0, "failed to parse --options near '%s'", opt);
      }
      break;
    case OPT_ACC_SETS:
      opt_accsets = parse_range(arg, 0, std::numeric_limits<int>::max());
      break;
    case OPT_ACC_WORD:
      opt_acc_words.push_back(arg);
      break;
    case OPT_ARE_ISOMORPHIC:
      opt->are_isomorphic = read_automaton(arg, opt->dict);
      break;
    case OPT_CLEAN_ACC:
      opt_clean_acc = true;
      break;
    case OPT_CNF_ACC:
      opt_dnf_acc = false;
      opt_cnf_acc = true;
      break;
    case OPT_COMPLEMENT:
      opt_complement = true;
      break;
    case OPT_COMPLEMENT_ACC:
      opt_complement_acc = true;
      break;
    case OPT_DECOMPOSE_STRENGTH:
      opt_decompose_strength = arg;
      break;
    case OPT_DESTUT:
      opt_destut = true;
      break;
    case OPT_DNF_ACC:
      opt_dnf_acc = true;
      opt_cnf_acc = false;
      break;
    case OPT_EDGES:
      opt_edges = parse_range(arg, 0, std::numeric_limits<int>::max());
      break;
    case OPT_EXCLUSIVE_AP:
      excl_ap.add_group(arg);
      break;
    case OPT_INSTUT:
      if (!arg || (arg[0] == '1' && arg[1] == 0))
	opt_instut = 1;
      else if (arg[0] == '2' && arg[1] == 0)
	opt_instut = 2;
      else
	error(2, 0, "unknown argument for --instut: %s", arg);
      break;
    case OPT_INTERSECT:
      opt->intersect = read_automaton(arg, opt->dict);
      break;
    case OPT_IS_COMPLETE:
      opt_is_complete = true;
      break;
    case OPT_IS_DETERMINISTIC:
      opt_is_deterministic = true;
      break;
    case OPT_IS_EMPTY:
      opt_is_empty = true;
      break;
    case OPT_IS_INHERENTLY_WEAK:
      opt_is_inherently_weak = true;
      break;
    case OPT_IS_UNAMBIGUOUS:
      opt_is_unambiguous = true;
      break;
    case OPT_IS_TERMINAL:
      opt_is_terminal = true;
      break;
    case OPT_IS_WEAK:
      opt_is_weak = true;
      break;
    case OPT_MERGE:
      opt_merge = true;
      break;
    case OPT_MASK_ACC:
      {
	for (auto res : to_longs(arg))
	  {
	    if (res < 0)
	      error(2, 0, "acceptance sets should be non-negative:"
		    " --mask-acc=%ld", res);
	    if (static_cast<unsigned long>(res)
		> sizeof(spot::acc_cond::mark_t::value_t))
	      error(2, 0, "this implementation does not support that many"
		    " acceptance sets: --mask-acc=%ld", res);
	    opt_mask_acc.set(res);
	  }
	break;
      }
    case OPT_KEEP_STATES:
      {
        std::vector<long> values = to_longs(arg);
        if (!values.empty())
          opt_keep_states_initial = values[0];
        for (auto res : values)
	  {
	    if (res < 0)
	      error(2, 0, "state ids should be non-negative:"
		    " --mask-acc=%ld", res);
            // We don't know yet how many states the automata contain.
            if (opt_keep_states.size() <= static_cast<unsigned long>(res))
              opt_keep_states.resize(res + 1, false);
	    opt_keep_states[res] = true;
	  }
	opt_rem_unreach = true;
	break;
      }
    case OPT_PRODUCT_AND:
      {
	auto a = read_automaton(arg, opt->dict);
	if (!opt->product_and)
	  opt->product_and = std::move(a);
	else
	  opt->product_and = spot::product(std::move(opt->product_and),
					   std::move(a));
      }
      break;
    case OPT_PRODUCT_OR:
      {
	auto a = read_automaton(arg, opt->dict);
	if (!opt->product_or)
	  opt->product_or = std::move(a);
	else
	  opt->product_or = spot::product_or(std::move(opt->product_or),
					     std::move(a));
      }
      break;
    case OPT_RANDOMIZE:
      if (arg)
	{
	  for (auto p = arg; *p; ++p)
	    switch (*p)
	      {
	      case 's':
		randomize_st = true;
		break;
	      case 't':
		randomize_tr = true;
		break;
	      default:
		error(2, 0, "unknown argument for --randomize: '%c'", *p);
	      }
	}
      else
	{
	  randomize_tr = true;
	  randomize_st = true;
	}
      break;
    case OPT_REM_AP:
      rem_ap.add_ap(arg);
      break;
    case OPT_REM_DEAD:
      opt_rem_dead = true;
      break;
    case OPT_REM_FIN:
      opt_rem_fin = true;
      break;
    case OPT_REM_UNREACH:
      opt_rem_unreach = true;
      break;
    case OPT_SAT_MINIMIZE:
      opt_sat_minimize = arg ? arg : "";
      break;
    case OPT_SEED:
      opt_seed = to_int(arg);
      break;
    case OPT_SEP_SETS:
      opt_sep_sets = true;
      break;
    case OPT_SIMPLIFY_EXCLUSIVE_AP:
      opt_simplify_exclusive_ap = true;
      opt_merge = true;
      break;
    case OPT_STATES:
      opt_states = parse_range(arg, 0, std::numeric_limits<int>::max());
      break;
    case OPT_STRIPACC:
      opt_stripacc = true;
      break;
    case ARGP_KEY_ARG:
      jobs.emplace_back(arg, true);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


namespace
{
  class hoa_processor: public job_processor
  {
  private:
    spot::postprocessor& post;
    automaton_printer printer;
  public:

    hoa_processor(spot::postprocessor& post)
      : post(post), printer(aut_input)
    {
    }

    int
    process_formula(spot::formula, const char*, int)
    {
      SPOT_UNREACHABLE();
    }

    int
    process_automaton(const spot::const_parsed_aut_ptr& haut,
		      const char* filename)
    {
      spot::stopwatch sw;
      sw.start();

      // If --stats or --name is used, duplicate the automaton so we
      // never modify the original automaton (e.g. with
      // merge_edges()) and the statistics about it make sense.
      auto aut = ((automaton_format == Stats) || opt_name)
	? spot::make_twa_graph(haut->aut, spot::twa::prop_set::all())
	: haut->aut;

      // Preprocessing.

      if (opt_stripacc)
	spot::strip_acceptance_here(aut);
      if (opt_merge)
	aut->merge_edges();
      if (opt_clean_acc || opt_rem_fin)
	cleanup_acceptance_here(aut);
      if (opt_sep_sets)
	separate_sets_here(aut);
      if (opt_complement_acc)
	aut->set_acceptance(aut->acc().num_sets(),
			    aut->get_acceptance().complement());
      if (opt_complement)
	{
	  if (!spot::is_deterministic(aut))
	    {
	      std::cerr << filename << ':'
			<< haut->loc << (": --complement currently supports "
					 "only deterministic automata\n");
	      exit(2);
	    }
	  aut = spot::dtwa_complement(aut);
	}
      if (opt_rem_fin)
	aut = remove_fin(aut);
      if (opt_dnf_acc)
	aut->set_acceptance(aut->acc().num_sets(),
			    aut->get_acceptance().to_dnf());
      if (opt_cnf_acc)
	aut->set_acceptance(aut->acc().num_sets(),
			    aut->get_acceptance().to_cnf());

      // Filters.

      bool matched = true;

      matched &= opt_states.contains(aut->num_states());
      matched &= opt_edges.contains(aut->num_edges());
      matched &= opt_accsets.contains(aut->acc().num_sets());
      if (opt_is_complete)
	matched &= is_complete(aut);
      if (opt_is_deterministic)
	matched &= is_deterministic(aut);
      if (opt_is_deterministic)
	matched &= is_deterministic(aut);
      else if (opt_is_unambiguous)
	matched &= is_unambiguous(aut);
      if (opt_is_terminal)
	matched &= is_terminal_automaton(aut);
      else if (opt_is_weak)
	matched &= is_weak_automaton(aut);
      else if (opt_is_inherently_weak)
	matched &= is_inherently_weak_automaton(aut);
      if (opt->are_isomorphic)
        matched &= opt->isomorphism_checker->is_isomorphic(aut);
      if (opt_is_empty)
	matched &= aut->is_empty();
      if (opt->intersect)
	matched &= !spot::product(aut, opt->intersect)->is_empty();
      if (!opt_acc_words.empty())
	{
	  auto word_aut = spot::parse_word(opt_acc_words[0],
				       opt->dict)->as_automaton();
	  for (size_t i = 1; i < opt_acc_words.size(); ++i)
	    {
	      // Compose each additionnanl word with the first one
	      word_aut = spot::product(word_aut,
				       spot::parse_word(opt_acc_words[i],
							opt->dict)
				       ->as_automaton());
	    }
	  matched &= !spot::product(aut, word_aut)->is_empty();
	}

      // Drop or keep matched automata depending on the --invert option
      if (matched == opt_invert)
        return 0;

      // Postprocessing.

      if (opt_mask_acc)
	aut = mask_acc_sets(aut, opt_mask_acc & aut->acc().all_sets());

      if (!excl_ap.empty())
	aut = excl_ap.constrain(aut, opt_simplify_exclusive_ap);

      if (!rem_ap.empty())
	aut = rem_ap.strip(aut);

      if (opt_destut)
	aut = spot::closure(std::move(aut));
      if (opt_instut == 1)
	aut = spot::sl(std::move(aut));
      else if (opt_instut == 2)
	aut = spot::sl2(std::move(aut));

      if (!opt_keep_states.empty())
	aut = mask_keep_states(aut, opt_keep_states, opt_keep_states_initial);
      if (opt_rem_dead)
	aut->purge_dead_states();
      else if (opt_rem_unreach)
	aut->purge_unreachable_states();

      if (opt->product_and)
	aut = spot::product(std::move(aut), opt->product_and);
      if (opt->product_or)
	aut = spot::product_or(std::move(aut), opt->product_or);

      if (opt_decompose_strength)
	{
	  aut = decompose_strength(aut, opt_decompose_strength);
	  if (!aut)
	    return 0;
	}

      if (opt_sat_minimize)
	{
	  aut = spot::sat_minimize(aut, opt_sat_minimize, sbacc);
	  if (!aut)
	    return 0;
	}

      aut = post.run(aut, nullptr);

      if (randomize_st || randomize_tr)
	spot::randomize(aut, randomize_st, randomize_tr);

      const double conversion_time = sw.stop();

      if (opt->uniq)
        {
          auto tmp =
	    spot::canonicalize(make_twa_graph(aut,
						 spot::twa::prop_set::all()));
          if (!opt->uniq->emplace(tmp->edge_vector().begin() + 1,
				  tmp->edge_vector().end()).second)
	    return 0;
        }

      ++match_count;

      printer.print(aut, nullptr, filename, -1, conversion_time, haut);

      if (opt_max_count >= 0 && match_count >= opt_max_count)
	abort_run = true;

      return 0;
    }

    int
    aborted(const spot::const_parsed_aut_ptr& h, const char* filename)
    {
      std::cerr << filename << ':' << h->loc << ": aborted input automaton\n";
      return 2;
    }

    int
    process_file(const char* filename)
    {
      auto hp = spot::automaton_stream_parser(filename, opt_parse);
      int err = 0;
      while (!abort_run)
	{
	  auto haut = hp.parse(opt->dict);
	  if (!haut->aut && haut->errors.empty())
	    break;
	  if (haut->format_errors(std::cerr))
	    err = 2;
	  if (!haut->aut)
	    error(2, 0, "failed to read automaton from %s", filename);
	  else if (haut->aborted)
	    err = std::max(err, aborted(haut, filename));
	  else
            process_automaton(haut, filename);
	}
      return err;
    }
  };
}

int
main(int argc, char** argv)
{
  setup(argv);

  const argp ap = { options, parse_opt, "[FILENAMES...]",
		    argp_program_doc, children, nullptr, nullptr };

  try
    {
      // This will ensure that all objects stored in this struct are
      // destroyed before global variables.
      opt_t o;
      opt = &o;

      // Disable post-processing as much as possible by default.
      level = spot::postprocessor::Low;
      pref = spot::postprocessor::Any;
      type = spot::postprocessor::Generic;
      if (int err = argp_parse(&ap, argc, argv, ARGP_NO_HELP, nullptr, nullptr))
	exit(err);

      if (level_set && !pref_set)
	pref = spot::postprocessor::Small;
      if (pref_set && !level_set)
	level = spot::postprocessor::High;

      if (jobs.empty())
	jobs.emplace_back("-", true);

      if (opt->are_isomorphic)
	{
	  if (opt_merge)
	    opt->are_isomorphic->merge_edges();
	  opt->isomorphism_checker = std::unique_ptr<spot::isomorphism_checker>
	    (new spot::isomorphism_checker(opt->are_isomorphic));
	}


      spot::srand(opt_seed);

      spot::postprocessor post(&extra_options);
      post.set_pref(pref | comp | sbacc);
      post.set_type(type);
      post.set_level(level);

      hoa_processor processor(post);
      if (processor.run())
	return 2;
    }
  catch (const spot::parse_error& e)
    {
      auto is_several_lines =
	[] (const char* err)
	{
	  for (;;)
	    {
	      if (*err == '\0')
		return false;
	      else if (*err == '\n')
		return true;
	      ++err;
	    }
	};
      auto err = e.what();
      if (is_several_lines(err))
	error(2, 0, "\n%s", err);
      else
	error(2, 0, "%s", err);
    }
  catch (const std::runtime_error& e)
    {
      error(2, 0, "%s", e.what());
    }
  catch (const std::invalid_argument& e)
    {
      error(2, 0, "%s", e.what());
    }

  if (automaton_format == Count)
    std::cout << match_count << std::endl;

  return !match_count;
}
