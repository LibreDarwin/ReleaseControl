##
# Implicit.make - open-source Apple Release Control makefile (implicit rules)
#
# Clean-room reimplementation of the interface provided by Apple's
# Core OS Makefiles "Standard" implicit rules.  Preserves the public
# makefile API (variable names, targets, defaults) so that Darwin
# Core OS projects build unmodified.  Not affiliated with Apple.
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 LibreDarwin.  All rights reserved.
# See the LICENSE file for the full BSD 3-Clause license text.
##

##
# Compiler options
##

CC_Archs    =
CC_Debug    = -g
ifeq ($(DEBUG),YES)
CC_Optimize =
else
CC_Optimize = -Os
endif
CC_Other    = -pipe

# Frameworks to link against
Frameworks = $(Extra_Frameworks)

# C preprocessor options
CPP_Defines  = $(Extra_CPP_Defines)
CPP_Includes = $(Extra_CPP_Includes)

# Linker options
LD_Libraries = $(Extra_LD_Libraries)

# CC/CPP/LD flag aggregates
CPP_Flags = $(CPP_Defines)            $(CPP_Includes)            $(Extra_CPP_Flags)
LD_Flags  = $(CC_Archs) $(Frameworks) $(LD_Libraries)             $(Extra_LD_Flags)
CC_Flags  = $(CC_Archs) $(CC_Debug) $(CC_Optimize) $(CC_Other)    $(Extra_CC_Flags)
Cxx_Flags = $(CC_Archs) $(CC_Debug) $(CC_Optimize) $(CC_Other)    $(Extra_Cxx_Flags)

# Compatible with the standard implicit variable names
CPPFLAGS = $(CPP_Flags)
CFLAGS   = $(CC_Flags)
CXXFLAGS = $(Cxx_Flags)
LDFLAGS  = $(LD_Flags)

##
# Targets
##

# C / Objective-C / C++ compilation

%.o: %.c
	@echo "Compiling "$@"..."
	$(_v) $(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BuildDirectory)/%.o: %.c
	@echo "Compiling "$@"..."
	$(_v) $(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(CC_Excecutable): $(CC_Objects)
	@echo "Linking "$@"..."
	$(_v) $(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

%: %.o
	@echo "Linking "$@"..."
	$(_v) $(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

# Shell scripts

%: %.sh
	@echo "Copying shell script "$@"..."
	$(_v) $(CP) $< $@
	$(_v) $(CHMOD) ugo+x $@

$(BuildDirectory)/%: %.sh
	@echo "Copying shell script "$@"..."
	$(_v) $(CP) $< $@
	$(_v) $(CHMOD) ugo+x $@