# Introduction
This repository is designed for the Yosys+Verific support. The open-source Yosys has extensive Verilog-2005 support while Verific adds complete support for SystemVerilog IEEE-1800, UPF IEEE-1801 and VHDL IEEE-1076 standards. 
The repository contains yosys_rs, verific_rs and open-source HDL projects as submodules, which are going to be used for the Sythesis and Verification. It also contains Yosys template scripts which can be used in the OpenFPGA tasks for the yosys_vpr flow. These scripts are designed to be used only with Yosys with Verific enabled.

# Requirements
The repository requires SSH key setup. Please see instructions at [connecting-to-github-with-ssh](https://docs.github.com/en/authentication/connecting-to-github-with-ssh)

The list of dependencies:
 * [`Ubuntu`](.github/scripts/install_ubuntu_dependencies_build.sh)
 * [`Centos`](.github/scripts/install_centos_dependencies_build.sh)
 * [`MacOS`](.github/scripts/install_macos_dependencies_build.sh)

# Repository Structure
```
.
|-- RTL_Benchmark
|-- benchmarks
|-- logic_synthesis-rs
|   |-- LSOracle-rs
|   `-- abc-rs
|-- scripts
|-- suites
|-- verific
|-- yosys
|-- yosys-plugins
`-- yosys-rs-plugin
    
```

The repository has the following submodules:
 - [yosys](https://github.com/RapidSilicon/yosys_rs.git) 
 - [yosys-plugins](https://github.com/SymbiFlow/yosys-f4pga-plugins.git) 
 - [yosys-rs-plugin](https://github.com/RapidSilicon/yosys-rs-plugin.git) 
 - [verific](https://github.com/RapidSilicon/verific_rs.git) 
 - [logic_synthesis-rs](https://github.com/RapidSilicon/logic_synthesis-rs.git) 
 - [RTL_Benchmark](https://github.com/RapidSilicon/RTL_Benchmark.git)

`benchmarks` directory contains benchmark open-source designs - SHOULD BE REMOVED:
 - `verilog` holds Verilog language desings.
 - `mixed_languages` holds mixed language desings.
 - `vhdl` holds VHDL submodule designs.
`suites` directory contains benchmark suites which can be automatically run by the automation scripts available at `scripts/synth`.
`scripts` directory contains automation scripts: 
 - `benchmarks` holds Yosys synthesis scripts for the available benchmarks.
 - `log_automation` holds the automation scripts to extract metrics from tools output log files.
 - `synth` holds the automation scripts to run synthesis on different tools.
 - `task_generator` holds the OpenFPGA tasks generator script and it's default settings JSON file. 
 - `yosys_templates` holds the OpenFPGA Yosys template scripts which are written to use the `verific` frontend.
`verific` directory contains Verific submodule.
`yosys` directory contains Yosys submodule.
`yosys-plugins` directory contains yosys-symbiflow-plugins submodule.
`yosys-rs-plugin` directory contains yosys-rs-plugin submodule.
`logic_synthesis-rs` directory contains logic_synthesis-rs submodule.
`RTL_Benchmark` directory contains RTL_Benchmark submodule.

## Build
Run **release** Makefile target to build Yosys with Verific enabled:
```bash
make release
```
Run **install** Makefile target to build and install Yosys with Verific enabled:
```bash
make install PREFIX=<INSTALL_DIR>
```
All available Makefile targets can be seen running **help** target:
```bash
make help
```

## Running tests
Initialize/update RTL_Benchmark submodule:
```bash
git submodule update --init --recursive --progress RTL_Benchmark
```
Execute python script to run suite of benchmarks:
```bash
python3 scripts/synth/synthesis.py --config_files suites/Golden/Golden_synth_rs_ade_with_bram_with_dsp.json
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
