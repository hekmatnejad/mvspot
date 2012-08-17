// -*- coding: utf-8 -*-
// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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

#ifndef SPOT_TGBAALGOS_SIMULATION_HH
# define SPOT_TGBAALGOS_SIMULATION_HH


namespace spot
{
  class tgba;

  /// \addtogroup tgba_reduction
  /// @{

  /// \brief Attempt to reduce the automaton by direct simulation When
  /// the suffixes (letter and acceptance conditions) seen by one
  /// state is included in the suffixes seen by another one, the first
  /// one is merged with the second.  The algorithm is based on the
  /// following paper, but generalized to handle TGBA directly.
  ///
  /// \verbatim
  /// @InProceedings{ etessami.00.concur,
  ///   author        = {Kousha Etessami and Gerard J. Holzmann},
  ///   title         = {Optimizing {B\"u}chi Automata},
  ///   booktitle     = {Proceedings of the 11th International Conference on
  ///	  	      Concurrency Theory (Concur'00)},
  ///   pages         = {153--167},
  ///   year          = {2000},
  ///   editor        = {C. Palamidessi},
  ///   volume        = {1877},
  ///   series        = {Lecture Notes in Computer Science},
  ///   address       = {Pennsylvania, USA},
  ///   publisher     = {Springer-Verlag}
  /// }
  /// \endverbatim
  ///
  /// \param automaton the automaton to simulate.
  /// \return a new automaton which is at worst a copy of the received
  /// one
  tgba* simulation(const tgba* automaton);


  /// \brief Attempt to reduce the automaton by direct cosimulation.
  /// When the prefixes (letter and acceptance conditions) seen by one
  /// state is included in the prefixes seen by another one, the first
  /// one is merged with the second.  The algorithm is based on the
  /// following paper, but generalized to handle TGBA directly.
  /// \verbatim
  /// @InProceedings{Somenzi:2000:EBA:647769.734097,
  ///    author = {Somenzi, Fabio and Bloem, Roderick},
  ///    title = {Efficient {B\"u}chi Automata from LTL Formulae},
  ///    booktitle = {Proceedings of the 12th International
  ///                 Conference on Computer Aided Verification},
  ///    series = {CAV '00},
  ///    year = {2000},
  ///    isbn = {3-540-67770-4},
  ///    pages = {248--263},
  ///    numpages = {16},
  ///    url = {http://dl.acm.org/citation.cfm?id=647769.734097},
  ///    acmid = {734097},
  ///    publisher = {Springer-Verlag},
  ///    address = {London, UK, UK},
  /// }
  /// \endverbatim
  /// \param automaton the automaton to simulate.
  /// \return a new automaton which is at worst a copy of the received
  /// one
  tgba* cosimulation(const tgba* automaton);

  /// @}
} // End namespace spot.



#endif // !SPOT_TGBAALGOS_SIMULATION_HH
