# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Script Name   : run_task_generator.py
# Description   : This script will generate openfpga tasks based on the 
#                 settings provided in the input JSON file.
# Args          : python3 run_task_generator.py --help
# Author        : Aram Kostanyan
# Email         : aram@rapidsilicon.com
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

import sys
import os
import argparse
import time
import logging
import shutil
import json
from configparser import ConfigParser, ExtendedInterpolation

if sys.version_info[0] < 3:
    raise Exception("run_task_generator script must be run with Python 3")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Configure logging system
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
LOG_FORMAT = "%(levelname)8s - %(message)s"
logging.basicConfig(level=logging.DEBUG, stream=sys.stdout, format=LOG_FORMAT)
logger = logging.getLogger("task_generator_logs")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Read commandline arguments
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
parser = argparse.ArgumentParser(description="The script will generate \
        OpenFPGA tasks based on existing tasks and the settings provided \
        in the input JSON file. It will read the input JSON file, \
        copy each 'original_task_dir' to 'new_task_dir' in the provided \
        OpenFPGA directory and set all configuration settings specified in \
        the 'config_sections' for each new task correspondingly.")
parser.add_argument("openfpga_path", type=str, 
        help="path to OpenFPGA root directory")
parser.add_argument("--settings_file", type=str,
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                "default_settings.json"),
        help="the JSON settings file for the tasks generation.")
parser.add_argument("--reg_tests", action="store_true",
        help="generate also regression tests script.")
parser.add_argument("--debug", action="store_true",
        help="run script in debug mode.")

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# Initialize global variables
# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
tasks_dir = ""
generator_settings = None
reg_tests_list = []
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
    args.settings_file = os.path.abspath(args.settings_file)
    if not os.path.isfile(args.settings_file):
        error_exit("The JSON settings file not found - %s" %
                args.settings_file)
    args.openfpga_path = os.path.abspath(args.openfpga_path)
    if not os.path.isdir(args.openfpga_path):
        error_exit("The OpenFPGA directory not found - %s" %
                args.openfpga_path)
    global tasks_dir
    tasks_dir = os.path.join(args.openfpga_path, "openfpga_flow", "tasks")
    if not os.path.isdir(tasks_dir):
        error_exit("The OpenFPGA tasks directory not found - %s" %
                tasks_dir)


def generate_regression_tests():
    global reg_tests_list
    setup_lines = [
            "#!/bin/bash",
            "set -e",
            "source openfpga.sh",
            "PYTHON_EXEC=python3.8",
            "###############################################",
            "# OpenFPGA Shell with VPR8",
            "###############################################",
            "echo -e \"yosys+verific regression tests\";",
            "\n"
            ]
    abs_script_name = os.path.join(args.openfpga_path, "openfpga_flow",
            "regression_test_scripts", "yosys+verific_reg_test.sh")
    try:
        with open(abs_script_name, "w") as f:
            f.write("\n".join(setup_lines))
            for test in reg_tests_list:
                f.write(" ".join(["run-task", test, 
                        "--debug --show_thread_logs"]))
                f.write("\n")
                if args.debug:
                    logger.debug("Adding to regression tests - %s" % test)
    except OSError as e:
        error_exit(e.strerror)
    logger.info("Regression test has been created - %s" % abs_script_name)


def main():
    """Main function."""
    logger.info("Starting tasks generation . . . . .")
    validate_inputs()
    global generator_settings
    global reg_tests_list

    try:
        with open(args.settings_file) as f:
            generator_settings = json.load(f)
    except OSError as e:
        error_exit(e.strerror)
    except json.JSONDecodeError as e:
        error_exit(e.msg)

    for task_settings in generator_settings:
        abs_orig_task_dir = os.path.join(tasks_dir,
                task_settings["original_task_dir"])
        if not os.path.isdir(abs_orig_task_dir):
            logger.warning("The task directory not found - %s" %
                    abs_orig_task_dir)
            continue
        abs_new_task_dir = os.path.join(tasks_dir,
                task_settings["new_task_dir"])
        if os.path.isdir(abs_new_task_dir):
            logger.warning("The task directory already exists, "\
                    "will not overwrite - %s" % abs_new_task_dir)
            continue
        shutil.copytree(abs_orig_task_dir, abs_new_task_dir)
        abs_task_conf = os.path.join(abs_new_task_dir, "config", "task.conf")
        if not os.path.isfile(abs_task_conf):
            logger.warning("The task config file not found - %s" %
                    abs_task_conf)
            continue
        if args.debug:
            logger.debug("Generating new task based on - %s" % 
                    abs_orig_task_dir)
        task_config = ConfigParser(allow_no_value=True,
                interpolation=ExtendedInterpolation())
        try:
            with open(abs_task_conf) as f:
                task_config.read_file(f)
        except OSError as e:
            error_exit(e.strerror)

        if "discard_config_sections" in task_settings:
            for section, section_settings in \
                   task_settings["discard_config_sections"].items():
                if not section_settings:
                    task_config[section].clear()
                else:
                    task_config_section = task_config[section]
                    for key in section_settings:
                        del task_config_section[key]
        if "config_sections" in task_settings:
            for section, section_settings in \
                   task_settings["config_sections"].items():
                task_config_section = task_config[section]
                for key in section_settings:
                    task_config_section[key] = section_settings[key].\
                           replace("${ROOT_PATH}", abs_root_dir)

        try:
            with open(abs_task_conf, "w") as f:
                task_config.write(f)
        except OSError as e:
            error_exit(e.strerror)

        if args.reg_tests:
            reg_tests_list.append(task_settings["new_task_dir"])

        if args.debug:
            logger.debug("New task has been generated - %s" %
                    abs_new_task_dir)
    
    if args.reg_tests:
        if reg_tests_list:
            generate_regression_tests()
        else:
            logger.warning("There is no test to add into regression tests.")


if __name__ == "__main__":
    startTime = time.time()
    args = parser.parse_args()
    main()
    endTime = time.time()
    logger.info("OpenFPGA tasks generation completed in %s seconds." % str(endTime - startTime))
