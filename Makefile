SHELL := /bin/bash

ifeq ($(CPU_CORES),)
	CPU_CORES := $(shell nproc)
	ifeq ($(CPU_CORES),)
		CPU_CORES := $(shell sysctl -n hw.physicalcpu)
	endif
	ifeq ($(CPU_CORES),)
		CPU_CORES := 2  # Good minimum assumption
	endif
endif

ADDITIONAL_CMAKE_OPTIONS ?=
PREFIX ?= /usr/local
RULE_MESSAGES ?= off

ABC=$(PREFIX)/bin/abc
DE=$(PREFIX)/bin/de
LSORACLE=$(PREFIX)/bin/lsoracle

##
## @ release
##     |---> info       :  Release build
##     |---> usage      :  make release
release: run-cmake-release
	cmake --build build -j $(CPU_CORES)

##
## @ debug
##     |---> info       :  Debug build
##     |---> usage      :  make debug
debug: run-cmake-debug
	cmake --build dbuild -j $(CPU_CORES)

run-cmake-release:
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B build

run-cmake-debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B dbuild

##
## @ test
##     |---> info       :  Run unit tests
##     |---> usage      :  make test
test: release
	cmake --build build --target test

##
## @ clean_test
##     |---> info       :  Run unit tests
##     |---> usage      :  make clean_test
clean_test:
	cd yosys-rs-plugin && $(MAKE) $@ YOSYS_PATH=$(shell pwd)/yosys/install

##
## @ clean
##     |---> info       :  Clean all
##     |---> usage      :  make clean
clean:
ifneq ("","$(wildcard build/Makefile)")
	cmake --build build --target clean_all
endif	
ifneq ("","$(wildcard dbuild/Makefile)")
	cmake --build dbuild --target clean_all
endif	
	$(RM) -r build dbuild

##
## @ install
##     |---> info       :  Install binaries and libraries
##     |---> usage      :  make install
install: release
	cmake --install build

# exports should not be used when https://github.com/RapidSilicon/yosys_verific_rs/issues/168 is fixed
##
## @ test_install
##     |---> info       :  Test if everything is installed properly
##     |---> usage      :  make test_install
test_install:
	export ABC=$(ABC) &&\
	export DE=$(DE) &&\
	export LSORACLE=$(LSORACLE) &&\
	cd yosys-rs-plugin && $(MAKE) test YOSYS_PATH=$(PREFIX)

##
## @ uninstall
##     |---> info       :  Uninstall binaries and libraries
##     |---> usage      :  make uninstall
uninstall:
	$(RM) -r $(PREFIX)/bin/yosys*
	$(RM) -r $(PREFIX)/share/yosys
	cd logic_synthesis-rs && $(MAKE) $@

help: Makefile
	@echo '   #############################################'
	@echo '  ###############################################'
	@echo ' ###                                           ###'
	@echo '###  ###    ###  ########  ###       #########  ###'
	@echo '###  ###    ###  ########  ###       #########  ###'
	@echo '###  ###    ###  ###       ###       ###   ###  ###'
	@echo '###  ##########  ########  ###       #########  ###'
	@echo '###  ##########  ########  ###       #########  ###'
	@echo '###  ###    ###  ###       ###       ###        ###'
	@echo '###  ###    ###  ########  ########  ###        ###'
	@echo '###  ###    ###  ########  ########  ###        ###'
	@echo ' ###                                           ###'
	@echo '  ###############################################'
	@echo '   #############################################'
	@sed -n 's/^##//p' $<
