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

#include "config.h"
#include "formater.hh"
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include "satsolver.hh"
#include <fstream>
#include <limits>

namespace spot
{
  namespace
  {
    struct satsolver_command: formater
    {
      const char* satsolver;

      satsolver_command()
      {
	satsolver = getenv("SPOT_SATSOLVER");
	if (!satsolver)
	  {
	    satsolver = "glucose %I >%O";
	    return;
	  }
	prime(satsolver);
	if (!has('I'))
	  throw std::runtime_error("SPOT_SATSOLVER should contain %I to "
				   "indicate how to use the input filename.");
	if (!has('O'))
	  throw std::runtime_error("SPOT_SATSOLVER should contain %O to "
				   "indicate how to use the output filename.");
      }

      int
      run(printable* in, printable* out)
      {
	declare('I', in);
	declare('O', out);
	std::ostringstream s;
	format(s, satsolver);
	return system(s.str().c_str());
      }
    };
  }

  int satsolver(printable* input, printable* output)
  {
    // Make this static, so the SPOT_SATSOLVER lookup is done only on
    // the first call to run_sat().
    static satsolver_command cmd;
    return cmd.run(input, output);
  }

  sat_solution
  satsolver_get_solution(const char* filename)
  {
    sat_solution sol;
    std::istream* in;
    if (filename[0] == '-' && filename[1] == 0)
      in = &std::cin;
    else
      in = new std::fstream(filename, std::ios_base::in);

    int c;
    while ((c = in->get()) != EOF)
      {
	// If a line does not start with 'v ', ignore it.
	if (c != 'v' || in->get() != ' ')
	  {
	    in->ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	    continue;
	  }
	// Otherwise, read integers one by one.
	int i;
	while (*in >> i)
	  {
	    if (i == 0)
	      goto done;
	    sol.push_back(i);
	  }
	if (!in->eof())
	  // If we haven't reached end-of-file, then we just attempted
	  // to extract something that wasn't an integer.  Clear the
	  // fail bit so that will loop over.
	  in->clear();
      }
  done:
    if (in != &std::cin)
      delete in;
    return sol;
  }

}