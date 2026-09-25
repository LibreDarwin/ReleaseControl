##
# Common.make - open-source Apple Release Control makefile (common)
#
# Clean-room reimplementation of the interface provided by Apple's
# Core OS Makefiles "Release Control" Common.make.  Preserves the
# public makefile API (variable names, targets, defaults) so that
# Darwin Core OS projects build unmodified.  Not affiliated with Apple.
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 LibreDarwin.  All rights reserved.
# See the LICENSE file for the full BSD 3-Clause license text.
##
# Set these variables as needed, then include this file:
#
#  Project           [ UNTITLED_PROJECT ]
#  ProjectName       [ $(Project)       ]
#  SubProjects
#  Extra_Environment
#  Passed_Targets
#
# Additional variables inherited from Standard/Standard.make
##

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

include $(CoreOSMakefiles)/Standard/Standard.make

##
# Defaults for the Release Control version bookkeeping variables
##

RC_ARCHS   = $(shell for i in `file /usr/lib/libSystem.B.dylib | grep 'shared library ' | sed 's|.*shared library ||'`; do $(CC) -arch $$i -E -x c /dev/null > /dev/null 2>&1 && echo $$i; done)
RC_RELEASE = unknown
RC_VERSION = unknown

# Standard build tree roots
SRCROOT = /tmp/$(ProjectName)/Sources
OBJROOT = /tmp/$(ProjectName)/Build
SYMROOT = /tmp/$(ProjectName)/Debug
DSTROOT = /tmp/$(ProjectName)/Release

##
# Project identity
##

ifndef Project
Project = UNTITLED_PROJECT
endif

ifndef ProjectName
ProjectName = $(Project)
endif

ifneq ($(RC_VERSION),unknown)
Version = $(RC_VERSION)
else
Version = $(RC_ProjectSourceVersion)
ifeq ($(Version),)
Version = 0
endif
endif

Sources        = $(SRCROOT)
Platforms      = $(patsubst %,%-apple-rhapsody$(RhapsodyVersion),$(RC_ARCHS:ppc=powerpc))
BuildDirectory = $(OBJROOT)

CC_Archs      = $(RC_ARCHS:%=-arch %)
#CPP_Defines += -DPROJECT_VERSION=\"$(Project)-$(Version)\"

##
# Environment passed down to sub-makes
##

Environment += CC="$(CC)"
Environment += CXX="$(CXX)"

ifneq "$(strip $(CFLAGS))" ""
Environment += CFLAGS="$(CFLAGS)"
endif
ifneq "$(strip $(CXXFLAGS))" ""
Environment += CCFLAGS="$(CXXFLAGS)" CXXFLAGS="$(CXXFLAGS)"
endif
ifneq "$(strip $(LDFLAGS))" ""
Environment += LDFLAGS="$(LDFLAGS)"
endif
ifneq "$(strip $(CPPFLAGS))" ""
Environment += CPPFLAGS="$(CPPFLAGS)"
endif
Environment += $(Extra_Environment)

VPATH=$(Sources)

ManPageDirectories = /usr/share/man

##
# Targets
##

.PHONY: all install installhdrs install_headers lazy_installsrc lazy_install_source installsrc install_source build clean recurse

all: build

$(DSTROOT): install

install:: install_headers build

# For RC
installhdrs:: install_headers

install_headers::

lazy_install_source::
	$(_v) if [ ! -f "$(SRCROOT)/Makefile" ]; then $(MAKE) install_source; fi

install_source::
ifneq ($(CommonNoInstallSource),YES)
	@echo "Installing source for $(Project)..."
	$(_v) $(MKDIR) "$(SRCROOT)"
	$(_v) $(TAR) -cp $(Tar_Cruft) . | $(TAR) -pox -C "$(SRCROOT)"
endif

ifndef ShadowTestFile
ShadowTestFile = $(BuildDirectory)/Makefile
endif

shadow_source:: $(ShadowTestFile)

$(ShadowTestFile):
	echo "Creating pseudo-copy of sources in the build directory...";
	$(_v) mkdir -p $(BuildDirectory);
	$(_v) for dir in $$( cd $(Sources) && $(FIND) . -type d ); do			\
	        cd $(BuildDirectory) && if [ ! -d $$dir ]; then $(MKDIR) $$dir; fi;	\
	      done
	$(_v) for file in $$( cd $(Sources) && $(FIND) . -type f ); do			\
	        cd $(BuildDirectory) && $(LN) -fs $(Sources)/$$file $$file;		\
	      done

# For RC
installsrc: install_source

build:: lazy_install_source

clean::
	@echo "Cleaning $(Project)..."
	$(_v) $(RMDIR) -f "$(BuildDirectory)" || true

$(Passed_Targets) $(Extra_Passed_Targets):
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $@

recurse:
ifdef SubProjects
	$(_v) for SubProject in $(SubProjects); do				\
		$(MAKE) -C $$SubProject $(TARGET)				\
			OBJROOT=$(OBJROOT)					\
			SYMROOT=$(SYMROOT)					\
			DSTROOT=$(DSTROOT)					\
		        BuildDirectory=$(BuildDirectory)/$${SubProject}		\
		               Sources=$(Sources)/$${SubProject}		\
		       CoreOSMakefiles=$(CoreOSMakefiles) || exit 1 ;		\
	      done
endif

rshowvar: showvar
	$(_v) $(MAKE) recurse TARGET=rshowvar

compress_man_pages:
	@echo "Man pages are no longer compressed.  Please remove the compress_man_pages target from your Makefile."