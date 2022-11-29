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
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DPRODUCTION_BUILD=$(PRODUCTION_BUILD) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) -DUPDATE_SUBMODULES=$(UPDATE_SUBMODULES) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B build

run-cmake-debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DPRODUCTION_BUILD=$(PRODUCTION_BUILD) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) -DUPDATE_SUBMODULES=$(UPDATE_SUBMODULES) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B dbuild

##
## @ test
##     |---> info       :  Run unit tests
##     |---> usage      :  make test
test: release
	cd build && ctest -R smoke-test

##
## @ dtest
##     |---> info       :  Run unit tests for debug build
##     |---> usage      :  make dtest
dtest: debug
	cd dbuild && ctest -R smoke-test

##
## @ dtest
##     |---> info       :  Run unit tests with valgrind 
##     |---> usage      :  make dtest
valgrind:
	cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_VALGRIND_TESTS=ON -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DPRODUCTION_BUILD=$(PRODUCTION_BUILD) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) -DUPDATE_SUBMODULES=$(UPDATE_SUBMODULES) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B dbuild
	cmake --build dbuild -j $(CPU_CORES)
	cd dbuild && ctest -R valgrind-test

##
## @ clean_test
##     |---> info       :  Clean unit tests
##     |---> usage      :  make clean_test
clean_test:
	cmake --build build --target clean_analyze
ifneq ("","$(wildcard yosys/install)")
	cd yosys-rs-plugin && $(MAKE) $@ YOSYS_PATH=$(shell pwd)/yosys/install
endif
ifneq ("","$(wildcard yosys/debug-install)")
	cd yosys-rs-plugin && $(MAKE) $@ YOSYS_PATH=$(shell pwd)/yosys/debug-install
endif

##
## @ clean
##     |---> info       :  Clean all
##     |---> usage      :  make clean
clean:
ifneq ("","$(wildcard build/Makefile)")
	cmake --build build --target clean_yosys_verific_rs
endif	
ifneq ("","$(wildcard dbuild/Makefile)")
	cmake --build dbuild --target clean_yosys_verific_rs
endif	
	$(RM) -r build dbuild

##
## @ install
##     |---> info       :  Install binaries and libraries
##     |---> usage      :  make install
install: release
	cmake --install build

##
## @ test_install
##     |---> info       :  Test if everything is installed properly
##     |---> usage      :  make test_install
test_install:
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
