## Set variables
CURRENT_SOURCE_DIR := $(shell pwd)
NUM_CPU := 16
ABCEXTERNAL := $(CURRENT_SOURCE_DIR)/logic_synthesis-rs/abc-rs/abc
YOSYS_PATH := $(CURRENT_SOURCE_DIR)/yosys/install
VERIFIC_DIR := $(CURRENT_SOURCE_DIR)/verific/verific-vJan22
ABC_MK_ARGS := -j $(NUM_CPU)
VERIFIC_MK_ARGS := VERSION="-O3" TOPFLAGS="-I/usr/include/tcl -fPIC -std=c++11" -j $(NUM_CPU)
YOSYS_MK_VERIFIC_ARGS := ENABLE_VERIFIC=1 DISABLE_VERIFIC_EXTENSIONS=1 VERIFIC_DIR=$(VERIFIC_DIR)
YOSYS_MK_ARGS := CONFIG=gcc PREFIX=$(YOSYS_PATH) ABCEXTERNAL=$(ABCEXTERNAL) -j $(NUM_CPU)
YOSYS_PLUGINS_MK_ARGS := YOSYS_PATH=$(YOSYS_PATH) EXTRA_FLAGS="-DPASS_NAME=synth_ql" -j $(NUM_CPU)
YOSYS_RS_PLUGIN_MK_ARGS := YOSYS_PATH=$(YOSYS_PATH) -j $(NUM_CPU)
LSORACLE_MK_ARGS := -j $(NUM_CPU)
CMAKE_COMMAND := cmake
LSORACLE_CMAKE_ARGS := -DCMAKE_BUILD_TYPE=RELEASE
LSORACLE_GCC_VERSION := (GCC) 8
CC :=
CXX :=

CC_PATHS := $(shell which -a gcc)
CXX_PATHS := $(shell which -a g++)

define gcc_version =
ifneq (,$(findstring $(LSORACLE_GCC_VERSION),$(shell $(1) --version)))
    CC := $(1)
endif
endef

define g++_version =
ifneq (,$(findstring $(LSORACLE_GCC_VERSION),$(shell $(1) --version)))
    CXX := $(1)
endif
endef

$(foreach path,$(CC_PATHS),$(eval $(call gcc_version,$(path))))
$(foreach path,$(CXX_PATHS),$(eval $(call g++_version,$(path))))

##
## @ build_yosys_verific
##     |---> info       :  Build abc-rs, yosys with Verific enabled, yosys-rs-plugin and yosys-plugins
##     |---> usage      :  make build_yosys_verific
build_yosys_verific: build_verific
	cd logic_synthesis-rs/abc-rs && $(MAKE) $(ABC_MK_ARGS)
	cd yosys && $(MAKE) install $(YOSYS_MK_ARGS) $(YOSYS_MK_VERIFIC_ARGS)
	cd yosys-plugins && $(MAKE) install_ql-qlf $(YOSYS_PLUGINS_MK_ARGS)
	cd yosys-rs-plugin && $(MAKE) install $(YOSYS_RS_PLUGIN_MK_ARGS)

##
## @ all
##     |---> info       :  Build all
##     |---> usage      :  make all
all: build_yosys_verific build_lsoracle

##
## @ build_yosys
##     |---> info       :  Build abc-rs, yosys, yosys-rs-plugin and yosys-plugins
##     |---> usage      :  make build_yosys
build_yosys:
	cd logic_synthesis-rs/abc-rs && $(MAKE) $(ABC_MK_ARGS)
	cd yosys && $(MAKE) install $(YOSYS_MK_ARGS) 
	cd yosys-plugins && $(MAKE) install_ql-qlf $(YOSYS_PLUGINS_MK_ARGS)
	cd yosys-rs-plugin && $(MAKE) install $(YOSYS_RS_PLUGIN_MK_ARGS)

##
## @ build_verific
##     |---> info       :  Build Verific
##     |---> usage      :  make build_verific
build_verific: 
	cd verific/verific-vJan22/tclmain && $(MAKE) $(VERIFIC_MK_ARGS)

##
## @ build_lsoracle
##     |---> info       :  Build LSOracle
##     |---> usage      :  make build_lsoracle
build_lsoracle:
ifeq (,$(CC))
	$(error No compatible GCC version to build LSOracle)
endif
ifeq (,$(CXX))
	$(error No compatible G++ version to build LSOracle)
endif

ifeq (,$(wildcard logic_synthesis-rs/LSOracle-rs/build))
	mkdir logic_synthesis-rs/LSOracle-rs/build
endif
	cd logic_synthesis-rs/LSOracle-rs/build &&\
	export CC=$(CC) && export CXX=$(CXX) &&\
	$(CMAKE_COMMAND) $(LSORACLE_CMAKE_ARGS) .. &&\
	$(MAKE) $(LSORACLE_MK_ARGS)

##
## @ init_submodules
##     |---> info       :  Initialize and update all submodules
##     |---> usage      :  make init_submodules
init_submodules:
	git submodule update --init --recursive yosys yosys-plugins verific RTL_Benchmark yosys-rs-plugin
	git submodule update --remote --recursive yosys yosys-plugins verific RTL_Benchmark yosys-rs-plugin
	git submodule update --init --remote logic_synthesis-rs
	cd logic_synthesis-rs && git submodule update --init --recursive
	cd logic_synthesis-rs/abc-rs && git fetch && git checkout main && git pull
	cd logic_synthesis-rs/LSOracle-rs && git fetch && git checkout main && git pull
	cd logic_synthesis-rs/LSOracle-rs && git submodule update --init --recursive

##
## @ clean
##     |---> info       :  Clean all generated files
##     |---> usage      :  make clean
clean: clean_yosys_verific clean_lsoracle

##
## @ clean_yosys_verific
##     |---> info       :  Clean Yosys and Verific generated files
##     |---> usage      :  make clean_yosys_verific
clean_yosys_verific: clean_yosys clean_verific

##
## @ clean_yosys
##     |---> info       :  Clean yosys, abc-rs, yosys-rs-plugin and yosys-plugins submodules generated files
##     |---> usage      :  make clean_yosys
clean_yosys:
ifneq ("","$(wildcard $(YOSYS_PATH))")
ifneq ("","$(wildcard yosys/Makefile)")
	cd yosys && $(MAKE) clean
endif	
ifneq ("","$(wildcard yosys-plugins/Makefile)")
	cd yosys-plugins && $(MAKE) clean
endif
ifneq ("","$(wildcard ./yosys-rs-plugin/Makefile)")
	cd yosys-rs-plugin && $(MAKE) clean
endif
ifneq ("","$(wildcard ./logic_synthesis-rs/abc-rs/Makefile)")
	cd logic_synthesis-rs/abc-rs && $(MAKE) clean
endif
	rm -rf $(YOSYS_PATH)
endif

##
## @ clean_verific
##     |---> info       :  Clean verific_rs submodule generated files
##     |---> usage      :  make clean_verific
clean_verific:
ifneq ("","$(wildcard ./verific/verific-v*/tclmain/Makefile)")
	cd verific/verific-v*/tclmain && $(MAKE) clean
	cd verific && git restore .
endif

##
## @ clean_lsoracle
##     |---> info       :  Clean logic_synthesis-rs/LSOracle-rs submodule generated files
##     |---> usage      :  make clean_lsoracle
clean_lsoracle:
ifneq ("","$(wildcard ./logic_synthesis-rs/LSOracle-rs/build/Makefile)")
	cd logic_synthesis-rs/LSOracle-rs/build && $(MAKE) clean
endif

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
