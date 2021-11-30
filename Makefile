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
	@make -C ./yosys config-gcc
	@make -C ./yosys

##
## @ build_yosys_verific
##     |---> info       :  Compile yosys with Verific enabled
##     |---> usage      :  make build_yosys_verific
build_yosys_verific: co_yosys_verific
	@make -C ./yosys CONFIG=gcc ENABLE_VERIFIC_EXECUTIONS=1 DISABLE_VERIFIC_EXTENSIONS=1 VERIFIC_DIR=/opt/verific-Sep21-2021

##
## @ co_yosys_verific
##     |---> info       :  Checkout yosys submodule verific-integration branch
##     |---> usage      :  make co_yosys_verific
co_yosys_verific: clean_yosys
	@git submodule update --init yosys
	@git -C ./yosys checkout verific-integration

##
## @ co_yosys
##     |---> info       :  Checkout yosys submodule
##     |---> usage      :  make co_yosys
co_yosys: clean_yosys
	@git submodule update --init yosys
	@git -C ./yosys checkout master

##
## @ co_benchmarks
##     |---> info       :  Checkout all submodule benchmarks
##     |---> usage      :  make co_benchmarks
co_benchmarks: co_vhdl co_sv co_mixed_languages

##
## @ co_vhdl
##     |---> info       :  Checkout all VHDL submodule benchmarks
##     |---> usage      :  make co_vhdl
co_vhdl: clean_vhdl
	@git submodule update --init --recursive benchmarks/vhdl

##
## @ co_sv
##     |---> info       :  Checkout all SV submodule benchmarks
##     |---> usage      :  make co_sv
co_sv: clean_system_verilog
	@git submodule update --init --recursive benchmarks/system_verilog

##
## @ co_mixed_languages
##     |---> info       :  Checkout all mixed language benchmarks
##     |---> usage      :  make co_mixed_languages
co_mixed_languages: clean_mixed_languages
	@git submodule update --init --recursive benchmarks/mixed_languages

##
## @ co_benchmark_name
##     |---> info       :  Checkout benchmark_name submodule benchmark
##     |---> usage      :  make co_benchmark_name
co_benchmark_name:
	@git submodule update --init --recursive $(shell find ./benchmarks -name $(BENCHMARK_NAME))

##
## @ clean
##     |---> info       :  Clean all generated files 
##     |---> usage      :  make clean
clean: clean_benchmarks clean_yosys

##
## @ clean_benchmarks
##     |---> info       :  Clean all benchmark submodule folders
##     |---> usage      :  make clean_benchmarks
clean_benchmarks: clean_vhdl clean_mixed_languages clean_system_verilog

##
## @ clean_vhdl
##     |---> info       :  Clean all benchmark/vhdl submodule folders
##     |---> usage      :  make clean_vhdl
clean_vhdl:
	@grep 'path = benchmarks/vhdl' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	@grep 'path = benchmarks/vhdl' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
##
## @ clean_system_verilog
##     |---> info       :  Clean all benchmark/system_verilog submodule folders
##     |---> usage      :  make clean_system_verilog
clean_system_verilog:
	@grep 'path = benchmarks/system_verilog' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	@grep 'path = benchmarks/system_verilog' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
##
## @ clean_mixed_languages
##     |---> info       :  Clean all benchmark/mixed_languages submodule folders
##     |---> usage      :  make clean_mixed_languages
clean_mixed_languages:
	@grep 'path = benchmarks/mixed_languages' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	@grep 'path = benchmarks/mixed_languages' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash

##
## @ clean_yosys
##     |---> info       :  Clean all yosys submodule folders
##     |---> usage      :  make clean_yosys
clean_yosys:
	@grep 'path = yosys' .gitmodules | sed 's/.*= //' | sed 's/$$/\/*/' | sed 's/^/.\//'| xargs echo rm -rf | bash
	@grep 'path = yosys' .gitmodules | sed 's/.*= //' | sed 's/$$/\/.??*/' | sed 's/^/.\//'| xargs echo rm -rf | bash

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
