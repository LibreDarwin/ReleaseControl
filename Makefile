##
# Makefile - open-source Apple Release Control makefile framework installer
#
# Installs the Release Control framework into the configured
# Core OS makefile path, generating the GNU make flavor (.make) of
# the dual-flavor standard includes with unifdef.  Clean-room
# reimplementation of the interface provided by Apple's Core OS
# Makefiles installer.  Not affiliated with Apple.
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 LibreDarwin.  All rights reserved.
# See the LICENSE file for the full BSD 3-Clause license text.
##

Project     = CoreOSMakefiles
Destination = $(DEVELOPER_DIR)/Makefiles/CoreOS

BSDSUFFIX   = .mk
BSDDEFINE   = BSDMAKESTYLE
GNUSUFFIX   = .make
INBASENAMES = Commands Variables
INSUFFIX    = .in
STANDARD    = Standard

install:
	@$(MAKE) installsrc SRCROOT=$(DSTROOT)$(Destination)
	rm -f $(DSTROOT)$(Destination)/Makefile
	@set -x && \
	    for i in $(INBASENAMES); do \
		unifdef -U$(BSDDEFINE) -t $(DSTROOT)$(Destination)/$(STANDARD)/$$i$(INSUFFIX) > $(DSTROOT)$(Destination)/$(STANDARD)/$$i$(GNUSUFFIX); \
		[ $$? -eq 1 ] || exit 1; \
		$(RM) -f $(DSTROOT)$(Destination)/$(STANDARD)/$$i$(INSUFFIX) || exit 1; \
	    done

installhdrs:
	$(_v) echo No headers to install

installsrc:
	install -d "$(SRCROOT)"
	rsync -a --exclude=.svn --exclude=.git \
	      --exclude=local --exclude=vendor --exclude=xbs \
	      --exclude=build ./ "$(SRCROOT)"

clean:
	$(_v) echo Nothing to clean