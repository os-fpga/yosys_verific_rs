# Introduction
This repository is designed for the Yosys+Verific support. The open-source Yosys has extensive Verilog-2005 support while Verific adds complete support for SystemVerilog IEEE-1800, UPF IEEE-1801 and VHDL IEEE-1076 standards. 
The repository contains yosys_rs and open-source HDL projects as submodules, which are going to be used for the Sythesis and Verification. It also contains Yosys template scripts which can be used in the OpenFPGA tasks for the yosys_vpr flow. These scripts are designed to be used only with Yosys with Verific enabled.

# Repository Structure
```
.
|-- benchmarks
|   |-- mixed_languages
|   |-- system_verilog
|   `-- vhdl
|-- scripts
|   |-- task_generator
|   `-- yosys_templates
`-- yosys
    
```

The repository has the following submodules:
 - [yosys](https://github.com/RapidSilicon/yosys_rs) 
 - [benchmarks/system_verilog/black-parrot](https://github.com/black-parrot/black-parrot)
 - [benchmarks/system_verilog/ariane](https://github.com/lowRISC/ariane)
 - [benchmarks/system_verilog/scm](https://github.com/pulp-platform/scm.git)
 - [benchmarks/system_verilog/udma_core](https://github.com/pulp-platform/udma_core.git)
 - [benchmarks/system_verilog/cva6](https://github.com/pulp-platform/cva6.git)
 - [benchmarks/vhdl/FPGA-FAST](https://github.com/PUTvision/FPGA-FAST.git)
 - [benchmarks/vhdl/PoC-Examples](https://github.com/VLSI-EDA/PoC-Examples.git)
 - [benchmarks/vhdl/Trivium_FPGA](https://github.com/yahniukov/Trivium_FPGA.git)
 - [benchmarks/vhdl/vhdl-hdmi-out](https://github.com/fcayci/vhdl-hdmi-out.git)

`benchmarks` directory contains benchmark open-source designs written in VHDL, SystemVerilog and mixed languages:
 - `mixed_languages` holds mixed language desings.
 - `system_verilog` holds SystemVerilog submodule designs.
 - `vhdl` holds VHDL submodule designs.

`scripts` directory contains the OpenFPGA task generator and OpenFPGA Yosys template scripts: 
 - `task_generator` holds the OpenFPGA tasks generator script and it's default settings JSON file. 
 - `yosys_templates` holds the OpenFPGA Yosys template scripts which are written to use the `verific` frontend.

`yosys` directory contains Yosys submodule.

## Build
After cloning the repo, run 'all' Makefile target to initialize/update all submodules and build Yosys with Verific enabled:
```bash
make all
```
All available Makefile targets can be seen running 'help' target:
```bash
make help
```

## How to generate yosys+verific OpenFPGA tasks
To generate tasks with default configurations/settings the following command should be run:
```bash
python3 scripts/task_generator/run_task_generator.py PATH_TO_OPENFPGA_ROOT --debug
```
To generate tasks with specific configurations/settings the following command should be run:
```bash
python3 scripts/task_generator/run_task_generator.py PATH_TO_OPENFPGA_ROOT --settings_file SPECIFIC_SETTINGS.json --debug
```
Detailed information regarding OpenFPGA tasks generation can be found [here](https://github.com/RapidSilicon/yosys_verific_rs/blob/main/scripts/task_generator/README.md).
