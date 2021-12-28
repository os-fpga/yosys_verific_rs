# Introduction
The script will run sysnthesis for the list of benchmarks provided in JSON input file alongside the corresponding configuration for synthesis. For each benchmark in JSON it will copy design files from 'rtl_path' corresponding directory created for that design run. The script will run the synthesis with the tool provided in the JSON as 'tool'. The benchmarks inthe list will be run parallel with up to 'num_process'number of processes. 

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
- verific - Use verific or not (thue/false),
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
    "vivado_template_script": "scripts/synth/vivado_official_v1_template.tcl",
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
```bash
python3 synthesis.py --config_files config_1.json config_2.json
```
