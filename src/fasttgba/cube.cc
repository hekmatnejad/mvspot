// Copyright (C) 2012 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
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

#include <sstream>
#include <iostream>
#include "cube.hh"

namespace spot
{
  cube::cube (size_t size)
  {
    true_var = boost::dynamic_bitset<>(size);
    false_var = boost::dynamic_bitset<>(size);
  }

  bool
  cube::operator==(const cube::cube& rhs)
  {
    return (true_var == rhs.true_var)
      &&  (false_var == rhs.false_var);
  }

  void
  cube::set_true_var(size_t index)
  {
    assert(index < size());
    true_var[index] = 1;
    false_var[index] = 0;
  }

  void
  cube::set_false_var(size_t index)
  {
    assert(index < size());
    true_var[index] = 0;
    false_var[index] = 1;
  }

  void
  cube::set_free_var(size_t index)
  {
    assert(index < size());
    true_var[index] = 0;
    false_var[index] = 0;
  }

  size_t
  cube::size()
  {
    return true_var.size();
  }

  std::string
  cube::dump(std::vector<std::string> names)
  {
    std::ostringstream oss;
    if (names.empty())
      {
	oss <<" true_var " << true_var << std::endl;
	oss <<" false_var " << false_var << std::endl;
      }
    else
      {
	size_t i;
	bool all_free = true;
	for (i = 0; i < size(); ++i)
	  {
	    if (true_var[i])
	      {
		oss << names[i] << " ";
		all_free = false;
	      }
	    if (false_var[i])
	      {
		oss << "!" << names[i] << " ";
		all_free = false;
	      }
	  }
	if (all_free)
	  oss << "1";
      }
    return oss.str();
  }
}