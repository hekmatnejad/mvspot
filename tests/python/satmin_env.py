# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2015  Laboratoire de Recherche et Développement
# de l'Epita
#
# This file is part of Spot, a model checking library.
#
# Spot is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# Spot is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import shutil
import sys
import spot
import os

# Ignore the test if glucose is not installed.
if shutil.which("glucose") == None:
    sys.exit(77)

start_spot_env = os.environ["SPOT_SATSOLVER"]
os.environ["SPOT_SATSOLVER"] = 'glucose -verb=0 -model %I>%O'

aut = spot.translate('GFa & GFb', 'BA')
assert aut.num_sets() == 1
assert aut.num_states() == 3
assert aut.is_deterministic()

min1 = spot.sat_minimize(aut, acc='Rabin 1')
assert min1.num_sets() == 2
assert min1.num_states() == 2

min2 = spot.sat_minimize(aut, acc='Streett 2', dichotomy=True)
assert min2.num_sets() == 4
assert min2.num_states() == 1

min3 = spot.sat_minimize(aut, acc='Rabin 2',
                         state_based=True, max_states=5, dichotomy=True)
assert min3.num_sets() == 4
assert min3.num_states() == 3

min4 = spot.sat_minimize(aut, acc='parity max odd 3',
                         colored=True, dichotomy=True)
assert min4.num_sets() == 3
assert min4.num_states() == 2

os.environ["SPOT_SATSOLVER"] = start_spot_env
