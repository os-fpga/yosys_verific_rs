# Introduction
This directory contains scripts which will run sysnthesis for the list of benchmarks provided in JSON input file alongside the corresponding configuration for synthesis. For each benchmark in JSON it will copy design files from 'rtl_path' to the corresponding directory created for that design run. The script will run synthesis with the tool provided in the JSON as 'tool'. The benchmarks in the list will be run in parallel with up to 'num_process' number of processes. 

# Directory Structure
```
.
|-- abc
|-- vivado
`-- yosys
    
```

`abc` directory contains ABC scripts.
`vivado` directory contains Vivado template TCL scripts.
`yosys` directory contains Yosys template synthesis scirpts.

# Command line arguments
- --config_files: The JSON configuration files

# Input JSON configuration format
The input JSON file contains list of benchmarks with synthesis run configurations. 

- tool - The tool to run the synthesis with (vivado/yosys),
- yosys_path - yosys installation path,
- abc_script - The path to 'abc' script,
- yosys_template_script - The path to yosys template script,
- vivado_template_script - The path to vivado template script,
- num_process - Max number of parallel runs,
- timeout - Upper limit for synthesis run duration,
- verific - Use verific or not (thrue/false),
- benchmarks - The list of designs
	- name - Design name,
	- rtl_path - The path to design directory,
	- top_module - The design's top module.

An example input JSON configuration:
```bash
{
    "tool": "vivado",
    "yosys_path": "yosys/install/bin/yosys",
    "abc_script": "scripts/synth/abc_scripts/abc_base6.v1.scr",
    "yosys_template_script": "scripts/synth/yosys_template.ys",
    "vivado_template_script": "scripts/synth/vivado_v1_template.tcl",
    "num_process": 4,
    "timeout": 10800,
    "verific": true,
    "benchmarks": [
        {
            "name": "cavlc",
            "rtl_path": "../Gap-Analysis/RTL_Benchmark/cavlc/rtl",
            "top_module": "cavlc"
        },
        {
            "name": "des_ao",
            "rtl_path": "../Gap-Analysis/RTL_Benchmark/des_area_opt",
            "top_module": "des_top"
        }
    ]
}
```

# How to run
Before running the automation script for comercial tools on **kyber** server the following command should be executed:
```bash
load_vivado
```
Below is the command example to run the automation script.
```bash
python3 synthesis.py --config_files config_1.json config_2.json
```
