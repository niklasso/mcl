## TODO ###########################################################################################
#

.PHONY:	lr ld lp lsh config all
all:	lr lsh

## Load Previous Configuration ####################################################################

-include config.mk

## Configurable options ###########################################################################

# Directory to store object files, libraries, executables, and dependencies:
BUILD_DIR  ?= build

# Include debug-symbols in release builds
MCL_RELSYM ?= -g

# Sets of compile flags for different build types
MCL_REL    ?= -O3 -D NDEBUG
MCL_DEB    ?= -O0 -D DEBUG 
MCL_PRF    ?= -O3 -D NDEBUG
MCL_FPIC   ?= -fpic

# Dependencies
MINISAT_INCLUDE?=
MINISAT_LIB    ?=-Lminisat

# GNU Standard Install Prefix
prefix         ?= /usr/local

## Write Configuration  ###########################################################################

config:
	rm -rf config.mk
	echo 'BUILD_DIR?=$(BUILD_DIR)'           >> config.mk
	echo 'MCL_RELSYM?=$(MCL_RELSYM)' >> config.mk
	echo 'MCL_REL?=$(MCL_REL)'       >> config.mk
	echo 'MCL_DEB?=$(MCL_DEB)'       >> config.mk
	echo 'MCL_PRF?=$(MCL_PRF)'       >> config.mk
	echo 'MCL_FPIC?=$(MCL_FPIC)'     >> config.mk
	echo 'MINISAT_INCLUDE?=$(MINISAT_INCLUDE)' >> config.mk
	echo 'MINISAT_LIB?=$(MINISAT_LIB)' >> config.mk
	echo 'prefix?=$(prefix)'                 >> config.mk

## Configurable options end #######################################################################

INSTALL ?= install

# GNU Standard Install Variables
exec_prefix ?= $(prefix)
includedir  ?= $(prefix)/include
bindir      ?= $(exec_prefix)/bin
libdir      ?= $(exec_prefix)/lib
datarootdir ?= $(prefix)/share
mandir      ?= $(datarootdir)/man

# Target file names
MCL_SLIB = libmcl.a#  Name of MCL static library.
MCL_DLIB = libmcl.so# Name of MCL shared library.

# Shared Library Version
SOMAJOR=1
SOMINOR=0
SORELEASE=0

MCL_CXXFLAGS = -I. -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -Wall -Wno-parentheses -Wextra $(MINISAT_INCLUDE)
MCL_LDFLAGS  = -Wall -lz $(MINISAT_LIB)

ifeq ($(VERB),)
ECHO=@
VERB=@
else
ECHO=#
VERB=
endif

SRCS = $(wildcard mcl/*.cc)
HDRS = $(wildcard mcl/*.h)
OBJS = $(SRCS:.cc=.o)

lr:	$(BUILD_DIR)/release/lib/$(MCL_SLIB)
ld:	$(BUILD_DIR)/debug/lib/$(MCL_SLIB)
lp:	$(BUILD_DIR)/profile/lib/$(MCL_SLIB)
lsh:	$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR).$(SORELEASE)

## Build-type Compile-flags:
$(BUILD_DIR)/release/%.o:			MCL_CXXFLAGS +=$(MCL_REL) $(MCL_RELSYM)
$(BUILD_DIR)/debug/%.o:				MCL_CXXFLAGS +=$(MCL_DEB) -g
$(BUILD_DIR)/profile/%.o:			MCL_CXXFLAGS +=$(MCL_PRF) -pg
$(BUILD_DIR)/dynamic/%.o:			MCL_CXXFLAGS +=$(MCL_REL) $(MCL_FPIC)

## Library dependencies
$(BUILD_DIR)/release/lib/$(MCL_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/release/$(o))
$(BUILD_DIR)/debug/lib/$(MCL_SLIB):		$(foreach o,$(OBJS),$(BUILD_DIR)/debug/$(o))
$(BUILD_DIR)/profile/lib/$(MCL_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/profile/$(o))
$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR).$(SORELEASE):	$(foreach o,$(OBJS),$(BUILD_DIR)/dynamic/$(o))

## Compile rules (these should be unified, buit I have not yet found a way which works in GNU Make)
$(BUILD_DIR)/release/%.o:	%.cc
	$(ECHO) echo Compiling: $@
	$(VERB) mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(VERB) $(CXX) $(MCL_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

$(BUILD_DIR)/profile/%.o:	%.cc
	$(ECHO) echo Compiling: $@
	$(VERB) mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(VERB) $(CXX) $(MCL_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

$(BUILD_DIR)/debug/%.o:	%.cc
	$(ECHO) echo Compiling: $@
	$(VERB) mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(VERB) $(CXX) $(MCL_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

$(BUILD_DIR)/dynamic/%.o:	%.cc
	$(ECHO) echo Compiling: $@
	$(VERB) mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(VERB) $(CXX) $(MCL_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

## Static Library rule
%/lib/$(MCL_SLIB):
	$(ECHO) echo Linking Static Library: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(AR) -rcs $@ $^

## Shared Library rule
$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR).$(SORELEASE):
	$(ECHO) echo Linking Shared Library: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(CXX) -o $@ -shared -Wl,-soname,$(MCL_DLIB).$(SOMAJOR) $^ $(MCL_LDFLAGS)

## Shared Library links
#$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR):	$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR).$(SORELEASE)
#	ln -sf -T $(notdir $^) $@
#
#$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB):	$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR)
#	ln -sf -T $(notdir $^) $@

install:	install-headers install-lib

install-headers:
#       Create directories
	$(INSTALL) -d $(DESTDIR)$(includedir)/mcl
#       Install headers
	for h in $(HDRS) ; do \
	  $(INSTALL) -m 644 $$h $(DESTDIR)$(includedir)/$$h ; \
	done

install-lib: $(BUILD_DIR)/release/lib/$(MCL_SLIB) $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR).$(SORELEASE) #$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR) $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB) 
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR).$(SORELEASE) $(DESTDIR)$(libdir)
#	$(INSTALL) -m 644 $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR) $(DESTDIR)$(libdir)
#	$(INSTALL) -m 644 $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB) $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 $(BUILD_DIR)/release/lib/$(MCL_SLIB) $(DESTDIR)$(libdir)

## Include generated dependencies
## NOTE: dependencies are assumed to be the same in all build modes at the moment!
-include $(foreach s, $(SRCS:.cc=.d), $(BUILD_DIR)/dep/$s)
