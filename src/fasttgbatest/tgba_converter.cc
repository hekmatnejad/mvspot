// Copyright (C) 2012 Laboratoire de Recherche et D�veloppement
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

#include <iostream>

// This part is for TGBA
#include "ltlast/allnodes.hh"
#include "ltlparse/public.hh"
#include "tgbaalgos/ltl2tgba_fm.hh"
#include "tgbaalgos/postproc.hh"

// This part is for FASTTGBA
#include "fasttgbaalgos/tgba2fasttgba.hh"
#include "fasttgbaalgos/dotty_dfs.hh"


void syntax (char*)
{
  std::cout << "Syntax" << std::endl;
  exit (1);
}

int main(int argc, char **argv)
{

  //  The environement for LTL
  spot::ltl::environment& env(spot::ltl::default_environment::instance());
  // Option for the simplifier
  spot::ltl::ltl_simplifier_options redopt(false, false, false, false, false);

  //  The dictionnary
  spot::bdd_dict* dict = new spot::bdd_dict();


  // The automaton of the formula
  const spot::tgba* a = 0;

  // Should whether the formula be reduced
  bool simpltl = false;

  // The index of the formula
  int formula_index = 0;

  for (;;)
    {
      //  Syntax check
      if (argc < formula_index + 2)
	syntax(argv[0]);

      ++formula_index;

      if (argc == (formula_index + 1))
	break;
      syntax(argv[0]);
    }

  // The formula string
  std::string input =  argv[formula_index];
  // The formula in spot
  const spot::ltl::formula* f = 0;

  //
  // Building the formula from the input
  //
  spot::ltl::parse_error_list pel;
  f = spot::ltl::parse(input, pel, env, false);

  if (f)
    {
      //
      // Simplification for the formula among differents levels
      //
      {
	spot::ltl::ltl_simplifier* simp = 0;
	if (simpltl)
	  simp = new spot::ltl::ltl_simplifier(redopt, dict);

	if (simp)
	  {
	    const spot::ltl::formula* t = simp->simplify(f);
	    f->destroy();
	    f = t;
	  }
	delete simp;
      }


      std::cout << "===>   "
		<< spot::ltl::to_string(f) << std::endl;

      //
      // Building the TGBA of the formula
      //
      a = spot::ltl_to_tgba_fm(f, dict);
      assert (a);

      spot::postprocessor *pp = new spot::postprocessor();
      a = pp->run(a, f);
      delete pp;


      const spot::fasttgba* ftgba = spot::tgba_2_fasttgba(a);
      spot::dotty_dfs dotty(ftgba);
      dotty.run();
      delete ftgba;
    }

  // Clean up
  f->destroy();
  delete a;
  delete dict;

  // Check effective clean up
  spot::ltl::atomic_prop::dump_instances(std::cerr);
  spot::ltl::unop::dump_instances(std::cerr);
  spot::ltl::binop::dump_instances(std::cerr);
  spot::ltl::multop::dump_instances(std::cerr);
  spot::ltl::automatop::dump_instances(std::cerr);
  assert(spot::ltl::atomic_prop::instance_count() == 0);
  assert(spot::ltl::unop::instance_count() == 0);
  assert(spot::ltl::binop::instance_count() == 0);
  assert(spot::ltl::multop::instance_count() == 0);
  assert(spot::ltl::automatop::instance_count() == 0);

  return 0;
}