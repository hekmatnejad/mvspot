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
#include "error.h"
#include "argmatch.h"
#include "common_aoutput.hh"
#include "common_post.hh"
#include "common_cout.hh"

#include <unistd.h>
#include <ctime>
#include <spot/twa/bddprint.hh>
#include <spot/twaalgos/dot.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twaalgos/isunamb.hh>
#include <spot/twaalgos/lbtt.hh>
#include <spot/twaalgos/neverclaim.hh>
#include <spot/twaalgos/strength.hh>
#include <spot/twaalgos/stutter.hh>
#include <spot/twaalgos/isdet.hh>

automaton_format_t automaton_format = Hoa;
static const char* automaton_format_opt = nullptr;
const char* opt_name = nullptr;
static const char* opt_output = nullptr;
static const char* stats = "";
enum check_type
  {
    check_unambiguous = (1 << 0),
    check_stutter = (1 << 1),
    check_strength = (1 << 2),
    check_semi_determinism = (1 << 3),
    check_all = -1U,
  };
static char const *const check_args[] =
  {
    "unambiguous",
    "stutter-invariant", "stuttering-invariant",
    "stutter-insensitive", "stuttering-insensitive",
    "stutter-sensitive", "stuttering-sensitive",
    "strength", "weak", "terminal",
    "semi-determinism", "semi-deterministic",
    "all",
    nullptr
  };
static check_type const check_types[] =
  {
    check_unambiguous,
    check_stutter, check_stutter,
    check_stutter, check_stutter,
    check_stutter, check_stutter,
    check_strength, check_strength, check_strength,
    check_semi_determinism, check_semi_determinism,
    check_all
  };
ARGMATCH_VERIFY(check_args, check_types);
unsigned opt_check = 0U;

enum {
  OPT_LBTT = 1,
  OPT_NAME,
  OPT_STATS,
  OPT_CHECK,
};

static const argp_option options[] =
  {
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Output format:", 3 },
    { "dot", 'd',
      "1|a|b|B|c|C(COLOR)|e|f(FONT)|h|k|n|N|o|r|R|s|t|v|y|+INT|<INT|#",
      OPTION_ARG_OPTIONAL,
      "GraphViz's format.  Add letters for "
      "(1) force numbered states, "
      "(a) acceptance display, (b) acceptance sets as bullets, "
      "(B) bullets except for Büchi/co-Büchi automata, "
      "(c) force circular nodes, "
      "(C) color nodes with COLOR, "
      "(e) force elliptic nodes, "
      "(f(FONT)) use FONT, (h) horizontal layout, "
      "(k) use state labels when possible, "
      "(n) with name, (N) without name, "
      "(o) ordered transitions, "
      "(r) rainbow colors for acceptance sets, "
      "(R) color acceptance sets by Inf/Fin, (s) with SCCs, "
      "(t) force transition-based acceptance, "
      "(v) vertical layout, "
      "(y) split universal edges by color, "
      "(+INT) add INT to all set numbers, "
      "(<INT) display at most INT states, "
      "(#) show internal edge numbers", 0 },
    { "hoaf", 'H', "1.1|i|k|l|m|s|t|v", OPTION_ARG_OPTIONAL,
      "Output the automaton in HOA format (default).  Add letters to select "
      "(1.1) version 1.1 of the format, "
      "(i) use implicit labels for complete deterministic automata, "
      "(s) prefer state-based acceptance when possible [default], "
      "(t) force transition-based acceptance, "
      "(m) mix state and transition-based acceptance, "
      "(k) use state labels when possible, "
      "(l) single-line output, "
      "(v) verbose properties", 0 },
    { "lbtt", OPT_LBTT, "t", OPTION_ARG_OPTIONAL,
      "LBTT's format (add =t to force transition-based acceptance even"
      " on Büchi automata)", 0 },
    { "name", OPT_NAME, "FORMAT", 0,
      "set the name of the output automaton", 0 },
    { "output", 'o', "FORMAT", 0,
      "send output to a file named FORMAT instead of standard output.  The"
      " first automaton sent to a file truncates it unless FORMAT starts"
      " with '>>'.", 0 },
    { "quiet", 'q', nullptr, 0, "suppress all normal output", 0 },
    { "spin", 's', "6|c", OPTION_ARG_OPTIONAL, "Spin neverclaim (implies --ba)."
      "  Add letters to select (6) Spin's 6.2.4 style, (c) comments on states",
      0 },
    { "utf8", '8', nullptr, 0, "enable UTF-8 characters in output "
      "(ignored with --lbtt or --spin)", 0 },
    { "stats", OPT_STATS, "FORMAT", 0,
      "output statistics about the automaton", 0 },
    { "check", OPT_CHECK, "PROP", OPTION_ARG_OPTIONAL,
      "test for the additional property PROP and output the result "
      "in the HOA format (implies -H).  PROP may be any prefix of "
      "'all' (default), 'unambiguous', 'stutter-invariant', or 'strength'.",
      0 },
    { nullptr, 0, nullptr, 0, nullptr, 0 }
  };

const struct argp aoutput_argp = { options, parse_opt_aoutput, nullptr, nullptr,
                                   nullptr, nullptr, nullptr };

// Those can be overridden by individual tools. E.g. randaut has no
// notion of input file, so %F and %L represent something else.
char F_doc[32] = "name of the input file";
char L_doc[32] = "location in the input file";

static const argp_option io_options[] =
  {
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Any FORMAT string may use "\
      "the following interpreted sequences (capitals for input,"
      " minuscules for output):", 4 },
    { "%F", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE, F_doc, 0 },
    { "%L", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE, L_doc, 0 },
    { "%H, %h", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "the automaton in HOA format on a single line (use %[opt]H or %[opt]h "
      "to specify additional options as in --hoa=opt)", 0 },
    { "%M, %m", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "name of the automaton", 0 },
    { "%S, %s", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of states", 0 },
    { "%E, %e", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of edges", 0 },
    { "%T, %t", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of transitions", 0 },
    { "%A, %a", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of acceptance sets", 0 },
    { "%G, %g", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "acceptance condition (in HOA syntax)", 0 },
    { "%C, %c, %[LETTERS]C, %[LETTERS]c", 0, nullptr,
      OPTION_DOC | OPTION_NO_USAGE,
      "number of SCCs; you may filter the SCCs to count "
      "using the following LETTERS, possibly concatenated: (a) accepting, "
      "(r) rejecting, (v) trivial, (t) terminal, (w) weak, "
      "(iw) inherently weak. Use uppercase letters to negate them.", 0 },
    { "%R, %[LETTERS]R", 0, nullptr,
      OPTION_DOC | OPTION_NO_USAGE,
      "CPU time (excluding parsing), in seconds; Add LETTERS to restrict to"
      "(u) user time, (s) system time, (p) parent process, "
      "or (c) children processes.", 0 },
    { "%N, %n", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of nondeterministic states", 0 },
    { "%D, %d", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "1 if the automaton is deterministic, 0 otherwise", 0 },
    { "%P, %p", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "1 if the automaton is complete, 0 otherwise", 0 },
    { "%r", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "wall-clock time elapsed in seconds (excluding parsing)", 0 },
    { "%W, %w", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "one word accepted by the automaton", 0 },
    { "%%", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "a single %", 0 },
    { "%<", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "the part of the line before the automaton if it "
      "comes from a column extracted from a CSV file", 4 },
    { "%>", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "the part of the line after the automaton if it "
      "comes from a column extracted from a CSV file", 4 },
    { nullptr, 0, nullptr, 0, nullptr, 0 }
  };

const struct argp aoutput_io_format_argp = { io_options, nullptr, nullptr,
                                             nullptr, nullptr,
                                             nullptr, nullptr };

static const argp_option o_options[] =
  {
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Any FORMAT string may use "\
      "the following interpreted sequences:", 4 },
    { "%F", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE, F_doc, 0 },
    { "%L", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE, L_doc, 0 },
    { "%h", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "the automaton in HOA format on a single line (use %[opt]h "
      "to specify additional options as in --hoa=opt)", 0 },
    { "%m", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "name of the automaton", 0 },
    { "%s", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of states", 0 },
    { "%e", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of edges", 0 },
    { "%t", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of transitions", 0 },
    { "%a", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of acceptance sets", 0 },
    { "%g", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "acceptance condition (in HOA syntax)", 0 },
    { "%c, %[LETTERS]c", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of SCCs; you may filter the SCCs to count "
      "using the following LETTERS, possibly concatenated: (a) accepting, "
      "(r) rejecting, (v) trivial, (t) terminal, (w) weak, "
      "(iw) inherently weak. Use uppercase letters to negate them.", 0 },
    { "%R, %[LETTERS]R", 0, nullptr,
      OPTION_DOC | OPTION_NO_USAGE,
      "CPU time (excluding parsing), in seconds; Add LETTERS to restrict to"
      "(u) user time, (s) system time, (p) parent process, "
      "or (c) children processes.", 0 },
    { "%n", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "number of nondeterministic states in output", 0 },
    { "%d", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "1 if the output is deterministic, 0 otherwise", 0 },
    { "%p", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "1 if the output is complete, 0 otherwise", 0 },
    { "%r", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "wall-clock time elapsed in seconds (excluding parsing)", 0 },
    { "%w", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "one word accepted by the output automaton", 0 },
    { "%%", 0, nullptr, OPTION_DOC | OPTION_NO_USAGE,
      "a single %", 0 },
    { nullptr, 0, nullptr, 0, nullptr, 0 }
  };

const struct argp aoutput_o_format_argp = { o_options,
                                            nullptr, nullptr, nullptr,
                                            nullptr, nullptr, nullptr };

int parse_opt_aoutput(int key, char* arg, struct argp_state*)
{
  // This switch is alphabetically-ordered.
  switch (key)
    {
    case '8':
      spot::enable_utf8();
      break;
    case 'd':
      automaton_format = Dot;
      automaton_format_opt = arg;
      break;
    case 'H':
      automaton_format = Hoa;
      automaton_format_opt = arg;
      break;
    case 'o':
      opt_output = arg;
      break;
    case 'q':
      automaton_format = Quiet;
      break;
    case 's':
      automaton_format = Spin;
      if (type != spot::postprocessor::Monitor)
        type = spot::postprocessor::BA;
      automaton_format_opt = arg;
      break;
    case OPT_CHECK:
      automaton_format = Hoa;
      if (arg)
        opt_check |= XARGMATCH("--check", arg, check_args, check_types);
      else
        opt_check |= check_all;
      break;
    case OPT_LBTT:
      automaton_format = Lbtt;
      automaton_format_opt = arg;
      // This test could be removed when more options are added,
      // because print_lbtt will raise an exception anyway.  The
      // error message is slightly better in the current way.
      if (arg && (arg[0] != 't' || arg[1] != 0))
        error(2, 0, "unknown argument for --lbtt: '%s'", arg);
      break;
    case OPT_NAME:
      opt_name = arg;
      break;
    case OPT_STATS:
      if (!*arg)
        error(2, 0, "empty format string for --stats");
      stats = arg;
      automaton_format = Stats;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

void setup_default_output_format()
{
  if (auto val = getenv("SPOT_DEFAULT_FORMAT"))
    {
      static char const *const args[] =
        {
          "dot", "hoa", "hoaf", nullptr
        };
      static automaton_format_t const format[] =
        {
          Dot, Hoa, Hoa
        };
      auto eq = strchr(val, '=');
      if (eq)
        {
          val = strndup(val, eq - val);
          automaton_format_opt = eq + 1;
        }
      ARGMATCH_VERIFY(args, format);
      automaton_format = XARGMATCH("SPOT_DEFAULT_FORMAT", val, args, format);
      if (eq)
        free(val);
    }
}

hoa_stat_printer::hoa_stat_printer(std::ostream& os, const char* format,
                                   stat_style input)
  : spot::stat_printer(os, format)
{
  if (input == aut_input)
    {
      declare('A', &haut_acc_);
      declare('C', &haut_scc_);
      declare('D', &haut_deterministic_);
      declare('E', &haut_edges_);
      declare('G', &haut_gen_acc_);
      declare('H', &input_aut_);
      declare('M', &haut_name_);
      declare('N', &haut_nondetstates_);
      declare('P', &haut_complete_);
      declare('S', &haut_states_);
      declare('T', &haut_trans_);
      declare('W', &haut_word_);
    }
  declare('<', &csv_prefix_);
  declare('>', &csv_suffix_);
  declare('F', &filename_);
  declare('L', &location_);
  declare('R', &timer_);
  if (input != ltl_input)
    declare('f', &filename_);        // Override the formula printer.
  declare('h', &output_aut_);
  declare('m', &aut_name_);
  declare('w', &aut_word_);
}

std::ostream&
hoa_stat_printer::print(const spot::const_parsed_aut_ptr& haut,
                        const spot::const_twa_graph_ptr& aut,
                        spot::formula f,
                        const char* filename, int loc, process_timer& ptimer,
                        const char* csv_prefix, const char* csv_suffix)
{
  timer_ = ptimer.dt;
  double run_time = ptimer.get_lap_sw();

  filename_ = filename ? filename : "";
  csv_prefix_ = csv_prefix ? csv_prefix : "";
  csv_suffix_ = csv_suffix ? csv_suffix : "";
  if (loc >= 0 && has('L'))
    {
      std::ostringstream os;
      os << loc;
      location_ = os.str();
    }
  output_aut_ = aut;
  if (haut)
    {
      input_aut_ = haut->aut;
      if (loc < 0 && has('L'))
        {
          std::ostringstream os;
          os << haut->loc;
          location_ = os.str();
        }

      if (has('T'))
        {
          spot::twa_sub_statistics s = sub_stats_reachable(haut->aut);
          haut_states_ = s.states;
          haut_edges_ = s.edges;
          haut_trans_ = s.transitions;
        }
      else if (has('E'))
        {
          spot::twa_sub_statistics s = sub_stats_reachable(haut->aut);
          haut_states_ = s.states;
          haut_edges_ = s.edges;
        }
      if (has('M'))
        {
          auto n = haut->aut->get_named_prop<std::string>("automaton-name");
          if (n)
            haut_name_ = *n;
          else
            haut_name_.val().clear();
        }
      if (has('S'))
        haut_states_ = haut->aut->num_states();

      if (has('A'))
        haut_acc_ = haut->aut->acc().num_sets();

      if (has('C'))
        haut_scc_.automaton(haut->aut);

      if (has('N'))
        {
          haut_nondetstates_ = count_nondet_states(haut->aut);
          haut_deterministic_ = (haut_nondetstates_ == 0);
        }
      else if (has('D'))
        {
          // This is more efficient than calling count_nondet_state().
          haut_deterministic_ = is_deterministic(haut->aut);
        }

      if (has('p'))
        haut_complete_ = is_complete(haut->aut);

      if (has('G'))
        {
          std::ostringstream os;
          os << haut->aut->get_acceptance();
          haut_gen_acc_ = os.str();
        }
      if (has('W'))
        {
          if (auto word = haut->aut->accepting_word())
            {
              std::ostringstream out;
              out << *word;
              haut_word_ = out.str();
            }
          else
            {
              haut_word_.val().clear();
            }
        }
    }

  if (has('m'))
    {
      auto n = aut->get_named_prop<std::string>("automaton-name");
      if (n)
        aut_name_ = *n;
      else
        aut_name_.val().clear();
    }
  if (has('w'))
    {
      if (auto word = aut->accepting_word())
        {
          std::ostringstream out;
          out << *word;
          aut_word_ = out.str();
        }
      else
        {
          aut_word_.val().clear();
        }
    }

  auto& res = this->spot::stat_printer::print(aut, f, run_time);
  // Make sure we do not store the automaton until the next one is
  // printed, as the registered APs will affect how the next
  // automata are built.
  output_aut_ = nullptr;
  input_aut_ = nullptr;
  haut_scc_.reset();
  return res;
}

automaton_printer::automaton_printer(stat_style input)
  : statistics(std::cout, stats, input),
    namer(name, opt_name, input),
    outputnamer(outputname, opt_output, input)
{
  if (automaton_format == Count && opt_output)
    throw std::runtime_error
      ("options --output and --count are incompatible");
}

void
automaton_printer::print(const spot::twa_graph_ptr& aut,
                         // Time for statistics
                         process_timer& ptimer,
                         spot::formula f,
                         // Input location for errors and statistics.
                         const char* filename,
                         int loc,
                         // input automaton for statistics
                         const spot::const_parsed_aut_ptr& haut,
                         const char* csv_prefix,
                         const char* csv_suffix)
{
  if (opt_check)
    {
      if (opt_check & check_stutter)
        spot::check_stutter_invariance(aut, f);
      if (opt_check & check_unambiguous)
        spot::check_unambiguous(aut);
      if (opt_check & check_strength)
        spot::check_strength(aut);
      if (opt_check & check_semi_determinism)
        spot::is_semi_deterministic(aut); // sets the property as a side effect.
    }

  // Name the output automaton.
  if (opt_name)
    {
      name.str("");
      namer.print(haut, aut, f, filename, loc, ptimer, csv_prefix, csv_suffix);
      aut->set_named_prop("automaton-name", new std::string(name.str()));
    }

  std::ostream* out = &std::cout;
  if (opt_output)
    {
      outputname.str("");
      outputnamer.print(haut, aut, f, filename, loc, ptimer,
                        csv_prefix, csv_suffix);
      std::string fname = outputname.str();
      auto p = outputfiles.emplace(fname, nullptr);
      if (p.second)
        p.first->second.reset(new output_file(fname.c_str()));
      out = &p.first->second->ostream();
    }

  // Output it.
  switch (automaton_format)
    {
    case Count:
    case Quiet:
      // Do not output anything.
      break;
    case Dot:
      spot::print_dot(*out, aut, automaton_format_opt);
      break;
    case Lbtt:
      spot::print_lbtt(*out, aut, automaton_format_opt);
      break;
    case Hoa:
      spot::print_hoa(*out, aut, automaton_format_opt) << '\n';
      break;
    case Spin:
      spot::print_never_claim(*out, aut, automaton_format_opt);
      break;
    case Stats:
      statistics.set_output(*out);
      statistics.print(haut, aut, f, filename, loc, ptimer,
                       csv_prefix, csv_suffix) << '\n';
      break;
    }
  flush_cout();
}

void automaton_printer::add_stat(char c, const spot::printable* p)
{
  namer.declare(c, p);
  statistics.declare(c, p);
  outputnamer.declare(c, p);
}

automaton_printer::~automaton_printer()
{
  for (auto& p : outputfiles)
    p.second->close(p.first);
}


void printable_automaton::print(std::ostream& os, const char* pos) const
{
  std::string options = "l";
  if (*pos == '[')
    {
      ++pos;
      auto end = strchr(pos, ']');
      options = std::string(pos, end - pos);
      options += 'l';
      pos = end + 1;
    }
  print_hoa(os, val_, options.c_str());
}

void printable_timer::print(std::ostream& os, const char* pos) const
{
  double res = 0;

#ifdef _SC_CLK_TCK
  const long clocks_per_sec = sysconf(_SC_CLK_TCK);
#else
#  ifdef CLOCKS_PER_SEC
  const long clocks_per_sec = CLOCKS_PER_SEC;
#  else
  const long clocks_per_sec = 100;
#  endif
#endif

  if (*pos != '[')
  {
    res = val_.get_uscp(true, true, true, true);
    os << res / clocks_per_sec;
    return;
  }

  bool user = false;
  bool system = false;
  bool parent = false;
  bool children = false;

  const char* beg = pos;
  auto error = [&](std::string str)
  {
    std::ostringstream tmp;
    const char* end = std::strchr(pos, ']');
    tmp << "unknown option '" << str << "' in '%" << std::string(beg, end + 2)
      << '\'';
    throw std::runtime_error(tmp.str());
  };

  do
  {
    ++pos;
    switch (*pos)
    {
      case 'u':
        user = true;
        break;
      case 's':
        system = true;
        break;
      case 'p':
        parent = true;
        break;
      case 'c':
        children = true;
        break;
      case ' ':
      case '\t':
      case '\n':
      case ',':
      case ']':
        break;
      default:
        error(std::string(pos, pos + 1));
    }
  } while (*pos != ']');

  if (!parent && !children)
    parent = children = true;
  if (!user && !system)
    user = system = true;

  res = val_.get_uscp(user, system, children, parent);
  os << res / clocks_per_sec;
}
