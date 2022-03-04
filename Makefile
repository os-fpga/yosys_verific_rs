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


##
## @ all 
##     |---> info       :  Checkout all submodules, buils yosys with verific enabled
##     |---> usage      :  make all
all: co_benchmarks co_and_build_yosys_verific

##
## @ co_and_build_yosys_verific
##     |---> info       :  Checkout and compile yosys with Verific enabled, yosys-rs-plugin and yosys-plugins
##     |---> usage      :  make build_yosys_verific
co_and_build_yosys_verific: clean_yosys clean_verific co_yosys co_verific build_yosys_verific

##
## @ build_yosys_verific
##     |---> info       :  Compile yosys with Verific enabled, yosys-rs-plugin and yosys-plugins
##     |---> usage      :  make build_yosys_verific
build_yosys_verific: build_verific
	cd logic_synthesis-rs/abc-rs && $(MAKE) $(ABC_MK_ARGS)
	cd yosys && $(MAKE) install $(YOSYS_MK_ARGS) $(YOSYS_MK_VERIFIC_ARGS)
	cd yosys-plugins && $(MAKE) install_ql-qlf $(YOSYS_PLUGINS_MK_ARGS)
	cd yosys-rs-plugin && $(MAKE) install $(YOSYS_RS_PLUGIN_MK_ARGS)

##
## @ build_yosys
##     |---> info       :  Compile yosys, yosys-rs-plugin and yosys-plugins
##     |---> usage      :  make build_yosys
build_yosys:
	cd logic_synthesis-rs/abc-rs && $(MAKE) $(ABC_MK_ARGS)
	cd yosys && $(MAKE) install $(YOSYS_MK_ARGS) 
	cd yosys-plugins && $(MAKE) install_ql-qlf $(YOSYS_PLUGINS_MK_ARGS)
	cd yosys-rs-plugin && $(MAKE) install $(YOSYS_RS_PLUGIN_MK_ARGS)

##
## @ build_verific
##     |---> info       :  Compile Verific
##     |---> usage      :  make build_verific
build_verific: 
	cd verific/verific-vJan22/tclmain && $(MAKE) $(VERIFIC_MK_ARGS)

##
## @ co_yosys
##     |---> info       :  Checkout yosys, yosys-rs-plugin and yosys-plugins submodules
##     |---> usage      :  make co_yosys
co_yosys:
	git submodule update --init --remote --recursive yosys
	cd yosys && git fetch && git checkout master && git pull
	git submodule update --init --remote --recursive yosys-plugins
	cd yosys-plugins && git fetch && git checkout master && git pull
	git submodule update --init --remote --recursive yosys-rs-plugin
	cd yosys-rs-plugin && git fetch && git checkout main && git pull
	git submodule update --init --remote --recursive logic_synthesis-rs
	cd logic_synthesis-rs/LSOracle-rs && git fetch && git checkout master && git pull
	cd logic_synthesis-rs/abc-rs && git fetch && git checkout save_PIs_and_POs && git pull

##
## @ co_verific
##     |---> info       :  Checkout verific submodule
##     |---> usage      :  make co_verific
co_verific:
	git submodule update --init --remote --recursive verific
	cd verific && git fetch && git checkout vJan22-yosys && git pull

##
## @ co_rtl_benchmark
##     |---> info       :  Checkout RTL_benchmark submodule
##     |---> usage      :  make co_rtl_benchmark
co_rtl_benchmark:
	git submodule update --init --remote --recursive RTL_Benchmark
	cd RTL_Benchmark && git fetch && git checkout master && git pull

##
## @ co_benchmarks
##     |---> info       :  Checkout all benchmark submodules
##     |---> usage      :  make co_benchmarks
co_benchmarks: co_vhdl co_system_verilog co_mixed_languages

##
## @ co_vhdl
##     |---> info       :  Checkout all VHDL benchmark submodules
##     |---> usage      :  make co_vhdl
co_vhdl:
	git submodule update --init --remote --recursive benchmarks/vhdl

##
## @ co_system_verilog
##     |---> info       :  Checkout all SV benchmark submodules
##     |---> usage      :  make co_system_verilog
co_system_verilog:
	git submodule update --init --remote --recursive benchmarks/system_verilog

##
## @ co_mixed_languages
##     |---> info       :  Checkout all mixed_languages benchmark submodules
##     |---> usage      :  make co_mixed_languages
co_mixed_languages:
	git submodule update --init --remote --recursive benchmarks/mixed_languages

##
## @ co_benchmark_name
##     |---> info       :  Checkout specified benchmark submodule
##     |---> usage      :  make co_benchmark_name BENCHMARK_NAME=VALUE
co_benchmark_name:
	git submodule update --init --remote --recursive $(shell find ./benchmarks -name $(BENCHMARK_NAME))

##
## @ clean
##     |---> info       :  Clean all generated files and remove all benchmark submodules
##     |---> usage      :  make clean
clean: clean_benchmarks clean_yosys clean_verific

##
## @ clean_benchmarks
##     |---> info       :  Remove all benchmark submodules
##     |---> usage      :  make clean_benchmarks
clean_benchmarks: clean_verilog clean_vhdl clean_mixed_languages clean_system_verilog

##
## @ clean_vhdl
##     |---> info       :  Remove all VHDL benchmark submodules 
##     |---> usage      :  make clean_vhdl
clean_vhdl:
	grep 'path = benchmarks/vhdl' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	grep 'path = benchmarks/vhdl' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash

##
## @ clean_verilog
##     |---> info       :  Remove all Verilog benchmark submodules 
##     |---> usage      :  make clean_verilog
clean_verilog:
	grep 'path = benchmarks/verilog' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	grep 'path = benchmarks/verilog' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash

##
## @ clean_system_verilog
##     |---> info       :  Remove all SV benchmark submodules
##     |---> usage      :  make clean_system_verilog
clean_system_verilog:
	grep 'path = benchmarks/system_verilog' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	grep 'path = benchmarks/system_verilog' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash

##
## @ clean_mixed_languages
##     |---> info       :  Clean all mixed_languages benchmark submodules
##     |---> usage      :  make clean_mixed_languages
clean_mixed_languages:
	grep 'path = benchmarks/mixed_languages' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	grep 'path = benchmarks/mixed_languages' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash

##
## @ clean_yosys
##     |---> info       :  Clean yosys, abc-rs, yosys-rs-plugin and yosys-plugins submodules generated files
##     |---> usage      :  make clean_yosys
clean_yosys:
ifneq ("","$(wildcard $(YOSYS_PATH))")
	rm -rf $(YOSYS_PATH)
endif
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
ifneq ("","$(wildcard ./logic_synthesis-rs/LSOracle-rs/Makefile)")
	cd logic_synthesis-rs/LSOracle-rs && $(MAKE) clean
endif

##
## @ clean_verific
##     |---> info       :  Clean verific_rs submodule generated files
##     |---> usage      :  make clean_verific_rs
clean_verific:
ifneq ("","$(wildcard ./verific/verific-v*/tclmain/Makefile)")
	cd verific/verific-v*/tclmain && $(MAKE) clean
	cd verific && git restore .
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
