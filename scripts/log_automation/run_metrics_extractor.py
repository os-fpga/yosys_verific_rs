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
parser.add_argument("--run_log", type=str,
        help="log file of tool's run")
parser.add_argument("--base", type=str,
        help="base for the calculations")


# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Initialize global variables
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
output_vivado_dirs = []
output_yosys_dirs = []
metrics = pd.DataFrame()
metrics = pd.DataFrame(metrics,columns=["Benchmarks"])
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
    if not os.path.isfile(os.path.abspath(args.run_log)):
        error_exit("Provided file is not found - %s" % 
            args.run_log)
    if args.base:
        args.base = os.path.abspath(args.base[:-1] if args.base.endswith('/') else args.base)
        if not ((args.base in output_yosys_dirs) or (args.base in output_vivado_dirs)):
            error_exit("Incorrect base for the calculations - %s" %
                args.base)

def metrics_to_csv():
    logger.info("Saving into : " + args.output_file)
    metrics.fillna('-', inplace=True)
    metrics.to_csv(args.output_file, encoding="utf-8-sig")

def get_design_index(design_name):
    return metrics[metrics['Benchmarks'] == design_name].index.item()

def extract_column_name(metric, tool, label):
    return metric + " " + tool + " " + label

def init_columns(metric_list):
    global metrics
    global output_yosys_dirs
    global output_vivado_dirs
    for metric in metric_list:
        if not (metric == metric_list[0]):
            for output_vivado_dir in output_vivado_dirs:
                label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
                metrics[extract_column_name(metric,"Vivado",label)] = '-'
        if metric not in metric_list[1 : 8]:
            for output_yosys_dir in output_yosys_dirs:
                label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
                metrics[extract_column_name(metric,"Yosys",label)] = '-'

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
            logger.info("Processing Yosys log : " + task_log)
            try:
                with open(task_log, 'r') as f:
                    results = re.findall(r"Printing statistics.*\n\n===.*===\n\n(.*)\n\n", f.read(), re.DOTALL)
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
            except OSError as e:
                error_exit(e.strerror)
   
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
            except OSError as e:
                logger.error(e.strerror)

def extract_run_log():
    global metrics

    if args.vivado:
        global output_vivado_dirs
        for output_vivado_dir in output_vivado_dirs:
            label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
            tool = "Vivado"
            with open(args.run_log, "r") as f:
                for line in f:
                    value = line.split()
                    if ("Successfully" in value) and (output_vivado_dir.split(os.path.sep)[-1] in value):
                        design_index = get_design_index(value[7])
                        metrics.at[design_index, extract_column_name("RUNTIME",tool,label)] = format(float(value[12]), '.1f')
                        metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Pass"
                    elif ("Failed" in value) and (output_vivado_dir.split(os.path.sep)[-1] in value) :
                        design_index = get_design_index(value[6])
                        metrics.at[design_index, extract_column_name("RUNTIME",tool,label)] = format(float(value[11]), '.1f')
                        metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Fail"
                    elif ("Timeout" in value):
                        design_index = get_design_index(value[11])
                        metrics.at[design_index, extract_column_name("RUNTIME",tool,label)] = format(float(value[4]), '.1f')
                        metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Timeout"

    if args.yosys:
        global output_yosys_dirs
        for output_yosys_dir in output_yosys_dirs:
            label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
            tool = "Yosys"
            with open(args.run_log, "r") as f:
                for line in f:
                    value = line.split()
                    if ("Successfully" in value) and (output_yosys_dir.split(os.path.sep)[-1] in value):
                        design_index = get_design_index(value[7])
                        metrics.at[design_index, extract_column_name("RUNTIME",tool,label)] = format(float(value[12]), '.1f')
                        metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Pass"
                    elif ("Failed" in value) and (output_yosys_dir.split(os.path.sep)[-1] in value):
                        design_index = get_design_index(value[6])
                        metrics.at[design_index, extract_column_name("RUNTIME",tool,label)] = format(float(value[11]), '.1f')
                        metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Fail"
                    elif ("Timeout" in value):
                        design_index = get_design_index(value[11])
                        metrics.at[design_index, extract_column_name("RUNTIME",tool,label)] = format(float(value[4]), '.1f')
                        metrics.at[design_index, extract_column_name("STATUS",tool,label)] = "Timeout"

def calc_vivado_luts():
    if args.vivado:
        global output_vivado_dirs
        global metrics
        title_columns = list(metrics.columns)
        for output_vivado_dir in output_vivado_dirs:
            label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
            tool = "Vivado"
            for i in range(0, len(metrics['Benchmarks'])):
                if metrics.at[i, extract_column_name("STATUS",tool,label)] == "Pass":
                    metrics.at[i, extract_column_name("LUT:CARRY4=5*LUT",tool,label)] = \
                        int(metrics.at[i, extract_column_name("LUT:CARRY4=1*LUT",tool,label)]) - \
                        int(metrics.at[i, extract_column_name("CARRY4",tool,label)]) + \
                        5 * int(metrics.at[i, extract_column_name("CARRY4",tool,label)])
                else:
                    metrics.at[i, extract_column_name("LUT:CARRY4=1*LUT",tool,label)] = '-'
                    metrics.at[i, extract_column_name("LUT:CARRY4=5*LUT",tool,label)] = '-'

                ind = title_columns.index(extract_column_name("LUT:CARRY4=1*LUT",tool,label))
                temp = metrics.pop(extract_column_name("LUT:CARRY4=5*LUT",tool,label))
                metrics.insert(ind + 1, extract_column_name("LUT:CARRY4=5*LUT",tool,label), temp)

def calc_percentage(percentage_list):
    global output_yosys_dirs
    global output_vivado_dirs
    label_base = None
    if output_yosys_dirs and (args.base in output_yosys_dirs):
        label_base = args.base.split(os.path.sep)[-2] + "_" + args.base.split(os.path.sep)[-1] 
        for metric in percentage_list[1::]:
            var_carry = metric.split()[1]
            for output_vivado_dir in output_vivado_dirs:
                label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
                for i in range(0, len(metrics['Benchmarks'])):
                    try:
                        metrics.at[i, extract_column_name(metric,"Vivado",label)] = format((float(metrics.at[i, extract_column_name("LUT", 'Yosys', label_base)]) - \
                            float(metrics.at[i, extract_column_name(var_carry, "Vivado", label)])) * 100 / float(metrics.at[i, extract_column_name("LUT", 'Yosys', label_base)]), '.1f')
                    except Exception:
                        metrics.at[i, extract_column_name(metric,"Vivado",label)] = '-'
        for output_yosys_dir in output_yosys_dirs:
            comparision_suit = output_yosys_dir.split(os.path.sep)[-1]
            label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
            if comparision_suit in args.base:
                continue
            for i in range(0, len(metrics['Benchmarks'])):
                try:
                    metrics.at[i,extract_column_name(percentage_list[0],"Yosys",label)] = format((float(metrics.at[i, extract_column_name("LUT", 'Yosys', label_base)]) - \
                        float(metrics.at[i, extract_column_name("LUT", "Yosys", label)])) * 100 / float(metrics.at[i, extract_column_name("LUT", 'Yosys', label_base)]), '.1f')
                except Exception:
                    metrics.at[i,extract_column_name(percentage_list[0],"Yosys",label)] = '-'
              
    if output_vivado_dirs and (args.base in output_vivado_dirs):
        label_base = args.base.split(os.path.sep)[-2] + "_" + args.base.split(os.path.sep)[-1] 
        for metric in percentage_list[1::]:
            var_carry = metric.split()[1]
            #if one suite provided for vivado and that one is base , it cannot be compared with itself
            if len(args.vivado) > 1:
                for output_vivado_dir in output_vivado_dirs:
                    comparision_suit = output_vivado_dir.split(os.path.sep)[-1]
                    if comparision_suit in args.base:
                        continue
                    label = output_vivado_dir.split(os.path.sep)[-2] + "_" + output_vivado_dir.split(os.path.sep)[-1]
                    for i in range(0, len(metrics['Benchmarks'])):
                        try:
                            metrics.at[i, extract_column_name(metric,"Vivado",label)] = format(((metrics.at[i, extract_column_name(var_carry,"Vivado",label_base)]) - \
                                float(metrics.at[i, extract_column_name(var_carry,"Vivado",label)])) * 100 / float(metrics.at[i, extract_column_name(var_carry,"Vivado",label_base)]), '.1f')
                        except Exception:
                            metrics.at[i, extract_column_name(metric,"Vivado",label)] = '-'
            for output_yosys_dir in output_yosys_dirs:
                label = output_yosys_dir.split(os.path.sep)[-2] + "_" + output_yosys_dir.split(os.path.sep)[-1]
                for i in range(0, len(metrics['Benchmarks'])):
                    try:
                        metrics.at[i, extract_column_name(metric,"Yosys",label)] = format((float(metrics.at[i, extract_column_name(var_carry, 'Vivado', label_base)]) - \
                            float(metrics.at[i, extract_column_name("LUT", "Yosys", label)])) * 100 / float(metrics.at[i, extract_column_name(var_carry, 'Vivado', label_base)]), '.1f')
                    except Exception:
                        metrics.at[i, extract_column_name(metric,"Yosys",label)] = '-'

def reorder_columns():
    global metrics
    title_columns =  list(metrics.columns)
    title_luts = [column for column in title_columns if column.startswith("LUT_AS_LOGIC")]
    i = 0
    if len(title_luts) != 0:
        ind = title_columns.index(title_luts[0])
        for column in title_columns:
            if 'PERCENTAGE' in column:
                temp = metrics.pop(column)
                metrics.insert(ind + i, column, temp)
                i += 1
    if len(title_luts) == 0:
        title_luts = [column for column in title_columns if column.startswith("LUT ")]
        ind = title_columns.index(title_luts[-1])
        i = 1
        for column in title_columns:
            if 'PERCENTAGE' in column:
                temp = metrics.pop(column)
                metrics.insert(ind + i, column, temp)
                i += 1

def main():
    """Main function."""
    logger.info("Starting metrics extraction . . . . .")
    global metrics
    validate_inputs()
    percentage_list = ["PERCENTAGE", "PERCENTAGE LUT:CARRY4=1*LUT", "PERCENTAGE LUT:CARRY4=5*LUT"]
    metric_list = ["LUT", "LUT:CARRY4=1*LUT", "LUT:CARRY4=5*LUT", "LUT_AS_LOGIC", "LUT_AS_MEMORY", "CARRY4", "MUXF7", "MUXF8", "DFF", "RUNTIME", "SRL", "DRAM", "BRAM", "DSP", "STATUS"]
    init_columns(metric_list)
    extract_yosys_metrics()
    extract_vivado_metrics()
    extract_run_log()
    calc_vivado_luts()
    calc_percentage(percentage_list)
    reorder_columns()
    metrics_to_csv()

if __name__ == "__main__":
    startTime = time.time()
    args = parser.parse_args()
    main()
    endTime = time.time()
    logger.info("Merics extraction completed in %s seconds." % str(endTime - startTime))
