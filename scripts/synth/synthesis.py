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
        help="The JSON configuration files.")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Initialize global variables
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
configuration_files = []
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
            error_exit(config_file + ": " + str(e))

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
    elif (synthesis_settings["tool"] == "diamond"):
        results = run_config_with_diamond(synthesis_settings, config_run_dir_base, cfg_name, pool)
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
    read_hdl_base += "\nread -incdir ."
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


def run_config_with_diamond(synthesis_settings, config_run_dir_base, cfg_name, pool):
    benchmarks = synthesis_settings["benchmarks"]
    diamond_file_template = os.path.join(abs_root_dir, synthesis_settings["diamond_template_script"])
    results = [pool.apply_async(run_benchmark_with_diamond, args=(benchmark, diamond_file_template, config_run_dir_base, cfg_name)) for benchmark in benchmarks]
    return results
    

def run_benchmark_with_yosys(benchmark, yosys_path, yosys_file_template, abc_script, run_dir_base, read_hdl_base, cfg_name):
    abs_rtl_path = os.path.join(abs_root_dir, benchmark["rtl_path"])
    files_dict = {"v": [], "sv": [], "vhdl": []}
    for filename in os.listdir(abs_rtl_path):
        if filename.endswith(".svh"):
            files_dict["sv"].append(filename)
        elif filename.endswith(".vh"):
            files_dict["v"].append(filename)
    for filename in os.listdir(abs_rtl_path):
        if filename.endswith(".vhd"):
            files_dict["vhdl"].append(filename)
        elif filename.endswith(".vhdl"):
            files_dict["vhdl"].append(filename)
        elif filename.endswith(".sv"):
            files_dict["sv"].append(filename)
        elif filename.endswith(".v"):
            files_dict["v"].append(filename)
        elif filename.endswith(".verilog"):
            files_dict["v"].append(filename)
        elif filename.endswith(".vlg"):
            files_dict["v"].append(filename)
    read_hdl = read_hdl_base
    if files_dict["v"]:
        read_verilog = "\nread -vlog2k " + " ".join(files_dict["v"])
        read_hdl += read_verilog
    if files_dict["sv"]:
        read_sv = "\nread -sv " + " ".join(files_dict["sv"])
        read_hdl += read_sv
    if files_dict["vhdl"]:
        read_vhdl = "\nread -vhdl " + " ".join(files_dict["vhdl"])
        read_hdl += read_vhdl
    
    benchmark_run_dir = os.path.join(run_dir_base, benchmark["name"])
    shutil.copytree(abs_rtl_path, benchmark_run_dir)
    yosys_file = os.path.join(benchmark_run_dir, "yosys.ys")
    
    rep = {"${READ_HDL}": read_hdl, "${TOP_MODULE}": benchmark["top_module"], "${BENCHMARK_NAME}": benchmark["name"], "${ABC_SCRIPT}": abc_script}
    create_file_from_template(yosys_file_template, rep, yosys_file) 
    
    os.system('cd {0}'.format(benchmark_run_dir))
    run_command(benchmark["name"], cfg_name, "yosys_output.log", "{1} yosys.ys".format(yosys_path))


def run_benchmark_with_vivado(benchmark, vivado_file_template, config_run_dir_base, cfg_name):
    benchmark_run_dir = os.path.join(config_run_dir_base, benchmark["name"])
    abs_rtl_path = os.path.join(abs_root_dir, benchmark["rtl_path"])
    shutil.copytree(abs_rtl_path, benchmark_run_dir)
    vivado_file = os.path.join(benchmark_run_dir, "vivado_script.tcl")
    
    rep = {"${BENCHMARK_RUN_DIR}": benchmark_run_dir, "${TOP_MODULE}": benchmark["top_module"], "${BENCHMARK_NAME}": benchmark["name"]}
    create_file_from_template(vivado_file_template, rep, vivado_file)
    os.system('cd {0}'.format(benchmark_run_dir))
    run_command(benchmark["name"], cfg_name, "vivado_output.log", "vivado -mode batch -source {1} -tempDir tmp".format(vivado_file))


def run_benchmark_with_diamond(benchmark, diamond_file_template, config_run_dir_base, cfg_name):
    benchmark_run_dir = os.path.join(config_run_dir_base, benchmark["name"])
    abs_rtl_path = os.path.join(abs_root_dir, benchmark["rtl_path"])
    shutil.copytree(abs_rtl_path, benchmark_run_dir)
    diamond_file = os.path.join(benchmark_run_dir, "diamond_script.tcl")
    top_module = os.path.join(benchmark_run_dir, benchmark["top_module"] + ".v")
    rep = {"${BENCHMARK_RUN_DIR}": benchmark_run_dir, "${TOP_MODULE}": top_module, "${BENCHMARK_NAME}": benchmark["name"]}
    create_file_from_template(diamond_file_template, rep, diamond_file)

    startTime = time.time()
    logger.info('Starting synthesis run of {0} for configuration {1}'.format(benchmark["name"], cfg_name))
    try:
        os.system('cd {0}; diamondc {1} > diamond_output.log'.format(benchmark_run_dir, diamond_file))
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

def run_command(bench_name, cfg_name, logfile, command):
    logger.info('Starting synthesis run of {0} for configuration {1}'.format(bench_name, cfg_name))
    with open(logfile, 'w') as output:
        try:
            startTime = time.time()
            process = subprocess.run(command,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE,
                                     universal_newlines=True)
            endTime = time.time()
            output.write(process.stdout)
            output.write(process.stderr)
            output.write(str(process.returncode))
            if process.returncode:
                logger.error('Failed synthesis run of {0} for configuration {1} in {2} seconds.'.format(bench_name, cfg_name, str(endTime - startTime)))
        except Exception:
            logger.error('Failed to execute synthesis of {0} for configuration {1}. Error message: {2}'.format(bench_name, cfg_name, e.strerror))
    logger.info('Completed synthesis run of {0} for configuration {1} in {2} seconds.'.format(bench_name, cfg_name, str(endTime - startTime)))
    return process.stdout
 

if __name__ == "__main__":
    startTime = time.time()
    args = parser.parse_args()
    main()
    endTime = time.time()
    logger.info("Synthesis run completed in %s seconds." % str(endTime - startTime))
