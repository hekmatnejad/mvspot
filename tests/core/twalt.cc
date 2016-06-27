// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et Développement
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
#include <spot/twa/twalt.hh>
#include <spot/twaalgos/hoa.hh>

int main()
{
  std::ofstream s("lol.debug");

  s << "Begin:" << std::endl;

  auto dict = spot::make_bdd_dict();
  auto aut = make_twalt(dict);

  s << "automaton and bdd_dict created" << std::endl;

  auto s1 = aut->new_state();
  auto s2 = aut->new_state();
  auto s3 = aut->new_state();
  auto s4 = aut->new_state();
  auto s5 = aut->new_state();

  s << "states created" << std::endl;

  auto mdr = "p1";
  auto mdr2 = "p2";

  aut->register_ap(mdr);
  aut->register_ap(mdr2);

  bdd p1; // = bdd_ithvar(aut->register_ap("p1"));
  bdd p2; // = bdd_ithvar(aut->register_ap("p2"));

  s << "bdds created" << std::endl;

  aut->new_edge(s2, s3, bddfalse);
  aut->new_edge(s1, {s2, s3, s4, s5}, p1);
  aut->new_edge(s3, {s4, s5}, p2);

  s << "edges created" << std::endl;

  std::ofstream out("lol.debug");
  //print_hoa(out, aut);
  return 0;
}
