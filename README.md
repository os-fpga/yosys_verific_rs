# Introduction
This repository is designed for the Yosys+Verific support. The open-source Yosys has extensive Verilog-2005 support while Verific adds complete support for SystemVerilog IEEE-1800, UPF IEEE-1801 and VHDL IEEE-1076 standards. 
The repository contains yosys_rs, verific_rs and open-source HDL projects as submodules, which are going to be used for the Sythesis and Verification. It also contains Yosys template scripts which can be used in the OpenFPGA tasks for the yosys_vpr flow. These scripts are designed to be used only with Yosys with Verific enabled.

# Requirements
The repository requires SSH key setup. Please see instructions at [connecting-to-github-with-ssh](https://docs.github.com/en/authentication/connecting-to-github-with-ssh)

# Repository Structure
```
.
|-- benchmarks
|   |-- mixed_languages
|   |-- system_verilog
|   `-- vhdl
|-- suites
|-- scripts
|   |-- benchmarks
|   |-- log_automation
|   |-- synth
|   |-- task_generator
|   `-- yosys_templates
|-- verific
|-- yosys
`-- yosys-plugins
    
```

The repository has the following submodules:
 - [yosys](https://github.com/RapidSilicon/yosys_rs.git) 
 - [yosys-plugins](https://github.com/SymbiFlow/yosys-symbiflow-plugins.git) 
 - [verific](https://github.com/RapidSilicon/verific_rs.git) 
 - [benchmarks/system_verilog/black-parrot](https://github.com/black-parrot/black-parrot.git)
 - [benchmarks/system_verilog/ariane](https://github.com/lowRISC/ariane.git)
 - [benchmarks/system_verilog/scm_design/scm](https://github.com/pulp-platform/scm.git)
 - [benchmarks/system_verilog/udma_core_design/udma_core](https://github.com/pulp-platform/udma_core.git)
 - [benchmarks/system_verilog/udma_core_design/common_cells](https://github.com/pulp-platform/common_cells.git)
 - [benchmarks/system_verilog/udma_core_design/tech_cells_generic](https://github.com/pulp-platform/tech_cells_generic.git)
 - [benchmarks/system_verilog/cva6](https://github.com/pulp-platform/cva6.git)
 - [benchmarks/vhdl/FPGA-FAST](https://github.com/PUTvision/FPGA-FAST.git)
 - [benchmarks/vhdl/PoC-Examples](https://github.com/VLSI-EDA/PoC-Examples.git)
 - [benchmarks/vhdl/Trivium_FPGA](https://github.com/yahniukov/Trivium_FPGA.git)
 - [benchmarks/vhdl/vhdl-hdmi-out](https://github.com/fcayci/vhdl-hdmi-out.git)
 - [benchmarks/vhdl/itc99-poli](https://github.com/squillero/itc99-poli.git)

`benchmarks` directory contains benchmark open-source designs written in VHDL, SystemVerilog and mixed languages:
 - `mixed_languages` holds mixed language desings.
 - `system_verilog` holds SystemVerilog submodule designs.
 - `vhdl` holds VHDL submodule designs.
`suites` directory contains benchmark suites which can be automatically run by the automation scripts available at `scripts/synth`.
`scripts` directory contains the OpenFPGA task generator and OpenFPGA Yosys template scripts: 
 - `benchmarks` holds Yosys synthesis scripts for the available benchmarks.
 - `log_automation` holds the automation scripts to extract metrics from tools output log files.
 - `synth` holds the automation scripts to run synthesis on different tools.
 - `task_generator` holds the OpenFPGA tasks generator script and it's default settings JSON file. 
 - `yosys_templates` holds the OpenFPGA Yosys template scripts which are written to use the `verific` frontend.
`verific` directory contains Verific submodule.
`yosys` directory contains Yosys submodule.
`yosys-plugins` directory contains yosys-symbiflow-plugins submodule.

## Build
After cloning the repo, run **co_and_build_yosys_verific** Makefile target to initialize/update required submodules and build Yosys with Verific enabled:
```bash
make co_and_build_yosys_verific
```
Run **all** Makefile target to initialize/update all submodules and build Yosys with Verific enabled:
```bash
make all
```
All available Makefile targets can be seen running **help** target:
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
