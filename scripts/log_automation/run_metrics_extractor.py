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
logging.basicConfig(level=logging.INFO, stream=sys.stdout, format=LOG_FORMAT)
logger = logging.getLogger("metrics_extractor_logs")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Read commandline arguments
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
parser = argparse.ArgumentParser(description="The script will generate a CSV \
         sheet with different metrics extracted from Vivado and Yosys output \
         logs. It will read all log files from the given list of vivado and \
         yosys runs, extract all necessary information from each design and \
         write the generated sheet into file given as input argument.")
parser.add_argument("--vivado", type=str, nargs="*",
        help="list of paths to Vivado run outputs")
parser.add_argument("--yosys", type=str, nargs="*", 
        help="list of path to Yosys run outputs")
parser.add_argument("--output_file", type=str, 
        help="output csv file")
parser.add_argument("--debug", action="store_true",
        help="run script in debug mode.")
parser.add_argument("--run_log", type=str, nargs="*",
        help="log file of tool's run")
parser.add_argument("--base", type=str,
        help="base for the calculations")
parser.add_argument("--exclude_metrics", type=str, nargs="*",
        default=[
            "LUT_AS_LOGIC", 
            "LUT_AS_MEMORY", 
            "MUXF7", 
            "MUXF8", 
            "MAX_LOGIC_LEVEL", 
            "AVERAGE_LOGIC_LEVEL", 
            "SRL", 
            "DRAM", 
            "BRAM"
        ],
        help="exclude specified metrics")
parser.add_argument("--viv_carry_as_lut", action="store_false",
        help="include CARRY4 cells in Vivado LUT calculation.")


# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Initialize global variables
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
output_vivado_dirs = []
output_yosys_dirs = []
run_log_files = []
metrics = pd.DataFrame()
metrics = pd.DataFrame(metrics,columns=["Benchmarks"])
metrics = metrics.fillna(0)
abs_root_dir = os.path.abspath(os.path.join(__file__, "..", "..", ".."))
yosys_log = "yosys_output.log" 
vivado_log = "vivado_output.log"

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
    if args.debug:
        logger.setLevel(logging.DEBUG)
    if not args.output_file:
        error_exit("Please provide output file.")
    if not (args.vivado or args.yosys):
        error_exit("Please provide run output directory.")
    if not args.run_log:
        error_exit("Please provide log file dirctory.")

    global output_yosys_dirs
    global output_vivado_dirs
    global run_log_files
    if args.yosys:
        args.yosys = [arg[:-1] if arg.endswith('/') else arg for arg in args.yosys]
        for yosys_dir in args.yosys:
            if not os.path.isdir(os.path.abspath(yosys_dir)):
                error_exit("Provided directory not found - %s" %
                    yosys_dir)
            output_yosys_dirs.append(os.path.abspath(yosys_dir))

    if args.vivado:
        args.vivado = [arg[:-1] if arg.endswith('/') else arg for arg in args.vivado]
        for vivado_dir in args.vivado:
            if not os.path.isdir(os.path.abspath(vivado_dir)):
                error_exit("Provided directory not found - %s" %
                    vivado_dir)
            output_vivado_dirs.append(os.path.abspath(vivado_dir))

    if args.base:
        args.base = os.path.abspath(args.base[:-1] if args.base.endswith('/') else args.base)
        if not ((args.base in output_yosys_dirs) or (args.base in output_vivado_dirs)):
            error_exit("Incorrect base for the calculations - %s" %
                args.base)

    for run_log in args.run_log:
        if not os.path.isfile(os.path.abspath(run_log)):
            error_exit("Provided file not found - %s" %
                run_log)
        run_log_files.append(os.path.abspath(run_log))

def metrics_to_csv():
    global metrics
    logger.info("Saving into : " + args.output_file)
    metrics.fillna('-', inplace=True)
    metrics.replace(to_replace='nan', value='-',inplace=True)
    metrics.iloc[-4] = ["" for col in metrics.columns]
    metrics.to_csv(args.output_file, encoding="utf-8-sig", index=False)

def get_design_index(design_name):
    return metrics[metrics['Benchmarks'] == design_name].index.item()

def extract_column_name(metric, tool, label):
    return metric + " " + tool + " " + label

def init_columns(metric_list):
    global metrics
    global output_yosys_dirs
    global output_vivado_dirs

    for metric in metric_list[ : 3]:
        if not (metric == metric_list[0]):
            for output_vivado_dir in output_vivado_dirs:
                label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
                metrics[extract_column_name(metric,"Vivado",label)] = '-'
        else:
            for output_yosys_dir in output_yosys_dirs:
                label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
                metrics[extract_column_name(metric,"Yosys",label)] = '-'

    for metric in metric_list:
        if not metric in  metric_list[ : 3]:
            if metric not in metric_list[3 : 7]:
                for output_yosys_dir in output_yosys_dirs:
                    label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
                    metrics[extract_column_name(metric,"Yosys",label)] = '-'
            for output_vivado_dir in output_vivado_dirs:
                label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
                metrics[extract_column_name(metric,"Vivado",label)] = '-'

        if not args.base:
            continue
        if metric in metric_list[ : 3]:
            if args.base and args.base in output_vivado_dirs: 
                if metric == metric_list[0]:
                    continue
                for output_yosys_dir in output_yosys_dirs:
                    label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
                    metrics[extract_column_name("PERCENTAGE " + metric, "Yosys", label)] = '-'
                for output_vivado_dir in output_vivado_dirs:
                    if args.base == output_vivado_dir:
                        continue
                    label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
                    metrics[extract_column_name("PERCENTAGE " + metric, "Vivado", label)] = '-'
            else:
                if metric == metric_list[0]:
                    for output_yosys_dir in output_yosys_dirs:
                        if args.base == output_yosys_dir:
                            continue
                        label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
                        metrics[extract_column_name("PERCENTAGE " + metric, "Yosys", label)] = '-'
                else:
                    for output_vivado_dir in output_vivado_dirs:
                        label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
                        metrics[extract_column_name("PERCENTAGE " + metric, "Vivado", label)] = '-'
        else:        
            if metric == metric_list[-1]:
                continue
            if metric not in metric_list[3 : 7]:
                for output_yosys_dir in output_yosys_dirs:
                    if args.base == output_yosys_dir:
                        continue
                    label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
                    metrics[extract_column_name("PERCENTAGE " + metric, "Yosys", label)] = '-'
            for output_vivado_dir in output_vivado_dirs:
                if args.base == output_vivado_dir:
                    continue
                if metric in metric_list[3 : 7] and args.base not in output_vivado_dirs:
                    continue
                label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
                metrics[extract_column_name("PERCENTAGE " + metric, "Vivado", label)] = '-'

def add_value(count, design_index, metric, tool, label):
    if count:
        metrics.at[design_index, extract_column_name(metric,tool,label)] = \
            int(metrics.at[design_index, extract_column_name(metric,tool,label)]) + int(count)

def extract_yosys_metrics():
    global metrics
    global output_yosys_dirs
    for output_yosys_dir in output_yosys_dirs:
        label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
        tool = "Yosys"
        tasks = os.listdir(output_yosys_dir)
        for task_name in tasks:
            if not os.path.isdir(os.path.join(output_yosys_dir, task_name)):
                continue
            if not metrics['Benchmarks'].eq(task_name).any():
                metrics=metrics.append({'Benchmarks':task_name}, ignore_index=True)
            task_log = os.path.join(output_yosys_dir, task_name, yosys_log)
            design_index = get_design_index(task_name)
            metrics.at[design_index, extract_column_name("LUT",tool,label)] = 0
            metrics.at[design_index, extract_column_name("DFF",tool,label)] = 0
            metrics.at[design_index, extract_column_name("CARRY4",tool,label)] = 0
            metrics.at[design_index, extract_column_name("DSP",tool,label)] = 0
            logger.info("Processing Yosys log : " + task_log)
            try:
                with open(task_log, 'r') as f:
                    log = f.read()
                    log_list = log.split("\n")
                    for line in log_list:
                        if line.startswith("real"):
                            line = line.split()
                            runtime = float(line[1])
                            metrics.at[design_index, extract_column_name("RUNTIME",tool,label)] = runtime
                            continue

                    results = re.findall(r"Printing statistics.*\n\n===.*===\n\n(.*)\n\n\n", log, re.DOTALL)
                    if not results:
                        logger.error("No information found in : " + task_name + " log file")
                        continue
                    results = results[len(results) - 1].splitlines()
                    for line in results:
                        logger.debug(line)
                        if re.search(r"lut", line, re.IGNORECASE):
                            add_value(line.split()[1], design_index, "LUT", tool, label)
                        if re.search('dff', line, re.IGNORECASE) or re.search('latch', line, re.IGNORECASE):
                            add_value(line.split()[1], design_index, "DFF", tool, label)
                        if re.search(r"adder_carry", line, re.IGNORECASE):
                            add_value(line.split()[1], design_index, "CARRY4", tool, label)
                        if re.search('RS_DSP', line, re.IGNORECASE):
                            add_value(line.split()[1], design_index, "DSP", tool, label)
                    results = re.findall("ABC: Mapping \(K=.*\).*(lev =.*\)).*MB", log)
                    if results:
                        results = results[-1].split()
                        metrics.at[design_index, extract_column_name("MAX_LOGIC_LEVEL",tool,label)] = results[2]
                        metrics.at[design_index, extract_column_name("AVERAGE_LOGIC_LEVEL",tool,label)] = results[3][1:-1]
                    else:
                        results = re.findall("ABC:   #Luts =\s+[0-9]+\s+Max Lvl =\s+[0-9]+\s+Avg Lvl =\s+[0-9\.]+", log)
                        if results:
                            results = results[-1].split()
                            metrics.at[design_index, extract_column_name("MAX_LOGIC_LEVEL",tool,label)] = results[7]
                            metrics.at[design_index, extract_column_name("AVERAGE_LOGIC_LEVEL",tool,label)] = results[11]
            except OSError as e:
                error_exit(e.strerror)
            except ValueError as e:
                logger.error("Value error in : " + task_name + " log file")
   
def extract_vivado_metrics():
    global metrics
    global output_vivado_dirs
    for output_vivado_dir in output_vivado_dirs:
        label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
        tool = "Vivado"
        tasks = os.listdir(output_vivado_dir)
        for task_name in tasks:
            task_dir = os.path.join(output_vivado_dir, task_name)
            if not os.path.isdir(task_dir):
                continue
            if not metrics['Benchmarks'].eq(task_name).any():
                metrics=metrics.append({'Benchmarks':task_name}, ignore_index=True)
            design_index = get_design_index(task_name)
            metrics.at[design_index, extract_column_name("LUT:CARRY4=1*LUT",tool,label)] = 0
            metrics.at[design_index, extract_column_name("LUT_AS_LOGIC",tool,label)] = 0
            metrics.at[design_index, extract_column_name("LUT_AS_MEMORY",tool,label)] = 0
            metrics.at[design_index, extract_column_name("MUXF7",tool,label)] = 0
            metrics.at[design_index, extract_column_name("MUXF8",tool,label)] = 0
            metrics.at[design_index, extract_column_name("CARRY4",tool,label)] = 0
            metrics.at[design_index, extract_column_name("DFF",tool,label)] = 0
            metrics.at[design_index, extract_column_name("SRL",tool,label)] = 0
            metrics.at[design_index, extract_column_name("DSP",tool,label)] = 0
            metrics.at[design_index, extract_column_name("DRAM",tool,label)] = 0
            metrics.at[design_index, extract_column_name("BRAM",tool,label)] = 0
            vivado_util_log = "util_temp_\w+_vivado_synth.log" 
            try:
                for filename in os.listdir(os.path.join(task_dir)):
                    if re.match(vivado_util_log, filename):
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
                                        add_value(line.split()[3], design_index, "LUT:CARRY4=1*LUT", tool, label)
                                    if re.search(r"muxf7", line, re.IGNORECASE):
                                        add_value(line.split()[3], design_index, "LUT:CARRY4=1*LUT", tool, label)
                                        add_value(line.split()[3], design_index, "MUXF7", tool, label)
                                    if re.search(r"muxf8", line, re.IGNORECASE):
                                        add_value(line.split()[3], design_index, "LUT:CARRY4=1*LUT", tool, label)
                                        add_value(line.split()[3], design_index, "MUXF8", tool, label)
                                    if re.search(r"carry", line, re.IGNORECASE):
                                        add_value(line.split()[3], design_index, "LUT:CARRY4=1*LUT", tool, label)
                                        add_value(line.split()[3], design_index, "CARRY4", tool, label)
                                    if re.search(r"Flop & Latch", line, re.IGNORECASE):
                                        add_value(line.split()[3], design_index, "DFF", tool, label)
                                    if re.search(r"srl", line, re.IGNORECASE):
                                        add_value(line.split()[3], design_index, "SRL", tool, label)
                                    if re.search(r"Distributed Memory", line, re.IGNORECASE):
                                        add_value(line.split()[3], design_index, "DRAM", tool, label)
                                    if re.search(r"Block Memory", line, re.IGNORECASE):
                                        add_value(line.split()[3], design_index, "BRAM", tool, label)
                                    if re.search(r"dsp", line, re.IGNORECASE):
                                        add_value(line.split()[3], design_index, "DSP", tool, label)
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
                                            metrics.at[design_index, extract_column_name("LUT:CARRY4=1*LUT",tool,label)] = \
                                                int(metrics.at[design_index, extract_column_name("LUT:CARRY4=1*LUT",tool,label)]) - int(lut_count)
                                            metrics.at[design_index, extract_column_name("LUT_AS_MEMORY",tool,label)] = int(lut_count)
                                    if re.search("LUT as Logic", line, re.IGNORECASE):
                                        lut_count = line.split()[5]
                                        if lut_count:
                                            metrics.at[design_index, extract_column_name("LUT_AS_LOGIC",tool,label)] = int(lut_count)
                        task_log = os.path.join(output_vivado_dir, task_name, vivado_log)
                        with open(task_log, 'r') as f:
                            
                            log = f.read()
                            log_list = log.split("\n")
                            for line in log_list:
                                if line.startswith("real"):
                                    line = line.split()
                                    runtime = float(line[1])
                                    metrics.at[design_index, extract_column_name("RUNTIME",tool,label)] = runtime
                                    continue

            except OSError as e:
                logger.error(e.strerror)

def extract_run_log():
    global metrics
    global run_log_files

    if args.vivado:
        global output_vivado_dirs
        for output_vivado_dir in output_vivado_dirs:
            label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
            tool = "Vivado"
            for run_log in run_log_files:
                with open(run_log, "r") as f:
                    for line in f:
                        if "Output directory" in line and output_vivado_dir.split(os.path.sep)[-2] not in line:
                            break
                        value = line.split()
                        if ("Successfully" in value) and (output_vivado_dir.split(os.path.sep)[-1] in value):
                            design_index = get_design_index(value[7])
                            metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Pass"
                        elif ("Failed" in value) and (output_vivado_dir.split(os.path.sep)[-1] in value) :
                            design_index = get_design_index(value[6])
                            metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Fail"
                        elif ("Timeout" in value):
                            design_index = get_design_index(value[11])
                            metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Timeout"

    if args.yosys:
        global output_yosys_dirs
        for output_yosys_dir in output_yosys_dirs:
            label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
            tool = "Yosys"
            for run_log in run_log_files:
                with open(run_log, "r") as f:
                    for line in f:
                        if "Output directory" in line and output_yosys_dir.split(os.path.sep)[-2] not in line:
                            break
                        value = line.split()
                        if ("Successfully" in value) and (output_yosys_dir.split(os.path.sep)[-1] in value):
                            design_index = get_design_index(value[7])
                            metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Pass"
                        elif ("Failed" in value) and (output_yosys_dir.split(os.path.sep)[-1] in value):
                            design_index = get_design_index(value[6])
                            metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Fail"
                        elif ("Timeout" in value):
                            design_index = get_design_index(value[11])
                            metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Timeout"

def calc_vivado_luts():
    if args.vivado:
        global output_vivado_dirs
        global metrics
        for output_vivado_dir in output_vivado_dirs:
            label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
            tool = "Vivado"
            for i in range(0, len(metrics['Benchmarks'])):
                try:
                    metrics.at[i, extract_column_name("LUT:CARRY4=5*LUT",tool,label)] = \
                        int(metrics.at[i, extract_column_name("LUT:CARRY4=1*LUT",tool,label)]) - \
                        int(metrics.at[i, extract_column_name("CARRY4",tool,label)]) + \
                        5 * int(metrics.at[i, extract_column_name("CARRY4",tool,label)])
                except:
                    metrics.at[i, extract_column_name("LUT:CARRY4=1*LUT",tool,label)] = '-'
                    metrics.at[i, extract_column_name("LUT:CARRY4=5*LUT",tool,label)] = '-'


def calc_percentage(metrics_list):
    global output_yosys_dirs
    global output_vivado_dirs
    vivado_lut_metrics = [metrics_list[0]] if args.viv_carry_as_lut else metrics_list[1:3]
    yosys_metrics = [metrics_list[0]] + metrics_list[7:-1]
    vivado_metrics = vivado_lut_metrics + metrics_list[7:-1]
    label_base = args.base.split(os.path.sep)[-2] + "_" + args.base.split(os.path.sep)[-1] 
    lut_label = metrics_list[0]
    if output_yosys_dirs and (args.base in output_yosys_dirs):
        for output_vivado_dir in output_vivado_dirs:
            label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
            for i in range(0, len(metrics['Benchmarks'])):
                for metric in vivado_metrics:
                    try:
                        if metric in vivado_lut_metrics:
                            metrics.at[i, extract_column_name("PERCENTAGE " + metric,"Vivado", label)] = format((float(metrics.at[i, extract_column_name(lut_label, 'Yosys', label_base)]) - \
                                float(metrics.at[i, extract_column_name(metric, "Vivado", label)])) * 100 / float(metrics.at[i, extract_column_name(lut_label, 'Yosys', label_base)]), '.1f')
                        else:
                            metrics.at[i,extract_column_name("PERCENTAGE " + metric, "Vivado", label)] = format((float(metrics.at[i, extract_column_name(metric, 'Yosys', label_base)]) - \
                            float(metrics.at[i, extract_column_name(metric, "Vivado", label)])) * 100 / float(metrics.at[i, extract_column_name(metric, 'Yosys', label_base)]), '.1f')

                    except Exception:
                        metrics.at[i, extract_column_name("PERCENTAGE " + metric,"Vivado", label)] = '-'
        for output_yosys_dir in output_yosys_dirs:
            label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
            if label_base == label:
                continue
            for i in range(0, len(metrics['Benchmarks'])):
                for metric in yosys_metrics:
                    try:
                        metrics.at[i,extract_column_name("PERCENTAGE " + metric, "Yosys",label)] = format((float(metrics.at[i, extract_column_name(metric, 'Yosys', label_base)]) - \
                        float(metrics.at[i, extract_column_name(metric, "Yosys", label)])) * 100 / float(metrics.at[i, extract_column_name(metric, 'Yosys', label_base)]), '.1f')

                    except Exception:
                        metrics.at[i,extract_column_name("PERCENTAGE " + metric, "Yosys",label)] = "-"
              
    if output_vivado_dirs and (args.base in output_vivado_dirs):
        label_base = args.base.split(os.path.sep)[-2] + "_" + args.base.split(os.path.sep)[-1] 
        if len(output_vivado_dirs) > 1:
            vivado_metrics = metrics_list[1:-1]
            for output_vivado_dir in output_vivado_dirs:
                label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
                if label == label_base:
                    continue
                for i in range(0, len(metrics['Benchmarks'])):
                    for metric in vivado_metrics:
                        try:
                            metrics.at[i, extract_column_name("PERCENTAGE " + metric, "Vivado", label)] = format(((metrics.at[i, extract_column_name(metric, "Vivado", label_base)]) - \
                                float(metrics.at[i, extract_column_name(metric, "Vivado", label)])) * 100 / float(metrics.at[i, extract_column_name(metric, "Vivado", label_base)]), '.1f')
                        except Exception:
                            metrics.at[i, extract_column_name("PERCENTAGE " + metric, "Vivado", label)] = '-'
        vivado_metrics = vivado_lut_metrics + metrics_list[7:-1]
        for output_yosys_dir in output_yosys_dirs:
            label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
            for i in range(0, len(metrics['Benchmarks'])):
                for metric in vivado_metrics:
                    try:
                        if metric in vivado_lut_metrics:
                            metrics.at[i, extract_column_name("PERCENTAGE " + metric, "Yosys", label)] = format((float(metrics.at[i, extract_column_name(metric, 'Vivado', label_base)]) - \
                                float(metrics.at[i, extract_column_name(lut_label, "Yosys", label)])) * 100 / float(metrics.at[i, extract_column_name(metric, 'Vivado', label_base)]), '.1f')
                        else:
                            metrics.at[i, extract_column_name("PERCENTAGE " + metric,"Yosys",label)] = format((float(metrics.at[i, extract_column_name(metric, 'Vivado', label_base)]) - \
                                float(metrics.at[i, extract_column_name(metric, "Yosys", label)])) * 100 / float(metrics.at[i, extract_column_name(metric, 'Vivado', label_base)]), '.1f')
                    except Exception:
                        metrics.at[i, extract_column_name("PERCENTAGE " + metric,"Yosys",label)] = '-'
                
def add_max_min_average():
    global metrics
    comparision_list = ["", "AVERAGE", "MAX", "MIN"]
    percentage_list = [column for column in metrics.columns if "PERCENTAGE" in column]
    for i in comparision_list:
        metrics = metrics.append({'Benchmarks':i}, ignore_index=True)
    metrics.fillna('-', inplace=True)
    metrics.replace(to_replace='nan', value='-',inplace=True)
    for m in percentage_list:
        temp = []
        for x in metrics[m]:
            try:
                temp.append(float(x))
            except:
                pass
        if temp:
            average = sum(temp) / len(temp)
            metrics.at[get_design_index("AVERAGE"),m] = format(average, '.1f')
            metrics.at[get_design_index("MAX"),m] = format(max(temp), '.1f')
            metrics.at[get_design_index("MIN"),m] = format(min(temp), '.1f')

def remove_columns():
    global metrics
    columns_to_drop = [column for column in metrics.columns if\
        any(metric in column for metric in args.exclude_metrics)]
    metrics.drop(columns_to_drop, axis="columns", inplace=True)

def remove_carrys_from_viv_luts():
    if args.vivado:
        global output_vivado_dirs
        global metrics
        for output_vivado_dir in output_vivado_dirs:
            label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
            tool = "Vivado"
            metrics.rename(columns={
                extract_column_name("LUT:CARRY4=5*LUT",tool,label): extract_column_name("LUT",tool,label)
                }, inplace=True)
            if args.base:
                metrics.rename(columns={
                    extract_column_name("PERCENTAGE LUT:CARRY4=5*LUT",tool,label): extract_column_name("PERCENTAGE LUT",tool,label)
                    }, inplace=True)
                yosys = "Yosys"
                for output_yosys_dir in output_yosys_dirs:
                    yosys_label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
                    metrics.rename(columns={
                        extract_column_name("PERCENTAGE LUT:CARRY4=5*LUT",yosys,yosys_label): extract_column_name("PERCENTAGE LUT",yosys,yosys_label)
                        }, inplace=True)
            for i in range(0, len(metrics['Benchmarks'])):
                try:
                    metrics.at[i, extract_column_name("LUT",tool,label)] = \
                        int(metrics.at[i, extract_column_name("LUT:CARRY4=1*LUT",tool,label)]) - \
                        int(metrics.at[i, extract_column_name("CARRY4",tool,label)])
                except:
                    pass
        args.exclude_metrics.append("LUT:CARRY4=1*LUT")


def main():
    """Main function."""
    logger.info("Starting metrics extraction . . . . .")
    global metrics
    validate_inputs()
    base_metrics_list = [
        "LUT", 
        "LUT:CARRY4=1*LUT", 
        "LUT:CARRY4=5*LUT", 
        "LUT_AS_LOGIC", 
        "LUT_AS_MEMORY", 
        "MUXF7", 
        "MUXF8", 
        "CARRY4", 
        "MAX_LOGIC_LEVEL", 
        "AVERAGE_LOGIC_LEVEL", 
        "DFF", 
        "RUNTIME", 
        "SRL", 
        "DRAM", 
        "BRAM", 
        "DSP",
        "STATUS"
    ]
    init_columns(base_metrics_list)
    extract_yosys_metrics()
    extract_vivado_metrics()
    extract_run_log()
    calc_vivado_luts()
    if args.viv_carry_as_lut:
        remove_carrys_from_viv_luts()
    if args.base:
        calc_percentage(base_metrics_list)
        add_max_min_average()
    remove_columns()
    metrics_to_csv()

if __name__ == "__main__":
    startTime = time.time()
    args = parser.parse_args()
    main()
    endTime = time.time()
    logger.info("Merics extraction completed in %s seconds." % str(endTime - startTime))
