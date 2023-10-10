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
import shlex
import json
import re
import multiprocessing as mp
import traceback
import subprocess
import signal
import copy
from datetime import datetime

if sys.version_info[0] < 3:
    raise Exception("Script must be run with Python 3")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Read commandline arguments
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
parser = argparse.ArgumentParser(description="The script will run benchmarks "
        "provided by config.json file")
parser.add_argument("--config_files", type=str, nargs="*",
        help="The JSON configuration files.")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Initialize global variables
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
configuration_files = []
abs_root_dir = os.path.abspath(os.path.join(__file__, "..", "..", ".."))
run_dir_base = None
now = datetime.now()

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Configure logging system
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
logFile = os.path.join(abs_root_dir, now.strftime("%d-%m-%YT%H-%M-%S") + ".log")
logging.basicConfig(
    level=logging.DEBUG,
    format="%(levelname)8s - %(message)s",
    handlers=[
        logging.FileHandler(logFile, mode="w"),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("synthesis")

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
        configuration_files.append( (os.path.basename(config_file), 
                config_file) )
    args.config_files = configuration_files


def main():
    """Main function."""
    logger.info("Starting synthesis for configs:")
    for config_file in args.config_files:
        logger.info("\t{0}".format(config_file))
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

    global run_dir_base
    global now
    run_dir_base = os.path.join(abs_root_dir, "result_" +\
            now.strftime("%d-%m-%YT%H-%M-%S"))
    os.mkdir(run_dir_base)
    logger.info("Output directory - {0}".format(run_dir_base))

    for cfg_name, settings in synthesis_settings_list:
        logger.info("Running synthesis for {0} config".format(cfg_name))
        startTime = time.time()
        run_benchmarks_for_config(settings, run_dir_base, cfg_name)
        endTime = time.time()
        logger.info("Finished synthesis for {0} config in {1} seconds".format(
                cfg_name, str(endTime - startTime)))


def run_benchmarks_for_config(synthesis_settings, run_dir_base, cfg_name):
            
    config_run_dir_base = os.path.join(run_dir_base, cfg_name)
    os.mkdir(config_run_dir_base)

    # running benchmarks parallel
    pool = mp.Pool(synthesis_settings["num_process"])
    
    results = []
    if (synthesis_settings["tool"] == "yosys"):
        results = run_config_with_yosys(synthesis_settings, 
                config_run_dir_base, cfg_name, pool)
    elif (synthesis_settings["tool"] == "vivado"):
        results = run_config_with_vivado(synthesis_settings, 
                config_run_dir_base, cfg_name, pool)
    elif (synthesis_settings["tool"] == "diamond"):
        results = run_config_with_diamond(synthesis_settings, 
                config_run_dir_base, cfg_name, pool)
    else:
        logger.error("Invalid tool in config file {0}".format(cfg_name))
        pool.terminate()
        pool.close()
        return
    for result in results:
        return_value = result[0].wait()
    pool.terminate()
    pool.close()


def run_config_with_yosys(synthesis_settings, config_run_dir_base, 
        cfg_name, pool):
    benchmarks = synthesis_settings["benchmarks"]
    TIMEOUT = synthesis_settings["timeout"]
    global_settings = {}
    results = []
    
    def collect_settings(row_settings, proccesed_settings):
        yosys_settings_names = ["yosys_template_script", "yosys_path", "abc_script"]
        if "yosys" in row_settings:
            yosys_settings = row_settings["yosys"]
            for setting in yosys_settings_names:
                if setting in yosys_settings:
                    proccesed_settings[setting] = os.path.join(abs_root_dir,
                        yosys_settings[setting])
            if "synth_rs" in yosys_settings:
                if "synth_rs" in proccesed_settings:
                    for k, v in yosys_settings["synth_rs"].items():
                        proccesed_settings["synth_rs"][k] = v
                else:
                    proccesed_settings["synth_rs"] = copy.deepcopy(yosys_settings["synth_rs"])

    collect_settings(synthesis_settings, global_settings)

    for benchmark in benchmarks:
        benchmark_settings = copy.deepcopy(global_settings)
        collect_settings(benchmark, benchmark_settings)
        error_flag = True        

        try:
            mandatory_settings_names = ["yosys_template_script", "yosys_path"]
            for setting in mandatory_settings_names:
                if setting not in benchmark_settings:
                    error_flag = False
                    logger.error('Missing {0} for {1} in configuration {2}.'.format(
                            setting, benchmark["name"], cfg_name))
        except:
            logger.error('Incorrect settings in configuration {}.'.format(
                        cfg_name))
        if error_flag:
            if "abc_script" not in benchmark_settings:
                benchmark_settings["abc_script"] = ""
            read_hdl_base = ""
            options = ""
            device = ""
            if "synth_rs" in benchmark_settings:
                for k, v in benchmark_settings["synth_rs"].items():
                    if k == "-tech":
                        device = v
                    if k == "-abc":
                        v = os.path.abspath( os.path.join(abs_root_dir, v))
                    if v:
                        if isinstance(v, bool): 
                            options += k + " "
                        else:
                            options += k + " " + v + " "
        results.append((pool.apply_async(run_benchmark_with_yosys,args=(
                benchmark, 
                benchmark_settings["yosys_path"], 
                benchmark_settings["yosys_template_script"],
                benchmark_settings["abc_script"], 
                options,
                device,
                config_run_dir_base, 
                read_hdl_base, 
                cfg_name, 
                TIMEOUT)), 
            benchmark))
    return results

def run_config_with_vivado(synthesis_settings, config_run_dir_base, 
        cfg_name, pool):
    results = []
    benchmarks = synthesis_settings["benchmarks"]
    TIMEOUT = synthesis_settings["timeout"]
    vivado_settings = {}   
    if "vivado" in synthesis_settings:
        if "vivado_template_script" in synthesis_settings["vivado"]:
            vivado_settings["vivado_template_script"] = os.path.join(abs_root_dir, synthesis_settings["vivado"]["vivado_template_script"])
    for benchmark in benchmarks:
        benchmark_settings = copy.deepcopy(vivado_settings)
        if "vivado" in benchmark:
            if "vivado_template_script" in benchmark["vivado"]:
                benchmark_settings["vivado_template_script"] = benchmark["vivado"]["vivado_template_script"]
        if benchmark_settings:
            results.append((pool.apply_async(run_benchmark_with_vivado,args=(
                    benchmark, 
                    benchmark_settings["vivado_template_script"], 
                    config_run_dir_base, 
                    cfg_name, 
                    TIMEOUT)), 
                 benchmark))
        else:
            logger.error('Please provide Vivado template script')
    return results

def run_config_with_diamond(synthesis_settings, config_run_dir_base, 
        cfg_name, pool):
    results = []
    benchmarks = synthesis_settings["benchmarks"]
    TIMEOUT = synthesis_settings["timeout"]
    diamond_settings = {}   
    if "diamond" in synthesis_settings:
        if "diamond_template_script" in synthesis_settings["diamond"]:
            diamond_settings["diamond_template_script"] = os.path.join(abs_root_dir, synthesis_settings["diamond"]["diamond_template_script"])
    for benchmark in benchmarks:
        benchmark_settings = copy.deepcopy(diamond_settings)
        if "diamond" in benchmark:
            if "diamond_template_script" in benchmark["diamond"]:
                benchmark_settings["diamond_template_script"] = benchmark["diamond"]["diamond_template_script"]
        if benchmark_settings:
            results.append((pool.apply_async(run_benchmark_with_diamond,args=(
                    benchmark, 
                    benchmark_settings["diamond_template_script"], 
                    config_run_dir_base, 
                    cfg_name, 
                    TIMEOUT)), 
                 benchmark))
        else:
            logger.error('Please provide diamond template script')
    return results
    

def run_benchmark_with_yosys(benchmark, yosys_path, yosys_file_template,
        abc_script, options, device, run_dir_base, read_hdl_base, cfg_name, timeout):
    try:
        abs_rtl_path = os.path.join(abs_root_dir, benchmark["rtl_path"])
        files_dict = {"v": [], "sv": [], "vhdl": []}
        flist_file = os.path.join(abs_rtl_path, "flist.flist")
        read_hdl = read_hdl_base
        files = None
        sim_bb = ""
        rs_plugin = ""
        if os.path.exists(flist_file):
            with open(flist_file) as fp:
                hdl_list = []
                curr_hdl_mode = None
                prev_hdl_mode = None
                files = [filename.strip() for filename in fp.readlines()]
                for filename in files:
                    if filename.endswith(".svh") or filename.endswith(".sv"):
                        curr_hdl_mode = "-sv"
                    elif filename.endswith(".vhd") or filename.endswith(".vhdl"):
                        curr_hdl_mode = "-vhdl"
                    elif filename.endswith(".vh") or filename.endswith(".v") \
                         or filename.endswith(".verilog") or filename.endswith(".vlg"):
                        curr_hdl_mode = "-vlog2k"
                    if not prev_hdl_mode:
                        read_hdl += "\nread " + curr_hdl_mode + " " + filename
                    elif prev_hdl_mode == curr_hdl_mode:
                        read_hdl += " " + filename
                    else:
                        read_hdl += "\nread " + curr_hdl_mode + " " + filename
                    prev_hdl_mode = curr_hdl_mode
        else:
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
                elif filename.endswith(".sv") and filename != "tb.sv":
                    files_dict["sv"].append(filename)
                elif filename.endswith(".v"):
                    files_dict["v"].append(filename)
                elif filename.endswith(".verilog"):
                    files_dict["v"].append(filename)
                elif filename.endswith(".vlg"):
                    files_dict["v"].append(filename)
            if files_dict["v"]:
                read_verilog = "\nread_verilog -I./ "+ "" + " ".join(files_dict["v"])  
                rs_plugin = "plugin -i synth-rs"
                read_hdl += read_verilog
                sim_bb = "read_verilog -sv " + run_dir_base + "/../../yosys/install/share/yosys/rapidsilicon/" + device + "/cell_sim_blackbox.v"
            if files_dict["sv"]:
                read_sv = "\nread -sv " + " ".join(files_dict["sv"])
                read_hdl += read_sv
            if files_dict["vhdl"]:
                read_vhdl = "\nread -vhdl " + " ".join(files_dict["vhdl"])
                read_hdl += read_vhdl
        
        benchmark_run_dir = os.path.join(run_dir_base, benchmark["name"])
        shutil.copytree(abs_rtl_path, benchmark_run_dir)
        yosys_file = os.path.join(benchmark_run_dir, "yosys.ys")
        
        options = "-top " + benchmark["top_module"] + " " + options

        rep = {"${PLUGINS}": rs_plugin, "${READ_HDL}": read_hdl, "${TOP_MODULE}": benchmark["top_module"],
                "${BENCHMARK_NAME}": benchmark["name"], "${ABC_SCRIPT}": abc_script,
                 "${OPTIONS}": options, "${READ_SIM_BB}": sim_bb,}
        create_file_from_template(yosys_file_template, rep, yosys_file) 
        os.chdir(benchmark_run_dir)
        run_command(benchmark["name"], cfg_name, "yosys_output.log", 
                [yosys_path, "yosys.ys"], timeout)

    except Exception as e:
        logger.error('Failed to execute synthesis of {0} for configuration '
                '{1}:\n {2}'.format(benchmark["name"], cfg_name, 
                traceback.format_exc()))

def run_benchmark_with_vivado(benchmark, vivado_file_template, 
        config_run_dir_base, cfg_name, timeout):
    try:
        benchmark_run_dir = os.path.join(config_run_dir_base, benchmark["name"])
        abs_rtl_path = os.path.join(abs_root_dir, benchmark["rtl_path"])
        shutil.copytree(abs_rtl_path, benchmark_run_dir)
        vivado_file = os.path.join(benchmark_run_dir, "vivado_script.tcl")
        flist_file = os.path.join(benchmark_run_dir, "flist.flist")
        benchmarks_list_str = None
        if os.path.exists(flist_file):
            with open(flist_file) as f:
                benchmarks_list_str = " ".join([shlex.quote(line.strip()) for line in f.readlines()])
        
        rep = {"${SOURCE_FILES}": benchmarks_list_str if benchmarks_list_str else benchmark_run_dir,
                "${BENCHMARK_RUN_DIR}": benchmark_run_dir, "${TOP_MODULE}": 
                benchmark["top_module"], "${BENCHMARK_NAME}": benchmark["name"]}
        create_file_from_template(vivado_file_template, rep, vivado_file)
        os.chdir(benchmark_run_dir)
        run_command(benchmark["name"], cfg_name, "vivado_output.log", ["vivado", 
                "-mode", "batch", "-source", vivado_file, 
                "-tempDir", "tmp"], timeout)

    except Exception as e:
        logger.error('Failed to execute synthesis of {0} for configuration '
                '{1}:\n {2}'.format(benchmark["name"], cfg_name, 
                traceback.format_exc()))

def run_benchmark_with_diamond(benchmark, diamond_file_template, 
        config_run_dir_base, cfg_name, timeout):
    try:
        benchmark_run_dir = os.path.join(config_run_dir_base, benchmark["name"])
        abs_rtl_path = os.path.join(abs_root_dir, benchmark["rtl_path"])
        shutil.copytree(abs_rtl_path, benchmark_run_dir)
        diamond_file = os.path.join(benchmark_run_dir, "diamond_script.tcl")
        top_module = os.path.join(benchmark_run_dir, benchmark["top_module"] + ".v")

        rep = {"${BENCHMARK_RUN_DIR}": benchmark_run_dir, 
                "${TOP_MODULE}": top_module, "${BENCHMARK_NAME}": benchmark["name"]}
        create_file_from_template(diamond_file_template, rep, diamond_file)
        os.chdir(benchmark_run_dir)
        run_command(benchmark["name"], cfg_name, "diamond_output.log", 
                ["diamondc", diamond_file], timeout)
    
    except Exception as e:
        logger.error('Failed to execute synthesis of {0} for configuration '
                '{1}:\n {2}'.format(benchmark["name"], cfg_name, 
                traceback.format_exc()))

def create_file_from_template(file_template, replacements, resulting_file):
    replacements = dict((re.escape(k), v) for k, v in replacements.items())
    pattern = re.compile("|".join(replacements.keys()))
    try:
        with open(file_template, "rt") as fin:
            with open(resulting_file, "wt") as fout:
                for line in fin:
                    result_line = pattern.sub(lambda m: replacements[
                            re.escape(m.group(0))], line)
                    fout.write(result_line)
    except OSError as e:
        error_exit(e.strerror)

def run_command(bench_name, cfg_name, logfile, command, timeout_s):
    logger.info('Starting synthesis run of {0} for configuration {1}'.format(
            bench_name, cfg_name))
    time_command = ["/usr/bin/time", "-p"]
    command = time_command + command
    process = None
    timeout = False
    startTime = time.time()
    with open(logfile, 'w') as output:
        try:
            process = subprocess.Popen(command, stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE, start_new_session=True)
            stdout, stderr = process.communicate(timeout=timeout_s)
            output.write(stdout.decode('utf-8'))
            output.write(stderr.decode('utf-8'))
        except subprocess.TimeoutExpired as e:
            timeout = True
            os.killpg(os.getpgid(process.pid), signal.SIGTERM)
            stdout, stderr = process.communicate()
            output.write(stdout.decode('utf-8'))
            output.write(stderr.decode('utf-8'))
        except Exception as e:
            logger.error('Failed to execute synthesis of {0} for configuration '
                    '{1}:\n {2}'.format(bench_name, cfg_name, 
                    traceback.format_exc()))
            return
    endTime = time.time()
    if process:
        if timeout:
            logger.error('Timeout of {0} seconds expired for synthesis '
                    'run of {1} for configuration {2}.'.format(str(timeout_s), 
                    bench_name, cfg_name))
        elif process.returncode:
            logger.error('Failed synthesis run of {0} for configuration ' 
                    '{1} in {2} seconds.'.format(bench_name, cfg_name, 
                    str(endTime - startTime)))
        else:
            logger.info('Successfully completed synthesis run of {0} for '
                    'configuration {1} in {2} seconds.'.format(bench_name,
                    cfg_name, str(endTime - startTime)))
 

if __name__ == "__main__":
    startTime = time.time()
    args = parser.parse_args()
    main()
    endTime = time.time()
    logger.info("Synthesis run completed in %s seconds." % str(
            endTime - startTime))
    if os.path.isdir(run_dir_base):
        shutil.move(logFile, os.path.join(run_dir_base, "run.log"))

