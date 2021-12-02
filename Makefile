##
## @ all 
##     |---> info       :  Checkout all submodules, buils yosys with verific enabled
##     |---> usage      :  make all
all: co_benchmarks build_yosys_verific

##
## @ build_yosys
##     |---> info       :  Compile yosys
##     |---> usage      :  make build_yosys
build_yosys: co_yosys 
	$(eval YOSYS_MK_ARGS := -j 4)
	@cd yosys && $(MAKE) config-gcc
	@cd yosys && $(MAKE) $(YOSYS_MK_ARGS)

##
## @ build_yosys_verific
##     |---> info       :  Compile yosys with Verific enabled
##     |---> usage      :  make build_yosys_verific
build_yosys_verific: co_yosys_verific
	$(eval YOSYS_MK_ARGS  := ENABLE_VERIFIC=1 DISABLE_VERIFIC_EXTENSIONS=1 VERIFIC_DIR=/opt/verific-Sep21-2021 -j 4)
	@cd yosys && $(MAKE) config-gcc
	@cd yosys && $(MAKE) $(YOSYS_MK_ARGS) 

##
## @ co_yosys_verific
##     |---> info       :  Checkout yosys submodule verific-integration branch
##     |---> usage      :  make co_yosys_verific
co_yosys_verific:
	@git submodule update --init yosys
	@cd yosys && git checkout verific-integration

##
## @ co_yosys
##     |---> info       :  Checkout yosys submodule
##     |---> usage      :  make co_yosys
co_yosys:
	@git submodule update --init yosys
	@cd yosys && git checkout master

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
	@git submodule update --init --recursive benchmarks/vhdl

##
## @ co_system_verilog
##     |---> info       :  Checkout all SV benchmark submodules
##     |---> usage      :  make co_system_verilog
co_system_verilog:
	@git submodule update --init --recursive benchmarks/system_verilog

##
## @ co_mixed_languages
##     |---> info       :  Checkout all mixed language benchmarks
##     |---> usage      :  make co_mixed_languages
co_mixed_languages:
	@git submodule update --init --recursive benchmarks/mixed_languages

##
## @ co_benchmark_name
##     |---> info       :  Checkout specified benchmark submodule
##     |---> usage      :  make co_benchmark_name BENCHMARK_NAME=VALUE
co_benchmark_name:
	@git submodule update --init --recursive $(shell find ./benchmarks -name $(BENCHMARK_NAME))

##
## @ clean
##     |---> info       :  Clean all generated files and remove all benchmark submodules
##     |---> usage      :  make clean
clean: clean_benchmarks clean_yosys

##
## @ clean_benchmarks
##     |---> info       :  Remove all benchmark submodules
##     |---> usage      :  make clean_benchmarks
clean_benchmarks: clean_vhdl clean_mixed_languages clean_system_verilog

##
## @ clean_vhdl
##     |---> info       :  Remove all benchmark/vhdl benchmark submodules 
##     |---> usage      :  make clean_vhdl
clean_vhdl:
	@grep 'path = benchmarks/vhdl' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	@grep 'path = benchmarks/vhdl' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
##
## @ clean_system_verilog
##     |---> info       :  Remove all benchmark/system_verilog benchmark submodules
##     |---> usage      :  make clean_system_verilog
clean_system_verilog:
	@grep 'path = benchmarks/system_verilog' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	@grep 'path = benchmarks/system_verilog' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
##
## @ clean_mixed_languages
##     |---> info       :  Clean all benchmark/mixed_languages benchmark submodules
##     |---> usage      :  make clean_mixed_languages
clean_mixed_languages:
	@grep 'path = benchmarks/mixed_languages' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	@grep 'path = benchmarks/mixed_languages' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash

##
## @ clean_yosys
##     |---> info       :  Clean yosys submodule generated files
##     |---> usage      :  make clean_yosys
clean_yosys:
	@cd yosys && $(MAKE) clean

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
