## Set variables
CURRENT_SOURCE_DIR = $(shell pwd)

##
## @ all 
##     |---> info       :  Checkout all submodules, buils yosys with verific enabled
##     |---> usage      :  make all
all: co_benchmarks co_and_build_yosys_verific

##
## @ co_and_build_yosys_verific
##     |---> info       :  Checkout and compile yosys with Verific enabled and yosys-plugins
##     |---> usage      :  make build_yosys_verific
co_and_build_yosys_verific: co_yosys co_verific build_yosys_verific

##
## @ build_yosys_verific
##     |---> info       :  Compile yosys with Verific enabled and yosys-plugins
##     |---> usage      :  make build_yosys_verific
build_yosys_verific: build_verific
	$(eval YOSYS_MK_ARGS  := CONFIG=gcc PREFIX=$(CURRENT_SOURCE_DIR)/yosys/install ENABLE_VERIFIC=1 DISABLE_VERIFIC_EXTENSIONS=1 VERIFIC_DIR=$(CURRENT_SOURCE_DIR)/verific/verific-vDec21 -j 4)
	$(eval YOSYS_PLUGINS_MK_ARGS := YOSYS_PATH=$(CURRENT_SOURCE_DIR)/yosys/install EXTRA_FLAGS="-DPASS_NAME=synth_ql")
	cd yosys && $(MAKE) install $(YOSYS_MK_ARGS) 
	cd yosys-plugins && $(MAKE) install_ql-qlf $(YOSYS_PLUGINS_MK_ARGS)

##
## @ build_yosys
##     |---> info       :  Compile yosys and yosys-plugins
##     |---> usage      :  make build_yosys
build_yosys:
	$(eval YOSYS_MK_ARGS := CONFIG=gcc PREFIX=$(CURRENT_SOURCE_DIR)/yosys/install -j 4)
	$(eval YOSYS_PLUGINS_MK_ARGS := YOSYS_PATH=$(CURRENT_SOURCE_DIR)/yosys/install EXTRA_FLAGS="-DPASS_NAME=synth_ql")
	cd yosys && $(MAKE) install $(YOSYS_MK_ARGS)
	cd yosys-plugins && $(MAKE) install_ql-qlf $(YOSYS_PLUGINS_MK_ARGS)

##
## @ build_verific
##     |---> info       :  Compile Verific
##     |---> usage      :  make build_verific
build_verific: 
	cd verific/verific-vDec21/tclmain && $(MAKE) VERSION="-O3" TOPFLAGS="-I/usr/include/tcl -fPIC -std=c++11"

##
## @ co_yosys
##     |---> info       :  Checkout yosys and yosys-plugins submodules
##     |---> usage      :  make co_yosys
co_yosys:
	git submodule update --init yosys
	cd yosys && git checkout master && git pull
	git submodule update --init yosys-plugins

##
## @ co_verific
##     |---> info       :  Checkout verific submodule
##     |---> usage      :  make co_verific
co_verific:
	git submodule update --init verific
	cd verific && git checkout vDec21-yosys && git pull

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
	git submodule update --init --recursive benchmarks/vhdl

##
## @ co_system_verilog
##     |---> info       :  Checkout all SV benchmark submodules
##     |---> usage      :  make co_system_verilog
co_system_verilog:
	git submodule update --init --recursive benchmarks/system_verilog

##
## @ co_mixed_languages
##     |---> info       :  Checkout all mixed_languages benchmark submodules
##     |---> usage      :  make co_mixed_languages
co_mixed_languages:
	git submodule update --init --recursive benchmarks/mixed_languages

##
## @ co_benchmark_name
##     |---> info       :  Checkout specified benchmark submodule
##     |---> usage      :  make co_benchmark_name BENCHMARK_NAME=VALUE
co_benchmark_name:
	git submodule update --init --recursive $(shell find ./benchmarks -name $(BENCHMARK_NAME))

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
##     |---> info       :  Clean yosys and yosys-plugins submodules generated files
##     |---> usage      :  make clean_yosys
clean_yosys:
	cd yosys && $(MAKE) clean
	cd yosys-plugins && $(MAKE) clean

##
## @ clean_verific
##     |---> info       :  Clean verific_rs submodule generated files
##     |---> usage      :  make clean_verific_rs
clean_verific:
	cd verific/verific-vDec21/tclmain && $(MAKE) clean
	cd verific && git restore .

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
