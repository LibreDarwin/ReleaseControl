##
# Standard.make - open-source Apple Release Control makefile (standard includes)
#
# Clean-room reimplementation of the interface provided by Apple's
# Core OS Makefiles "Standard" makefile set.  Preserves the public
# makefile API (variable names, targets, defaults) so that Darwin
# Core OS projects build unmodified.  Not affiliated with Apple.
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 LibreDarwin.  All rights reserved.
# See the LICENSE file for the full BSD 3-Clause license text.
##

.PHONY: default

default: all

include $(CoreOSMakefiles)/Standard/Commands.make
include $(CoreOSMakefiles)/Standard/Variables.make
include $(CoreOSMakefiles)/Standard/Implicit.make

noop: