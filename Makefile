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
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B build

run-cmake-debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B dbuild

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
