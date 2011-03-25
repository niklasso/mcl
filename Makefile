###################################################################################################

.PHONY:	lr ld lp lsh config all install install-headers install-lib clean distclean
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
MINISAT_LIB    ?=-lminisat

# GNU Standard Install Prefix
prefix         ?= /usr/local

## Write Configuration  ###########################################################################

config:
	@( echo 'BUILD_DIR?=$(BUILD_DIR)'            ; \
	   echo 'MCL_RELSYM?=$(MCL_RELSYM)'	     ; \
	   echo 'MCL_REL?=$(MCL_REL)'      	     ; \
	   echo 'MCL_DEB?=$(MCL_DEB)'      	     ; \
	   echo 'MCL_PRF?=$(MCL_PRF)'      	     ; \
	   echo 'MCL_FPIC?=$(MCL_FPIC)'    	     ; \
	   echo 'MINISAT_INCLUDE?=$(MINISAT_INCLUDE)'; \
	   echo 'MINISAT_LIB?=$(MINISAT_LIB)'	     ; \
	   echo 'prefix?=$(prefix)'                  ) > config.mk

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
SORELEASE=.0

MCL_CXXFLAGS = -I. -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -Wall -Wno-parentheses -Wextra $(MINISAT_INCLUDE)
MCL_LDFLAGS  = -Wall -lz $(MINISAT_LIB)

ECHO=@echo
ifeq ($(VERB),)
VERB=@
else
VERB=
endif

SRCS = $(wildcard mcl/*.cc)
HDRS = $(wildcard mcl/*.h)
OBJS = $(SRCS:.cc=.o)

lr:	$(BUILD_DIR)/release/lib/$(MCL_SLIB)
ld:	$(BUILD_DIR)/debug/lib/$(MCL_SLIB)
lp:	$(BUILD_DIR)/profile/lib/$(MCL_SLIB)
lsh:	$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE)

## Build-type Compile-flags:
$(BUILD_DIR)/release/%.o:			MCL_CXXFLAGS +=$(MCL_REL) $(MCL_RELSYM)
$(BUILD_DIR)/debug/%.o:				MCL_CXXFLAGS +=$(MCL_DEB) -g
$(BUILD_DIR)/profile/%.o:			MCL_CXXFLAGS +=$(MCL_PRF) -pg
$(BUILD_DIR)/dynamic/%.o:			MCL_CXXFLAGS +=$(MCL_REL) $(MCL_FPIC)

## Library dependencies
$(BUILD_DIR)/release/lib/$(MCL_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/release/$(o))
$(BUILD_DIR)/debug/lib/$(MCL_SLIB):		$(foreach o,$(OBJS),$(BUILD_DIR)/debug/$(o))
$(BUILD_DIR)/profile/lib/$(MCL_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/profile/$(o))
$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE):	$(foreach o,$(OBJS),$(BUILD_DIR)/dynamic/$(o))

## Compile rules (these should be unified, buit I have not yet found a way which works in GNU Make)
$(BUILD_DIR)/release/%.o:	%.cc
	$(ECHO) Compiling: $@
	$(VERB) mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(VERB) $(CXX) $(MCL_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

$(BUILD_DIR)/profile/%.o:	%.cc
	$(ECHO) Compiling: $@
	$(VERB) mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(VERB) $(CXX) $(MCL_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

$(BUILD_DIR)/debug/%.o:	%.cc
	$(ECHO) Compiling: $@
	$(VERB) mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(VERB) $(CXX) $(MCL_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

$(BUILD_DIR)/dynamic/%.o:	%.cc
	$(ECHO) Compiling: $@
	$(VERB) mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(VERB) $(CXX) $(MCL_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

## Static Library rule
%/lib/$(MCL_SLIB):
	$(ECHO) Linking Static Library: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(AR) -rcs $@ $^

## Shared Library rule
$(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE)\
 $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR)\
 $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB):
	$(ECHO) Linking Shared Library: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(CXX) $(MCL_LDFLAGS) $(LDFLAGS) -o $@ -shared -Wl,-soname,$(MCL_DLIB).$(SOMAJOR) $^
	$(VERB) ln -sf $(MCL_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE) $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR)
	$(VERB) ln -sf $(MCL_DLIB).$(SOMAJOR) $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB)

install:	install-headers install-lib
install-debug:	install-headers install-lib-debug

install-headers:
#       Create directories
	$(INSTALL) -d $(DESTDIR)$(includedir)/mcl
#       Install headers
	for h in $(HDRS) ; do \
	  $(INSTALL) -m 644 $$h $(DESTDIR)$(includedir)/$$h ; \
	done

install-lib-debug: $(BUILD_DIR)/debug/lib/$(MCL_SLIB)
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 $(BUILD_DIR)/debug/lib/$(MCL_SLIB) $(DESTDIR)$(libdir)

install-lib: $(BUILD_DIR)/release/lib/$(MCL_SLIB) $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE)
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE) $(DESTDIR)$(libdir)
	ln -sf $(DESTDIR)$(libdir)/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE) $(DESTDIR)$(libdir)/$(MCL_DLIB).$(SOMAJOR)
	ln -sf $(DESTDIR)$(libdir)/$(MCL_DLIB).$(SOMAJOR) $(DESTDIR)$(libdir)/$(MCL_DLIB)
	$(INSTALL) -m 644 $(BUILD_DIR)/release/lib/$(MCL_SLIB) $(DESTDIR)$(libdir)

clean:
	rm -f $(foreach t, release debug profile dynamic, $(foreach o, $(SRCS:.cc=.o), $(BUILD_DIR)/$t/$o)) \
	  $(foreach d, $(SRCS:.cc=.d), $(BUILD_DIR)/dep/$d) \
	  $(foreach t, release debug profile, $(BUILD_DIR)/$t/lib/$(MCL_SLIB)) \
	  $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE)\
	  $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB).$(SOMAJOR)\
	  $(BUILD_DIR)/dynamic/lib/$(MCL_DLIB)

distclean:	clean
	rm -f config.mk

## Include generated dependencies
## NOTE: dependencies are assumed to be the same in all build modes at the moment!
-include $(foreach s, $(SRCS:.cc=.d), $(BUILD_DIR)/dep/$s)
