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


#ifndef SPOT_FASTTGBA_CUBE_HH
# define SPOT_FASTTGBA_CUBE_HH

#include <vector>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include "ap_dict.hh"

namespace spot
{
  /// \brief This class represents conjunction of variables
  ///
  /// It is used as a wrapper for manipulating set of variables
  /// without dealing with the implementation.
  class SPOT_API cube
  {
  public:
    /// \brief Initialize a cube of size \a size
    ///
    /// Default initialisation set all the cube to true
    cube (ap_dict& aps);

    /// \breif a copy contructor
    cube (const spot::cube& c);

    /// \brief Compare two cubes
    ///
    /// \param rhs the object to compare with
    bool operator==(const spot::cube& rhs);

    /// \brief The logical AND of two cubes
    ///
    /// Warning! This function do not check the consistency
    /// of the resulting cube. To check this consistency
    /// a call to is_valid is needed.
    ///
    /// \param rhs the object to AND  with
    cube operator&(const cube& rhs) const;

    /// \brief Set the variable at the \a index position to true
    ///
    /// \param index the index in the cube
    void set_true_var(size_t index);

    /// \brief Set the variable at the \a index position to false
    ///
    /// \param index the index in the cube
    void set_false_var(size_t index);

    /// \brief Set the variable at the \a index position to free
    ///
    /// \param index the index in the cube
    void set_free_var(size_t index);

    /// \brief return the size of the cube
    size_t size() const;

    /// \brief return true if the cube is valid considering true and false
    /// variables
    bool is_valid() const;

    /// \brief output the description of the cube
    std::string dump();

  protected:

    // -----------------------------------------------------------
    // This class is a wrapper for two bitsets :
    //   - true_var  : a bitset representing variables that
    //                 are set to true
    //   - false_var : a bitset representing variables that
    //                 are set to false
    //
    // In the  two vectors a bit set to 1 represent a variable set to
    // true (resp. false) for the true_var (resp. false_var)
    //
    // The cube for (a & !b) will be repensented by :
    //     - true_var  = 1 0
    //     - false_var = 0 1
    //
    // To represent free variables such as in (a & !b) | (a & b)
    // (wich is equivalent to (a) with b free)
    //     - true_var  : 1 0
    //     - false_var : 0 0
    // This exemple shows that the representation of free variables
    // is done by unsetting variable in both vector
    //
    // Warning : a variable cannot be set in both bitset at the
    //           same time (consistency! cannot be true and false)
    // -----------------------------------------------------------

    boost::dynamic_bitset<> true_var;   ///< the set of variables set to true
    boost::dynamic_bitset<> false_var;  ///< the set of variables set to false
    ap_dict& aps_;			///< the reference over the atomic props
    mutable bool valid;
  };
}

#endif  // SPOT_FASTTGBA_CUBE_HH