// Copyright (C) 2009 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
// Copyright (C) 2003, 2004 Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include <list>
#include <iostream>
#include "ltlenv/defaultenv.hh"
#include "tgbaparse/public.hh"
#include "tgbaalgos/dotty.hh"
#include "misc/acccompl.hh"


using namespace spot;


int main(int argc,
        char** argv)
{
  if (argc != 2)
  {
    std::cout << "Error in usage" << std::endl;
    return 1;
  }

  spot::bdd_dict* dict = new spot::bdd_dict();

  tgba_parse_error_list pel;

  std::string name(argv[1]);

  spot::tgba* automata = tgba_parse(name, pel, dict);
  if (!pel.empty())
  {
    spot::format_tgba_parse_errors(std::cerr, name, pel);
    return 2;
  }

  spot::AccComplAutomaton*  acc1 = new spot::AccComplAutomaton(automata);

  spot::dotty_reachable(std::cout, automata);
  acc1->run();

  std::cout << "--------sed-me---------" << std::endl;

  spot::AccComplAutomaton*  acc2 = new spot::AccComplAutomaton(automata);
  acc2->revert();
  spot::dotty_reachable(std::cout, automata);

  delete acc1;
  delete acc2;
  delete automata;
  delete dict;

  return 0;
}
