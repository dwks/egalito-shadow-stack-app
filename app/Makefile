# Makefile for egalito applications

include ../egalito/env.mk

ETAPP = $(BUILDDIR)etapp

CFLAGS      += -g -I ../egalito/src/
CXXFLAGS    += -g -I ../egalito/src/
CLDFLAGS    += -L ../egalito/src/$(BUILDDIR) -legalito \
	-Wl,-rpath=$(abspath ../egalito/src/$(BUILDDIR)) \
	-Wl,-rpath=$(abspath ../egalito/dep/capstone/install/lib)

APP_SOURCES = $(wildcard src/*.cpp)

exe-filename = $(foreach s,$1,$(BUILDDIR)$(dir $s)$(basename $(notdir $s)))
obj-filename = $(foreach s,$1,$(BUILDDIR)$(dir $s)$(basename $(notdir $s)).o)
dep-filename = $(foreach s,$1,$(BUILDDIR)$(dir $s)$(basename $(notdir $s)).d)

ETAPP_SOURCES = $(APP_SOURCES)
ETAPP_OBJECTS = $(call obj-filename,$(ETAPP_SOURCES))

ALL_SOURCES = $(sort $(ETAPP_SOURCES))
ALL_OBJECTS = $(call obj-filename,$(ALL_SOURCES))

BUILDTREE = $(sort $(dir $(ALL_OBJECTS)))


OUTPUTS = $(ETAPP)

# Default target
.PHONY: all
all: app-all .symlinks
	@true

app-all: $(OUTPUTS) | rebuild-src

ifeq ($(findstring clean,$(MAKECMDGOALS)),)
Makefile: rebuild-src
endif

$(ALL_OBJECTS): | $(BUILDTREE)
$(BUILDTREE): ../egalito/config/config.h
	@mkdir -p $@

../egalito/config/config.h:
	$(call short-make,../egalito/config)

.symlinks: $(OUTPUTS)
	@touch .symlinks
	@echo "LN-S" $(OUTPUTS)
	@ln -sf $(ETAPP)
	@ln -sf $(shell pwd)/../egalito/src/$(BUILDDIR)libegalito.so $(BUILDDIR)libegalito.so

.PHONY: rebuild-src
rebuild-src:
	$(call short-make,../egalito/src)

# Dependencies
DEPEND_FILES = $(call dep-filename,$(ALL_SOURCES))
-include $(DEPEND_FILES)

# Special files

# Programs and libraries
$(ETAPP): $(ETAPP_OBJECTS)
	$(SHORT_LINK) $(CXXFLAGS) -o $@ $^ $(CLDFLAGS) -lreadline -Wl,-rpath=$(abspath ../egalito/src)
$(ETAPP): ../egalito/src/$(BUILDDIR)libegalito.so

# Other targets
.PHONY: clean realclean
clean:
	-rm -rf $(BUILDDIR) .symlinks etapp
