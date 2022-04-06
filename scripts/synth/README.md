# Introduction
This directory contains scripts which will run sysnthesis for the list of benchmarks provided in JSON input file alongside the corresponding configuration for synthesis. For each benchmark in JSON it will copy design files from 'rtl_path' to the corresponding directory created for that design run. The script will run synthesis with the tool provided in the JSON as 'tool'. The benchmarks in the list will be run in parallel with up to 'num_process' number of processes. 

# Directory Structure
```
.
|-- abc
|-- vivado
|-- diamond
`-- yosys
    
```

`abc` directory contains ABC scripts.
`vivado` directory contains Vivado template TCL scripts.
`diamond` directory contains Diamond template TCL scripts.
`yosys` directory contains Yosys template synthesis scirpts.

# Command line arguments
- --config_files: The JSON configuration files

# Input JSON configuration format
The input JSON file contains list of benchmarks with synthesis run configurations. 

- tool - The tool to run the synthesis with (vivado/yosys/diamond),
- vivado - Vivado commands dictionary,
    - vivado_template_script - The path to vivado template script,
- yosys - Yosys commands dictionary,
    - yosys_path - yosys installation path,
    - yosys_template_script - The path to yosys template script,
    - abc_script - The path to 'abc' script,
    - verific - Use verific or not (true/false),
    - synth_rs - Supported options for synthesis plugin
        - tech
        - blif
        - verilog
        - goal
        - effort
        - de
        - abc(DEV_BUILD)
        - cec(DEV_BUILD)
        - carry(DEV_BUILD)
        - sdff(DEV_BUILD)
        - no_dsp(DEV_BUILD)
        - no_bram(DEV_BUILD)

- diamond - Diamond commands dictionary,
    - diamond_template_script - The path to diamond template script,
- num_process - Max number of parallel runs,
- timeout - Upper limit for synthesis run duration,
- benchmarks - The list of designs
	- name - Design name,
	- rtl_path - The path to design directory,
	- top_module - The design's top module.

An example input JSON configuration:
```bash
{
    "tool": "yosys",
    "yosys": {

        "yosys_path": "yosys/install/bin/yosys",
        "abc_script": "scripts/synth/abc_scripts/abc_base6.v1.scr",
        "yosys_template_script": "scripts/synth/yosys/yosys_template_synth_rs_optional.ys",
        "verific": true,
        "synth_rs" : {

            "-tech": "genesis",
            "-goal": "area",
            "-no_dsp": true,
        }
    },
    "vivado_template_script": "scripts/synth/vivado_v1_template.tcl",
    "num_process": 4,
    "timeout": 10800,
    "benchmarks": [
        {
            "name": "cavlc",
            "rtl_path": "../Gap-Analysis/RTL_Benchmark/cavlc/rtl",
            "top_module": "cavlc"
        },
        {
            "name": "des_ao",
            "yosys": {
                "synth_rs" : {
                    "-goal": "delay"
                },
            },
            "rtl_path": "../Gap-Analysis/RTL_Benchmark/des_area_opt",
            "top_module": "des_top"
        }
    ]
}
```
# How to run
Before running the automation script with Yosys if `'-de': true` specified the following command should be executed:
```bash
source export_env.sh
```
Before running the automation script for comercial tools on **kyber** server the following command should be executed:
```bash
load_vivado
```
Below is the command example to run the automation script.
```bash
python3 synthesis.py --config_files config_1.json config_2.json
```
