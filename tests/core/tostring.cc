// -*- coding: utf-8 -*-
// Copyright (C) 2008, 2009, 2012, 2015 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
// Copyright (C) 2003 Laboratoire d'Informatique de Paris 6 (LIP6),
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

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>

static void
syntax(char *prog)
{
  std::cerr << prog << " formula1" << std::endl;
  exit(2);
}

int
main(int argc, char **argv)
{
  if (argc != 2)
    syntax(argv[0]);

  {
    spot::parse_error_list p1;
    auto f1 = spot::parse_infix_psl(argv[1], p1);

    if (spot::format_parse_errors(std::cerr, argv[1], p1))
      return 2;

    // The string generated from an abstract tree should be parsable
    // again.

    std::string f1s = spot::str_psl(f1);
    std::cout << f1s << '\n';

    auto f2 = spot::parse_infix_psl(f1s, p1);

    if (spot::format_parse_errors(std::cerr, f1s, p1))
      return 2;

    // This second abstract tree should be equal to the first.

    if (f1 != f2)
      return 1;

    // It should also map to the same string.

    std::string f2s = spot::str_psl(f2);
    std::cout << f2s << '\n';

    if (f2s != f1s)
      return 1;
  }

  assert(spot::fnode::instances_check());
  return 0;
}