// -*- coding: utf-8 -*-
// Copyright (C) 2012, 2013, 2014, 2015, 2016 Laboratoire de Recherche
// et Développement de l'Epita (LRDE).
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

#include <argp.h>
#include "error.h"

#include "common_setup.hh"
#include "common_r.hh"
#include "common_cout.hh"
#include "common_finput.hh"
#include "common_output.hh"
#include "common_aoutput.hh"
#include "common_post.hh"

#include <spot/tl/formula.hh>
#include <spot/tl/print.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/misc/optionmap.hh>
#include <spot/misc/timer.hh>

static const char argp_program_doc[] ="\
Translate linear-time formulas (LTL/PSL) into Büchi automata.\n\n\
By default it will apply all available optimizations to output \
the smallest Transition-based Generalized Büchi Automata, \
in GraphViz's format.\n\
If multiple formulas are supplied, several automata will be output.";


static const argp_option options[] =
  {
    /**************************************************/
    { "%f", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "the formula, in Spot's syntax", 4 },
    { "%<", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "the part of the line before the formula if it "
      "comes from a column extracted from a CSV file", 4 },
    { "%>", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "the part of the line after the formula if it "
      "comes from a column extracted from a CSV file", 4 },
    /**************************************************/
    { "unambiguous", 'U', nullptr, 0, "output unambiguous automata", 2 },
    { nullptr, 0, nullptr, 0, "Miscellaneous options:", -1 },
    { "extra-options", 'x', "OPTS", 0,
      "fine-tuning options (see spot-x (7))", 0 },
    { nullptr, 0, nullptr, 0, nullptr, 0 }
  };

const struct argp_child children[] =
  {
    { &finput_argp, 0, nullptr, 1 },
    { &aoutput_argp, 0, nullptr, 0 },
    { &aoutput_o_format_argp, 0, nullptr, 0 },
    { &post_argp, 0, nullptr, 0 },
    { &misc_argp, 0, nullptr, -1 },
    { nullptr, 0, nullptr, 0 }
  };

static spot::option_map extra_options;
static spot::postprocessor::output_pref unambig = 0;

static int
parse_opt(int key, char* arg, struct argp_state*)
{
  // This switch is alphabetically-ordered.
  switch (key)
    {
    case 'U':
      unambig = spot::postprocessor::Unambiguous;
      break;
    case 'x':
      {
        const char* opt = extra_options.parse_options(arg);
        if (opt)
          error(2, 0, "failed to parse --options near '%s'", opt);
      }
      break;
    case ARGP_KEY_ARG:
      // FIXME: use stat() to distinguish filename from string?
      jobs.emplace_back(arg, false);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


namespace
{
  class trans_processor final: public job_processor
  {
  public:
    spot::translator& trans;
    automaton_printer printer;

    trans_processor(spot::translator& trans)
      : trans(trans), printer(ltl_input)
    {
    }

    int
    process_formula(spot::formula f,
                    const char* filename = nullptr, int linenum = 0)
    {
      // This should not happen, because the parser we use can only
      // read PSL/LTL formula, but since our formula type can
      // represent more than PSL formula, let's make this
      // future-proof.
      if (!f.is_psl_formula())
        {
          std::string s = spot::str_psl(f);
          error_at_line(2, 0, filename, linenum,
                        "formula '%s' is not an LTL or PSL formula",
                        s.c_str());
        }

      spot::stopwatch sw;
      sw.start();
      auto aut = trans.run(&f);
      const double translation_time = sw.stop();

      printer.print(aut, f, filename, linenum, translation_time, nullptr,
                    prefix, suffix);
      return 0;
    }
  };
}

int
main(int argc, char** argv)
{
  // By default we name automata using the formula.
  opt_name = "%f";

  setup(argv);

  const argp ap = { options, parse_opt, "[FORMULA...]",
                    argp_program_doc, children, nullptr, nullptr };

  simplification_level = 3;

  if (int err = argp_parse(&ap, argc, argv, ARGP_NO_HELP, nullptr, nullptr))
    exit(err);

  if (jobs.empty())
    error(2, 0, "No formula to translate?  Run '%s --help' for usage.",
          program_name);

  spot::translator trans(&extra_options);
  trans.set_pref(pref | comp | sbacc | unambig);
  trans.set_type(type);
  trans.set_level(level);

  try
    {
      trans_processor processor(trans);
      if (processor.run())
        return 2;
    }
  catch (const std::runtime_error& e)
    {
      error(2, 0, "%s", e.what());
    }
  return 0;
}