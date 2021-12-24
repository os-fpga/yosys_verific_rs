# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Script Name   : run_task_generator.py
# Description   : This script will generate synthesis on the provided benchmark
#		  list provided in the input JSON settings file.
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
abs_root_dir = os.getcwd() #dir_path = os.path.dirname(os.path.realpath(__file__))

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
	run_dir_base = os.path.join(abs_root_dir, "result_" + now.strftime("%d_%m_%YT%H_%M_%S"))
	os.mkdir(run_dir_base)

	for cfg_name, settings in synthesis_settings_list:
		run_benchmarks_for_config(settings, run_dir_base, cfg_name)



def run_benchmarks_for_config(synthesis_settings, run_dir_base, cfg_name):
	read_hdl_base = "read"
	if (synthesis_settings["verific"]):
		read_hdl_base += " -verific"
	else:
		read_hdl_base += " -noverific"
	yosys_file_template = os.path.join(abs_root_dir, synthesis_settings["yosys_template_script"])
	yosys_abs = os.path.join(abs_root_dir, synthesis_settings["yosys_path"])
	os.system('export PATH=$PATH:{0};'.format(yosys_abs))
	
	benchmarks = synthesis_settings["benchmarks"]
	abc_script = os.path.abspath( os.path.join(abs_root_dir, synthesis_settings["abc_script"]) )
	config_run_dir_base = os.path.join(run_dir_base, cfg_name)
	os.mkdir(config_run_dir_base)

	# running benchmarks parallel
	TIMEOUT = 10800
	pool = mp.Pool(mp.cpu_count())
	results = [pool.apply_async(run_benchmark, args=(benchmark, yosys_file_template, abc_script, config_run_dir_base, read_hdl_base)) for benchmark in benchmarks]
	for i, result in enumerate(results):
		try:
			return_value = result.get(TIMEOUT) # wait for up to TIMEOUT seconds
		except mp.TimeoutError:
			print('Timeout for benchmark', i)
	pool.terminate()
	pool.close()


	
def run_benchmark(benchmark, yosys_file_template, abc_script, run_dir_base, read_hdl_base):
	abs_rtl_path = os.path.join(abs_root_dir, benchmark["rtl_path"])
	file_extension = ""
	read_hdl = ""
	for file in os.listdir(abs_rtl_path):
		if (file_extension == ""):
			if file.endswith(".vhd"):
				file_extension = ".vhd"
				read_hdl += "\nread -vhdl"
			elif file.endswith(".sv"):
				file_extension = ".sv"
				read_hdl += "\nread -sv"
			elif file.endswith(".v"):
				file_extension = ".v"
				read_hdl += "\nread -formal"
		if (file_extension != "" and file.endswith(file_extension)):
			read_hdl += " " + file
	read_hdl = read_hdl_base + read_hdl
	
	benchmark_run_dir = os.path.join(run_dir_base, benchmark["name"])
	shutil.copytree(abs_rtl_path, benchmark_run_dir)
	yosys_file = os.path.join(benchmark_run_dir, "yosys.ys")

	rep = {"${READ_HDL}": read_hdl, "${TOP_MODULE}": benchmark["top_module"], "${BENCHMARK_NAME}": benchmark["name"], "${ABC_SCRIPT}": abc_script}
	rep = dict((re.escape(k), v) for k, v in rep.items())
	pattern = re.compile("|".join(rep.keys()))
	
	try:
		with open(yosys_file_template, "rt") as fin:
			with open(yosys_file, "wt") as fout:
				for line in fin:
					result_line = pattern.sub(lambda m: rep[re.escape(m.group(0))], line)
					fout.write(result_line)
	except OSError as e:
		error_exit(e.strerror)
	os.system('cd {0}; ls; yosys yosys.ys > yosys_output.log'.format(benchmark_run_dir))


if __name__ == "__main__":
    startTime = time.time()
    args = parser.parse_args()
    main()
    endTime = time.time()
    logger.info("Benchmark synthesis run completed in %s seconds." % str(endTime - startTime))
