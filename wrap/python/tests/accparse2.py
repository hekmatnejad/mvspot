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

import spot

a = spot.acc_cond(5)
a.set_acceptance(spot.parse_acc_code('parity min odd 5'))
assert(a.is_parity() == [True, False, True])
a.set_acceptance(spot.parse_acc_code('parity max even 5'))
assert(a.is_parity() == [True, True, False])
a.set_acceptance(spot.parse_acc_code('generalized-Buchi 5'))
assert(a.is_parity()[0] == False)
assert(a.is_parity(True)[0] == False)
a.set_acceptance(spot.parse_acc_code(
    'Inf(4) | (Fin(3)&Inf(2)) | (Fin(3)&Fin(1)&Inf(0))'))
assert(a.is_parity()[0] == False)
assert(a.is_parity(True) == [True, True, False])

a = spot.acc_cond(0)
a.set_acceptance(spot.parse_acc_code('all'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == 0)
a.set_acceptance(spot.parse_acc_code('none'))
assert(a.is_rabin() == 0)
assert(a.is_streett() == -1)

a = spot.acc_cond(2)
a.set_acceptance(spot.parse_acc_code('(Fin(0)&Inf(1))'))
assert(a.is_rabin() == 1)
assert(a.is_streett() == -1)
a.set_acceptance(spot.parse_acc_code('Inf(1)&Fin(0)'))
assert(a.is_rabin() == 1)
assert(a.is_streett() == -1)
a.set_acceptance(spot.parse_acc_code('(Fin(0)|Inf(1))'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == 1)
a.set_acceptance(spot.parse_acc_code('Inf(1)|Fin(0)'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == 1)

a = spot.acc_cond(4)
a.set_acceptance(spot.parse_acc_code('(Fin(0)&Inf(1))|(Fin(2)&Inf(3))'))
assert(a.is_rabin() == 2)
assert(a.is_streett() == -1)
a.set_acceptance(spot.parse_acc_code('(Inf(3)&Fin(2))|(Fin(0)&Inf(1))'))
assert(a.is_rabin() == 2)
assert(a.is_streett() == -1)
a.set_acceptance(spot.parse_acc_code('(Inf(2)&Fin(3))|(Fin(0)&Inf(1))'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == -1)
a.set_acceptance(spot.parse_acc_code('(Inf(3)&Fin(2))|(Fin(2)&Inf(1))'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == -1)
a.set_acceptance(spot.parse_acc_code('(Inf(1)&Fin(0))|(Fin(0)&Inf(1))'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == -1)
r = spot.parse_acc_code('(Fin(0)&Inf(1))|(Inf(1)&Fin(0))|(Inf(3)&Fin(2))')
a.set_acceptance(r)
assert(a.is_rabin() == 2)
assert(a.is_streett() == -1)

a.set_acceptance(spot.parse_acc_code('(Fin(0)|Inf(1))&(Fin(2)|Inf(3))'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == 2)
a.set_acceptance(spot.parse_acc_code('(Inf(3)|Fin(2))&(Fin(0)|Inf(1))'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == 2)
a.set_acceptance(spot.parse_acc_code('(Inf(2)|Fin(3))&(Fin(0)|Inf(1))'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == -1)
a.set_acceptance(spot.parse_acc_code('(Inf(3)|Fin(2))&(Fin(2)|Inf(1))'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == -1)
a.set_acceptance(spot.parse_acc_code('(Inf(1)|Fin(0))&(Fin(0)|Inf(1))'))
assert(a.is_rabin() == -1)
assert(a.is_streett() == -1)
r = spot.parse_acc_code('(Fin(0)|Inf(1))&(Inf(1)|Fin(0))&(Inf(3)|Fin(2))')
a.set_acceptance(r)
assert(a.is_rabin() == -1)
assert(a.is_streett() == 2)