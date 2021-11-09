# Introduction
The script will generate OpenFPGA tasks based on existing tasks and the settings provided in the input JSON file. It will read the input JSON file, copy each 'original_task_dir' to 'new_task_dir' in the provided OpenFPGA directory and set all configuration settings specified in the 'config_sections' for each new task correspondingly.

# Command line arguments
- openfpga_path: path to OpenFPGA root directory
- --settings_file: the JSON settings file for the tasks generation. 'default_settings.json' will be used if option is omitted.
- --debug: run script in debug mode.

# Input JSON settings format
The input JSON file contains list of TASK_SETTINGs. 
Each TASK_SETTING has the following properties:
- original_task_dir - existing task directory in the provided openfpga directory tree,
- new_task_dir - new task directory in the provided openfpga directory tree,
- config_sections - contains individual properties for a task configuration section.
An example input JSON settings for one TASK_SETTINGs is as the following:
```bash
[
    {
        "original_task_dir": "basic_tests/k4_series/k4n4_fracff",
        "new_task_dir": "basic_tests/k4_series/k4n4_fracff_verific",
        "config_sections": {
            "GENERAL": {
                "verific": "true"
            },
            "OpenFPGA_SHELL": {
                "yosys_blackbox_modules": "latchre,dffrn,dffre,dff,dffr"
            },
            "SYNTHESIS_PARAM": {
                "bench_yosys_common": "${PATH:OPENFPGA_PATH}/openfpga_flow/misc/ys_tmpl_yosys+verific_vpr_dff_flow.ys",
                "bench_yosys_rewrite_common": "${PATH:OPENFPGA_PATH}/openfpga_flow/misc/ys_tmpl_yosys+verific_vpr_flow_with_rewrite.ys;${PATH:OPENFPGA_PATH}/openfpga_flow/misc/ys_tmpl_rewrite_flow.ys"
            }
        }
    }
]
```

# How to run
To run with default settings.
```bash
python3 run_task_generator.py PATH_TO_OPENFPGA_ROOT --debug
```
To run with specific settings.
```bash
python3 run_task_generator.py PATH_TO_OPENFPGA_ROOT --settings_file SPECIFIC_SETTINGS.json --debug
```
Note. The script can be run from any directory:
