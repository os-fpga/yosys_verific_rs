# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Script Name   : run_metrics_extractor.py
# Description   : This script will generate CSV sheet with different metrics
#		  extracted from Vivado and Yosys output logs.
# Args          : python3 run_metrics_extractor.py --help
# Author        : Bella Baghdasaryan
# Email         : bella@rapidsilicon.com
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

import sys
import os
import re
import csv
import argparse
import time
import logging
import pandas as pd
import glob
from configparser import ConfigParser, ExtendedInterpolation

if sys.version_info[0] < 3:
    raise Exception("run_metrics_extractor script must be run with Python 3")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Configure logging system
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
LOG_FORMAT = "%(levelname)8s - %(message)s"
logging.basicConfig(level=logging.DEBUG, stream=sys.stdout, format=LOG_FORMAT)
logger = logging.getLogger("metrics_extractor_logs")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Read commandline arguments
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
parser = argparse.ArgumentParser(description="The script will generate a CSV \
         sheet with different metrics extracted from Vivado and Yosys output \
         logs. It will read all log files from the input directory, extract \
         all necessary information from each design and write the generated \
         sheet in the provided input directory.")
parser.add_argument("--vivado", type=str, 
        help="path to Vivado run output")
parser.add_argument("--yosys", type=str, 
        help="path to Yosys run output")
parser.add_argument("--output_file", type=str, 
        help="output csv file")
parser.add_argument("--debug", action="store_true",
        help="run script in debug mode.")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Initialize global variables
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
output_vivado_dir = ""
output_yosys_dir = ""
metrics = pd.DataFrame()
metrics = pd.DataFrame(metrics,columns=["Benchmarks", "LUTs Yosys", "LUTs Vivado",
"DFFs Yosys", "DFFs Vivado", "SRLs Yosys", "SRLs Vivado", "DRAMs Yosys", 
"DRAMs Vivado", "BRAMs Yosys", "BRAMs Vivado", "DSPs Yosys", "DSPs Vivado"])
metrics = metrics.fillna(0)
abs_root_dir = os.path.abspath(os.path.join(__file__, "..", "..", ".."))
yosys_log = "yosys_output.log" 

def error_exit(msg):
    """Exit with error message"""
    logger.error("Current working directory : " + os.getcwd())
    logger.error(msg)
    logger.error("Exiting . . . . . .")
    exit(1)

def validate_inputs():
    """
    Check if input files and directories exist and replace variables with 
    absolute path.
    In case of error - print error message and exit the script.
    """
    if not args.output_file:
        error_exit("Please provide output file.")
    if not (args.vivado or args.yosys):
        error_exit("Please provide run output directory")

    global output_yosys_dir
    global output_vivado_dir
    if args.yosys:
        output_yosys_dir = os.path.abspath(args.yosys)
        if not os.path.isdir(output_yosys_dir):
            error_exit("Provided directory not found - %s" %
                output_yosys_dir)
    if args.vivado:
        output_vivado_dir = os.path.abspath(args.vivado)
        if not os.path.isdir(output_vivado_dir):
            error_exit("Provided directory not found - %s" %
                output_vivado_dir)

def metrics_to_csv():
    logger.info("Saving into : " + args.output_file)
    metrics.fillna('-', inplace=True)
    metrics.to_csv(args.output_file, encoding="utf-8-sig")

def get_design_index(design_name):
    return metrics[metrics['Benchmarks'] == design_name].index.item()

def extract_yosys_metrics():
    global metrics
    global output_yosys_dir
    tasks = os.listdir(output_yosys_dir)
    for task_name in tasks:
        if not os.path.isdir(os.path.join(output_yosys_dir, task_name)):
            continue
        if not metrics['Benchmarks'].eq(task_name).any():
            metrics=metrics.append({'Benchmarks':task_name}, ignore_index=True)
        task_log = os.path.join(output_yosys_dir, task_name, yosys_log)
        design_index = get_design_index(task_name)
        metrics.at[design_index, "LUTs Yosys"] = 0
        metrics.at[design_index, "DFFs Yosys"] = 0
        logger.info("Processing Yosys log : " + task_log)
        try:
            with open(task_log, 'r') as f:
                results = re.findall(r"Printing statistics.*\n\n.*\n\n(.*?)\n\n", f.read(), re.DOTALL)
                if not results:
                    logger.error("No information found in : " + task_name + " log file")
                    continue
                results = results[len(results) - 1].splitlines()
                for line in results:
                    if re.search(r"lut", line, re.IGNORECASE):
                        lut_count = line.split()[1]
                        if lut_count:
                            metrics.at[design_index, "LUTs Yosys"] = int(metrics.at[design_index, "LUTs Yosys"]) + int(lut_count)
                    if re.search('dff', line, re.IGNORECASE) or re.search('latch', line, re.IGNORECASE):
                        dff_count = line.split()[1]
                        if dff_count:
                            metrics.at[design_index, "DFFs Yosys"] = int(metrics.at[design_index, "DFFs Yosys"]) + int(dff_count)
        except OSError as e:
            error_exit(e.strerror)
   
def extract_vivado_metrics():
    global metrics
    global output_vivado_dir
    tasks = os.listdir(output_vivado_dir)
    for task_name in tasks:
        task_dir = os.path.join(output_vivado_dir, task_name)
        if not os.path.isdir(task_dir):
            continue
        if not metrics['Benchmarks'].eq(task_name).any():
            metrics=metrics.append({'Benchmarks':task_name}, ignore_index=True)
        design_index = get_design_index(task_name)
        metrics.at[design_index, "LUTs Vivado"] = 0
        metrics.at[design_index, "DFFs Vivado"] = 0
        metrics.at[design_index, "SRLs Vivado"] = 0
        metrics.at[design_index, "DRAMs Vivado"] = 0
        metrics.at[design_index, "BRAMs Vivado"] = 0
        metrics.at[design_index, "DSPs Vivado"] = 0
        vivado_log = "util_temp_\w+_vivado_synth.log" 
        try:
            for filename in os.listdir(os.path.join(task_dir)):
                if re.match(vivado_log, filename):
                    logger.info("Processing Vivado log : " + os.path.join(task_dir, filename))
                    with open(os.path.join(task_dir, filename), 'r') as f:
                        primitives = re.findall(r"Primitives\n?.*Ref Name.*Used.*Functional Category(.*?)\n\n", f.read(), re.DOTALL)
                        if not primitives:
                            logger.error("No Primitives section found in : " + task_name + " log file")
                            continue
                        for record in primitives:
                            if not record:
                                continue
                            lines = re.split("\n", record)
                            for line in lines:
                                if re.search(r"lut", line, re.IGNORECASE):
                                    lut_count = line.split()[3]
                                    if lut_count:
                                        metrics.at[design_index, "LUTs Vivado"] = int(metrics.at[design_index, "LUTs Vivado"]) + int(lut_count)
                                if re.search(r"muxf7", line, re.IGNORECASE):
                                    mux_count = line.split()[3]
                                    if mux_count:
                                        metrics.at[design_index, "LUTs Vivado"] = int(metrics.at[design_index, "LUTs Vivado"]) + int(mux_count)
                                if re.search(r"muxf8", line, re.IGNORECASE):
                                    mux_count = line.split()[3]
                                    if mux_count:
                                        metrics.at[design_index, "LUTs Vivado"] = int(metrics.at[design_index, "LUTs Vivado"]) + int(mux_count)
                                if re.search(r"carry", line, re.IGNORECASE):
                                    carry_count = line.split()[3]
                                    if carry_count:
                                        metrics.at[design_index, "LUTs Vivado"] = int(metrics.at[design_index, "LUTs Vivado"]) + int(carry_count)
                                if re.search(r"Flop & Latch", line, re.IGNORECASE):
                                    dff_count = line.split()[3]
                                    if dff_count:
                                        metrics.at[design_index, "DFFs Vivado"] = int(metrics.at[design_index, "DFFs Vivado"]) + int(dff_count)
                                if re.search(r"srl", line, re.IGNORECASE):
                                    srl_count = line.split()[3]
                                    if srl_count:
                                        metrics.at[design_index, "SRLs Vivado"] = int(metrics.at[design_index, "SRLs Vivado"]) + int(srl_count)
                                if re.search(r"Distributed Memory", line, re.IGNORECASE):
                                    dram_count = line.split()[3]
                                    if dram_count:
                                        metrics.at[design_index, "DRAMs Vivado"] = int(metrics.at[design_index, "DRAMs Vivado"]) + int(dram_count)
                                if re.search(r"Block Memory", line, re.IGNORECASE):
                                    bram_count = line.split()[3]
                                    if bram_count:
                                        metrics.at[design_index, "BRAMs Vivado"] = int(metrics.at[design_index, "BRAMs Vivado"]) + int(bram_count)
                                if re.search(r"dsp", line, re.IGNORECASE):
                                    dsp_count = line.split()[3]
                                    if dsp_count:
                                        metrics.at[design_index, "DSPs Vivado"] = int(metrics.at[design_index, "DSPs Vivado"]) + int(dsp_count)
                    with open(os.path.join(task_dir, filename), 'r') as f:
                        slice_logic = re.findall(r"Slice Logic.*Site Type.*Used.*Fixed.*Available.*Util\%|\n?(.*?)\n\n?", f.read(), re.DOTALL)
                        if not slice_logic:
                            logger.error("No Slice Logic section found in : " + task_name + " log file")
                            continue
                        for record in slice_logic:
                            if not record:
                                continue
                            lines = re.split("\n", record)
                            for line in lines:
                                if re.search("LUT as Memory", line, re.IGNORECASE):
                                    lut_count = line.split()[5]
                                    if lut_count:
                                        metrics.at[design_index, "LUTs Vivado"] = int(metrics.at[design_index, "LUTs Vivado"]) - int(lut_count)
        except OSError as e:
            logger.error(e.strerror)

def main():
    """Main function."""
    logger.info("Starting metrics extraction . . . . .")
    global metrics
    validate_inputs()

    extract_yosys_metrics()
    extract_vivado_metrics()
    metrics_to_csv()

if __name__ == "__main__":
    startTime = time.time()
    args = parser.parse_args()
    main()
    endTime = time.time()
    logger.info("Merics extraction completed in %s seconds." % str(endTime - startTime))
