# -*- Makefile -*-
#
# ----------------------------------------------------------------------
#
# Brad T. Aagaard, U.S. Geological Survey
# Charles A. Williams, GNS Science
# Matthew G. Knepley, University of Chicago
#
# This code was developed as part of the Computational Infrastructure
# for Geodynamics (http://geodynamics.org).
#
# Copyright (c) 2010-2017 University of California, Davis
#
# See COPYING for license information.
#
# ----------------------------------------------------------------------
#

TESTS = testcontrib.py
dist_check_SCRIPTS = testcontrib.py

noinst_PYTHON = \
	TestViscousFriction.py 


check-local: check-TESTS
	for f in $(TEST_LOGS); do printf "\n=== $$f ===\n\n"; cat $$f; done

# End of file 
