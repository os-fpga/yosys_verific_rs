# Introduction
This directory contains scripts which will extract and analyze metrics from the provided run directories.

# Directory Structure
```
.
|-- run_metrics_extractor.py
`-- result_comparision.py
    
```

`run_metrics_extractor.py` script extracts metrics from the provided run directories into CSV file.
`result_comparision.py` script performs analysis of the input CSV metrics file.

# Dependencies
```bash
python3 -m pip install --upgrade pip --user
python3 -m pip install pandas termcolor --user
```

# Description
The `run_metrics_extractor.py` script will generate a CSV sheet with different metrics extracted from
Vivado and Yosys output logs. It will read all log files from the given list
of vivado and yosys runs, extract all necessary information from each design
and write the generated sheet into file given as input argument. Optionally it 
can compare Vivado and Yosys runs the metrics.

The `result_comparision.py` does basic analyzing of QoR results which is provided in the input CSV file.

# Command line arguments
The `run_metrics_extractor.py` script has the following command line arguments:
- `--help` - show help message and exit
- `--vivado` - list of paths to Vivado run outputs
- `--yosys` - list of paths to Yosys run outputs
- `--output_file` - output csv file
- `--run_log` - list of `synthesis.py` log files
- `--base` - [optional] path to Vivado or Yosys run output which will be used as a base in comparision
- `--debug` - run script in debug mode
- `--exclude_metrics` - exclude specified metrics. The default list is:
  - `LUT_AS_LOGIC`,
  - `LUT_AS_MEMORY`,
  - `MUXF7`,
  - `MUXF8`,
  - `MAX_LOGIC_LEVEL`,
  - `AVERAGE_LOGIC_LEVEL`,
  - `SRL`,
  - `DRAM`,
  - `BRAM`,
  - `DSP`
- `--viv_carry_as_lut` - include CARRY4 cells in Vivado LUT calculation

The `result_comparision.py` script has the following command line arguments:
- `file` - CSV file to analyze

# How to run
```bash
python3 scripts/log_automation/run_metrics_extractor.py --yosys result_DATETIME/All_lut_synth_rs_ade.json --vivado result_DATETIME/All_lut_vivado.json --output_file yosys_VS_vivado.csv --run_log result_DATETIME/run.log --base result_DATETIME/All_lut_vivado.json
```
```bash
python3 scripts/log_automation/result_comparision.py yosys_VS_vivado.csv
```
