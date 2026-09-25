##
# GNUSource.make - open-source Apple Release Control makefile
#
# Clean-room reimplementation of the interface provided by Apple's
# Core OS Makefiles "Release Control" GNUSource.make.  Wraps GNU
# autoconf projects to build under Release Control.  Preserves the
# public makefile API (variable names, targets, defaults) so that
# Darwin Core OS projects build unmodified.  Not affiliated with Apple.
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 LibreDarwin.  All rights reserved.
# See the LICENSE file for the full BSD 3-Clause license text.
##
# Set these variables as needed, then include this file:
#
#  Install_Prefix        [ $(USRDIR)                              ]
#  Install_Man           [ $(MANDIR)                              ]
#  Install_Info          [ $(SHAREDIR)/info                       ]
#  Install_HTML          [ <depends>                              ]
#  Install_Source        [ $(NSSOURCEDIR)/Commands/$(ProjectName) ]
#  Configure             [ $(Sources)/configure                   ]
#  Extra_Configure_Flags
#  Extra_Install_Flags
#  Passed_Targets        [ check                                  ]
#
# Additional variables inherited from ReleaseControl/Common.make
##

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

Passed_Targets += check

include $(CoreOSMakefiles)/ReleaseControl/Common.make

##
# My variables
##

Sources     = $(SRCROOT)/$(Project)
ConfigStamp = $(BuildDirectory)/configure-stamp

ifndef Install_Prefix
Install_Prefix = $(USRDIR)
endif
ifndef Install_Man
Install_Man = $(MANDIR)
endif
ifndef Install_Info
Install_Info = $(SHAREDIR)/info
endif
ifndef Install_HTML
Install_HTML = $(SHAREDIR)/html/$(ProjectName)
endif
ifndef Install_Source
Install_Source = $(NSSOURCEDIR)/$(ToolType)/$(ProjectName)
endif

RC_Install_Prefix = $(DSTROOT)$(Install_Prefix)
RC_Install_Man    = $(DSTROOT)$(Install_Man)
RC_Install_Info   = $(DSTROOT)$(Install_Info)
RC_Install_HTML   = $(DSTROOT)$(Install_HTML)
ifneq ($(Install_Source),)
RC_Install_Source = $(DSTROOT)$(Install_Source)
endif

ifndef Configure
Configure = $(Sources)/configure
endif

Environment += TEXI2HTML="$(TEXI2HTML) -subdir ."

# Dependency tracking with the -M* options is broken for multi-arch
# builds on modern compilers (radar 4158518); turn it off for every
# configure-based project so the build does not fail.
Configure_Flags = --prefix="$(Install_Prefix)"	\
		  --mandir="$(Install_Man)"	\
		  --infodir="$(Install_Info)"	\
		  --disable-dependency-tracking \
		  $(Extra_Configure_Flags)

ifndef Configure_Products
Configure_Products = config.h config.log
endif

# For backward compatibility; most projects override this
Extra_Make_Flags ?= $(Environment)

Make_Flags = $(Extra_Make_Flags)

Install_Flags = prefix="$(RC_Install_Prefix)"	\
		mandir="$(RC_Install_Man)"	\
	       infodir="$(RC_Install_Info)"	\
	       htmldir="$(RC_Install_HTML)"	\
	               $(Extra_Install_Flags)

Install_Target = install-strip

##
# Targets
##

.PHONY: configure almostclean

install:: build
ifneq ($(GnuNoInstall),YES)
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Make_Flags) $(Install_Flags) $(Install_Target)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) -depth -exec $(RMDIR) "{}" \;
	$(_v) $(FIND) $(SYMROOT) $(Find_Cruft) -depth -exec $(RMDIR) "{}" \;
ifneq ($(GnuNoChown),YES)
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT) $(SYMROOT)
endif
endif
ifdef GnuAfterInstall
	$(_v) $(MAKE) $(GnuAfterInstall)
endif

build:: configure
ifneq ($(GnuNoBuild),YES)
	@echo "Building $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory) $(Make_Flags)
endif

configure:: lazy_install_source $(ConfigStamp)

reconfigure::
	$(_v) $(RM) $(ConfigStamp)
	$(_v) $(MAKE) configure

$(ConfigStamp):
ifneq ($(GnuNoConfigure),YES)
	@echo "Configuring $(Project)..."
	$(_v) $(MKDIR) $(BuildDirectory)
# Disable LD_TRACE_FILE during configure
	$(_v) cd $(BuildDirectory) && $(Environment) $(Extra_Configure_Environment) LD_TRACE_FILE=/dev/null $(Configure) $(Configure_Flags)
ifneq ($(Configure_Products),)
	$(_v) - $(CP) $(foreach PRODUCT,$(Configure_Products),$(BuildDirectory)/$(PRODUCT)) $(SYMROOT)
endif
endif
	$(_v) touch $@

almostclean::
ifneq ($(GnuNoClean),YES)
	@echo "Cleaning $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory) clean
endif