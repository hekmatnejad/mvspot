## -*- coding: utf-8 -*-

## Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016
## Laboratoire de Recherche et Développement de l'Epita (LRDE).
## Copyright (C) 2003, 2004, 2005, 2006 Laboratoire d'Informatique de
## Paris 6 (LIP6), département Systèmes Répartis Coopératifs (SRC),
## Université Pierre et Marie Curie.
##
## This file is part of Spot, a model checking library.
##
## Spot is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## Spot is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
## or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
## License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) $(BUDDY_CPPFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS)
LDADD = $(top_builddir)/spot/libspot.la

TEST_EXTENTIONS = .test .py .ipynb .pl

LOG_COMPILER = ./run
TEST_LOG_COMPILER = ./run
LOG_DRIVER = $(TEST_LOG_DRIVER)
# ensure run is rebuilt before the tests are run.
check_SCRIPTS = run

# We try to keep this somehow by strength.  Test basic things first,
# because such failures will be easier to diagnose and fix.
TESTS = $(TESTS_sanity) $(TESTS_tl) $(TESTS_graph) $(TESTS_kripke) \
  $(TESTS_twa) $(TESTS_python) $(TESTS_ltsmin)

distclean-local:
	find . -name '*.dir' -type d -print | xargs rm -rf


############################## CORE ##############################

check_SCRIPTS += core/defs

core/defs: $(top_builddir)/config.status $(srcdir)/core/defs.in
	$(top_builddir)/config.status --file core/defs

# These are the most used test programs, and they are also useful
# to run manually outside the test suite.  Always build them.
noinst_PROGRAMS = core/ikwiad core/randtgba

# Keep this sorted alphabetically.
check_PROGRAMS = \
  core/acc \
  core/bitvect \
  core/complement \
  core/checkpsl \
  core/checkta \
  core/consterm \
  core/emptchk \
  core/equals \
  core/graph \
  core/kind \
  core/length \
  core/intvcomp \
  core/intvcmp2 \
  core/ltlprod \
  core/ltl2dot \
  core/ltl2text \
  core/ltlrel \
  core/lunabbrev \
  core/nequals \
  core/nenoform \
  core/ngraph \
  core/parse_print \
  core/readsat \
  core/reduc \
  core/reduccmp \
  core/reduceu \
  core/reductaustr \
  core/syntimpl \
  core/taatgba \
  core/tgbagraph \
  core/tostring \
  core/tunabbrev \
  core/tunenoform

# Keep this sorted alphabetically.
core_acc_SOURCES = core/acc.cc
core_bitvect_SOURCES  = core/bitvect.cc
core_checkpsl_SOURCES = core/checkpsl.cc
core_checkta_SOURCES = core/checkta.cc
core_complement_SOURCES = core/complementation.cc
core_emptchk_SOURCES  = core/emptchk.cc
core_graph_SOURCES = core/graph.cc
core_ikwiad_SOURCES = core/ikwiad.cc
core_intvcomp_SOURCES = core/intvcomp.cc
core_intvcmp2_SOURCES = core/intvcmp2.cc
core_ltlprod_SOURCES  = core/ltlprod.cc
core_ngraph_SOURCES = core/ngraph.cc
core_parse_print_SOURCES = core/parse_print_test.cc
core_randtgba_SOURCES = core/randtgba.cc
core_readsat_SOURCES = core/readsat.cc
core_taatgba_SOURCES = core/taatgba.cc
core_tgbagraph_SOURCES = core/twagraph.cc
core_consterm_SOURCES = core/consterm.cc
core_equals_SOURCES = core/equalsf.cc
core_kind_SOURCES = core/kind.cc
core_length_SOURCES = core/length.cc
core_ltl2dot_SOURCES = core/readltl.cc
core_ltl2dot_CPPFLAGS = $(AM_CPPFLAGS) -DDOTTY
core_ltl2text_SOURCES = core/readltl.cc
core_ltlrel_SOURCES = core/ltlrel.cc
core_lunabbrev_SOURCES = core/equalsf.cc
core_lunabbrev_CPPFLAGS = $(AM_CPPFLAGS) -DUNABBREV='"^ie"'
core_nenoform_SOURCES = core/equalsf.cc
core_nenoform_CPPFLAGS = $(AM_CPPFLAGS) -DNENOFORM
core_nequals_SOURCES = core/equalsf.cc
core_nequals_CPPFLAGS = $(AM_CPPFLAGS) -DNEGATE
core_reduc_SOURCES = core/reduc.cc
core_reduccmp_SOURCES = core/equalsf.cc
core_reduccmp_CPPFLAGS = $(AM_CPPFLAGS) -DREDUC
core_reduceu_SOURCES = core/equalsf.cc
core_reduceu_CPPFLAGS = $(AM_CPPFLAGS) -DREDUC -DEVENT_UNIV
core_reductaustr_SOURCES = core/equalsf.cc
core_reductaustr_CPPFLAGS = $(AM_CPPFLAGS) -DREDUC_TAUSTR
core_syntimpl_SOURCES = core/syntimpl.cc
core_tostring_SOURCES = core/tostring.cc
core_tunabbrev_SOURCES = core/equalsf.cc
core_tunabbrev_CPPFLAGS = $(AM_CPPFLAGS) -DUNABBREV='"^ieFG"'
core_tunenoform_SOURCES = core/equalsf.cc
core_tunenoform_CPPFLAGS = $(AM_CPPFLAGS) -DNENOFORM -DUNABBREV='"^ieFG"'


TESTS_tl = \
  core/bare.test \
  core/parse.test \
  core/parseerr.test \
  core/utf8.test \
  core/length.test \
  core/equals.test \
  core/tostring.test \
  core/lunabbrev.test \
  core/tunabbrev.test \
  core/nenoform.test \
  core/tunenoform.test \
  core/unabbrevwm.test \
  core/consterm.test \
  core/kind.test \
  core/remove_x.test \
  core/ltlrel.test \
  core/ltlgrind.test \
  core/ltlcrossgrind.test \
  core/ltlfilt.test \
  core/exclusive-ltl.test \
  core/latex.test \
  core/lbt.test \
  core/lenient.test \
  core/rand.test \
  core/isop.test \
  core/syntimpl.test \
  core/reduc.test \
  core/reduc0.test \
  core/reducpsl.test \
  core/reduccmp.test \
  core/uwrm.test \
  core/eventuniv.test \
  core/stutter-ltl.test

TESTS_graph = \
  core/graph.test \
  core/ngraph.test \
  core/tgbagraph.test

TESTS_kripke = \
  core/kripke.test

TESTS_twa = \
  core/acc.test \
  core/acc2.test \
  core/intvcomp.test \
  core/bitvect.test \
  core/ltlcross3.test \
  core/taatgba.test \
  core/renault.test \
  core/nondet.test \
  core/det.test \
  core/neverclaimread.test \
  core/parseaut.test \
  core/optba.test \
  core/complete.test \
  core/complement.test \
  core/remfin.test \
  core/dstar.test \
  core/readsave.test \
  core/ltldo.test \
  core/ltldo2.test \
  core/maskacc.test \
  core/maskkeep.test \
  core/prodor.test \
  core/simdet.test \
  core/sim2.test \
  core/sim3.test \
  core/ltl2tgba.test \
  core/ltl2neverclaim.test \
  core/ltl2neverclaim-lbtt.test \
  core/ltlprod.test \
  core/explprod.test \
  core/explpro2.test \
  core/explpro3.test \
  core/explpro4.test \
  core/tripprod.test \
  core/dupexp.test \
  core/exclusive-tgba.test \
  core/remprop.test \
  core/degendet.test \
  core/degenid.test \
  core/degenlskip.test \
  core/randomize.test \
  core/lbttparse.test \
  core/scc.test \
  core/sccdot.test \
  core/sccsimpl.test \
  core/sepsets.test \
  core/dbacomp.test \
  core/obligation.test \
  core/wdba.test \
  core/wdba2.test \
  core/babiak.test \
  core/monitor.test \
  core/dra2dba.test \
  core/unambig.test \
  core/ltlcross4.test \
  core/ltl3dra.test \
  core/ltl2dstar.test \
  core/ltl2dstar2.test \
  core/ltl2dstar3.test \
  core/ltl2dstar4.test \
  core/ltl2ta.test \
  core/ltl2ta2.test \
  core/randaut.test \
  core/randtgba.test \
  core/isomorph.test \
  core/uniq.test \
  core/sbacc.test \
  core/stutter-tgba.test \
  core/strength.test \
  core/emptchk.test \
  core/emptchke.test \
  core/dfs.test \
  core/ltlcrossce.test \
  core/ltlcrossce2.test \
  core/emptchkr.test \
  core/ltlcounter.test \
  core/basimul.test \
  core/satmin.test \
  core/satmin2.test \
  core/spotlbtt.test \
  core/ltlcross.test \
  core/spotlbtt2.test \
  core/ltlcross2.test \
  core/complementation.test \
  core/randpsl.test \
  core/cycles.test


############################## PYTHON ##############################

TESTS_python = \
  python/acc_cond.ipynb \
  python/accparse.ipynb \
  python/accparse2.py \
  python/alarm.py \
  python/automata.ipynb \
  python/automata-io.ipynb \
  python/bddnqueen.py \
  python/decompose.ipynb \
  python/formulas.ipynb \
  python/implies.py \
  python/interdep.py \
  python/ltl2tgba.test \
  python/ltlparse.py \
  python/ltlsimple.py \
  python/minato.py \
  python/optionmap.py \
  python/parsetgba.py \
  python/piperead.ipynb \
  python/product.ipynb \
  python/randaut.ipynb \
  python/randgen.py \
  python/randltl.ipynb \
  python/relabel.py \
  python/remfin.py \
  python/satmin.py \
  python/setxor.py \
  python/testingaut.ipynb

SUFFIXES = .ipynb .html
.ipynb.html:
	$(IPYTHON) nbconvert $< --to html --stdout >$@

.PHONY: nb-html
nb-html: $(TESTS_python:.ipynb=.html)

EXTRA_DIST = \
  $(TESTS) \
  python/ltl2tgba.py \
  python/ipnbdoctest.py


############################## LTSMIN ##############################

noinst_PROGRAMS += ltsmin/modelcheck

ltsmin_modelcheck_SOURCES = ltsmin/modelcheck.cc
ltsmin_modelcheck_LDADD = $(top_builddir)/spot/ltsmin/libspotltsmin.la

check_SCRIPTS += ltsmin/defs

ltsmin/defs: $(top_builddir)/config.status $(srcdir)/core/defs.in
	$(top_builddir)/config.status --file ltsmin/defs:core/defs.in

TESTS_ltsmin = \
  ltsmin/check.test \
  ltsmin/finite.test \
  ltsmin/finite2.test \
  ltsmin/kripke.test

EXTRA_DIST += ltsmin/beem-peterson.4.dve ltsmin/finite.dve ltsmin/finite.pm

ltlsmin/kripke.log: core/parse_print$(EXEEXT)


############################## SANITY ##############################

TESTS_sanity = \
  sanity/80columns.test \
  sanity/includes.test \
  sanity/ipynb.pl \
  sanity/private.test \
  sanity/readme.pl \
  sanity/style.test