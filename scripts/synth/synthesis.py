# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Script Name   : run_task_generator.py
# Description   : This script will generate synthesis on the provided benchmark
#               : list provided in the input JSON settings file.
# Args          : python3 synthesis.py --help
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

import sys
import os
import argparse
import time
import logging
import shutil
import json
import re
import multiprocessing as mp
from datetime import datetime

if sys.version_info[0] < 3:
    raise Exception("Script must be run with Python 3")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Configure logging system
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
LOG_FORMAT = "%(levelname)8s - %(message)s"
logging.basicConfig(level=logging.DEBUG, stream=sys.stdout, format=LOG_FORMAT)
logger = logging.getLogger("benchmark_run_logs")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Read commandline arguments
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
parser = argparse.ArgumentParser(description="The script will run benchmarks provided by config.json file")
parser.add_argument("--config_files", type=str, nargs="*",
        help="the JSON settings file for the tasks generation.")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Initialize global variables
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
configuration_files = []
#abs_root_dir = os.getcwd() #dir_path = os.path.dirname(os.path.realpath(__file__))
abs_root_dir = os.path.abspath(os.path.join(__file__, "..", "..", ".."))

def error_exit(msg):
    """Exit with error message"""
    logger.error("Current working directory : " + os.getcwd())
    logger.error(msg)
    logger.error("Exiting . . . . . .")
    exit(1)


def validate_inputs():
    """
    Check if input files and directories exist and replace variables with
    absolute pathe.
    In case of error - print error message and exit the script.
    """
    global configuration_files
    for config_file in args.config_files:
        config_file = os.path.abspath(config_file)
        if not os.path.isfile(config_file):
            error_exit("The JSON settings file not found - %s" % config_file)
        configuration_files.append( (os.path.basename(config_file), config_file) )
    args.config_files = configuration_files


def main():
    """Main function."""
    logger.info("Starting synthesis scripts generation . . . . .")
    validate_inputs()

    synthesis_settings_list = []
    for cfg_name, config_file in configuration_files:
        try:
            with open(config_file) as f:
                synthesis_settings = json.load(f)
                synthesis_settings_list.append((cfg_name, synthesis_settings))
        except OSError as e:
            error_exit(e.strerror)
        except json.JSONDecodeError as e:
            error_exit(e.msg)

    now = datetime.now()
    run_dir_base = os.path.join(abs_root_dir, "result_" + now.strftime("%d-%m-%YT%H-%M-%S"))
    os.mkdir(run_dir_base)
    logger.info("Output directory - {0}".format(run_dir_base))

    for cfg_name, settings in synthesis_settings_list:
        run_benchmarks_for_config(settings, run_dir_base, cfg_name)



def run_benchmarks_for_config(synthesis_settings, run_dir_base, cfg_name):
            
    config_run_dir_base = os.path.join(run_dir_base, cfg_name)
    os.mkdir(config_run_dir_base)

    # running benchmarks parallel
    TIMEOUT = synthesis_settings["timeout"]
    pool = mp.Pool(synthesis_settings["num_process"])
    
    results = []
    if (synthesis_settings["tool"] == "yosys"):
        results = run_config_with_yosys(synthesis_settings, config_run_dir_base, cfg_name, pool)
    elif (synthesis_settings["tool"] == "vivado"):
        results = run_config_with_vivado(synthesis_settings, config_run_dir_base, cfg_name, pool)
    else:
        logger.error("Invalid tool in config file {0}".format(cfg_name))
        pool.terminate()
        pool.close()
        return
    
    for i, result in enumerate(results):
        try:
            return_value = result.get(TIMEOUT) # wait for up to TIMEOUT seconds
        except mp.TimeoutError:
            logger.error('Timeout for benchmark', i)
    pool.terminate()
    pool.close()


def run_config_with_yosys(synthesis_settings, config_run_dir_base, cfg_name, pool):
    read_hdl_base = "read"
    if (synthesis_settings["verific"]):
        read_hdl_base += " -verific"
    else:
        read_hdl_base += " -noverific"
    yosys_file_template = os.path.join(abs_root_dir, synthesis_settings["yosys_template_script"])
    yosys_abs = os.path.join(abs_root_dir, synthesis_settings["yosys_path"])
    abc_script = os.path.abspath( os.path.join(abs_root_dir, synthesis_settings["abc_script"]) )
    benchmarks = synthesis_settings["benchmarks"]

    results = [pool.apply_async(run_benchmark_with_yosys, args=(benchmark, yosys_abs, yosys_file_template, abc_script, config_run_dir_base, read_hdl_base, cfg_name)) for benchmark in benchmarks]
    return results
 

def run_config_with_vivado(synthesis_settings, config_run_dir_base, cfg_name, pool):
    benchmarks = synthesis_settings["benchmarks"]
    vivado_file_template = os.path.join(abs_root_dir, synthesis_settings["vivado_template_script"])
    results = [pool.apply_async(run_benchmark_with_vivado, args=(benchmark, vivado_file_template, config_run_dir_base, cfg_name)) for benchmark in benchmarks]
    return results

    
def run_benchmark_with_yosys(benchmark, yosys_path, yosys_file_template, abc_script, run_dir_base, read_hdl_base, cfg_name):
    abs_rtl_path = os.path.join(abs_root_dir, benchmark["rtl_path"])
    filename_extension = ""
    read_hdl = ""
    for filename in os.listdir(abs_rtl_path):
        if filename.endswith(".svh"):
            filename_extension = ".svh"
            read_hdl += "\nread -sv"
        elif filename.endswith(".vh"):
            filename_extension = ".vh"
            read_hdl += "\nread -vlog2k"
        else:
            filename_extension = ""
        if (filename_extension != "" and filename.endswith(filename_extension)):
            read_hdl += " " + filename
    for filename in os.listdir(abs_rtl_path):
        if filename.endswith(".vhd"):
            filename_extension = ".vhd"
            read_hdl += "\nread -vhdl"
        elif filename.endswith(".vhdl"):
            filename_extension = ".vhdl"
            read_hdl += "\nread -vhdl"
        elif filename.endswith(".sv"):
            filename_extension = ".sv"
            read_hdl += "\nread -sv"
        elif filename.endswith(".v"):
            filename_extension = ".v"
            read_hdl += "\nread -vlog2k"
        elif filename.endswith(".verilog"):
            filename_extension = ".verilog"
            read_hdl += "\nread -vlog2k"
        elif filename.endswith(".vlg"):
            filename_extension = ".vlg"
            read_hdl += "\nread -vlog2k"
        else:
            filename_extension = ""
        if (filename_extension != "" and filename.endswith(filename_extension)):
            read_hdl += " " + filename
    read_hdl = read_hdl_base + read_hdl
    
    benchmark_run_dir = os.path.join(run_dir_base, benchmark["name"])
    shutil.copytree(abs_rtl_path, benchmark_run_dir)
    yosys_file = os.path.join(benchmark_run_dir, "yosys.ys")
    
    rep = {"${READ_HDL}": read_hdl, "${TOP_MODULE}": benchmark["top_module"], "${BENCHMARK_NAME}": benchmark["name"], "${ABC_SCRIPT}": abc_script}
    create_file_from_template(yosys_file_template, rep, yosys_file) 
    
    startTime = time.time()
    logger.info('Starting synthesis run of {0} for configuration {1}'.format(benchmark["name"], cfg_name))
    try:
        os.system('cd {0}; {1} yosys.ys > yosys_output.log'.format(benchmark_run_dir, yosys_path))
    except Exception as e:
        logger.error('Synthesis run error for {0} with configuration {1}. Error message: {2}'.format(benchmark["name"], cfg_name, e.strerror))
    endTime = time.time()
    logger.info('Completed synthesis run of {0} with configuration {1} in {2} seconds.'.format(benchmark["name"], cfg_name, str(endTime - startTime)))


def run_benchmark_with_vivado(benchmark, vivado_file_template, config_run_dir_base, cfg_name):
    benchmark_run_dir = os.path.join(config_run_dir_base, benchmark["name"])
    abs_rtl_path = os.path.join(abs_root_dir, benchmark["rtl_path"])
    shutil.copytree(abs_rtl_path, benchmark_run_dir)
    vivado_file = os.path.join(benchmark_run_dir, "vivado_script.tcl")
    
    rep = {"${BENCHMARK_RUN_DIR}": benchmark_run_dir, "${TOP_MODULE}": benchmark["top_module"], "${BENCHMARK_NAME}": benchmark["name"]}
    create_file_from_template(vivado_file_template, rep, vivado_file)

    startTime = time.time()
    logger.info('Starting synthesis run of {0} for configuration {1}'.format(benchmark["name"], cfg_name))
    try:
        os.system('load_vivado; cd {0}; vivado -mode batch -source {1} -tempDir tmp > vivado_output.log'.format(benchmark_run_dir, vivado_file))
    except Exception as e:
        logger.error('Synthesis run error for {0} with configuration {1}. Error message: {2}'.format(benchmark["name"], cfg_name, e.strerror))
    endTime = time.time()
    logger.info('Completed synthesis run of {0} with configuration {1} in {2} seconds.'.format(benchmark["name"], cfg_name, str(endTime - startTime)))


def create_file_from_template(file_template, replacements, resulting_file):
    replacements = dict((re.escape(k), v) for k, v in replacements.items())
    pattern = re.compile("|".join(replacements.keys()))
    try:
        with open(file_template, "rt") as fin:
            with open(resulting_file, "wt") as fout:
                for line in fin:
                    result_line = pattern.sub(lambda m: replacements[re.escape(m.group(0))], line)
                    fout.write(result_line)
    except OSError as e:
        error_exit(e.strerror)
 

if __name__ == "__main__":
    startTime = time.time()
    args = parser.parse_args()
    main()
    endTime = time.time()
    logger.info("Synthesis run completed in %s seconds." % str(endTime - startTime))
