# Copyright (C) 2022 RapidSilicon..

# Either find yosys in system and use its path or use the given path
YOSYS_PATH ?= $(realpath $(dir $(shell which yosys))/..)

# Find yosys-config, throw an error if not found
YOSYS_CONFIG ?= $(YOSYS_PATH)/bin/yosys-config
ifeq (,$(wildcard $(YOSYS_CONFIG)))
$(error "Didn't find 'yosys-config' under '$(YOSYS_PATH)'")
endif

CXX ?= $(shell $(YOSYS_CONFIG) --cxx)
CXXFLAGS ?= $(shell $(YOSYS_CONFIG) --cxxflags) #-DSDC_DEBUG
LDFLAGS ?= $(shell $(YOSYS_CONFIG) --ldflags)
LDLIBS ?= $(shell $(YOSYS_CONFIG) --ldlibs)
PLUGINS_DIR ?= $(shell $(YOSYS_CONFIG) --datdir)/plugins
DATA_DIR ?= $(shell $(YOSYS_CONFIG) --datdir)
EXTRA_FLAGS ?=

COMMON			= ../yosys-rs-plugin/common
GENESIS			= ../yosys-rs-plugin/genesis
GENESIS2		= ../yosys-rs-plugin/genesis2
GENESIS3		= ../yosys-rs-plugin/genesis3

NAME = pow-extract
SOURCES = src/rs_pow_extract.cc 

OBJS := $(SOURCES:cc=o)

all: $(NAME).so

$(OBJS): %.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(EXTRA_FLAGS) -c -o $@ $(filter %.cc, $^)

$(NAME).so: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $^ $(LDLIBS)

install_plugin: $(NAME).so
	install -D $< $(PLUGINS_DIR)/$<

install_modules: 
	$(foreach f,$^,install -D $(f) $(DATA_DIR)/rapidsilicon/$(f);)

.PHONY: install
install: install_plugin install_modules

valgrind_gen:
	$(MAKE) -C tests valgrind_gen YOSYS_PATH=$(YOSYS_PATH)

valgrind:
	$(MAKE) -C tests valgrind_gen2 YOSYS_PATH=$(YOSYS_PATH)

test_gen:
	$(MAKE) -C tests tests_gen YOSYS_PATH=$(YOSYS_PATH)

test:
	$(MAKE) -C tests tests_gen2 YOSYS_PATH=$(YOSYS_PATH)

clean:
	rm -rf src/*.d src/*.o *.so pmgen/
	