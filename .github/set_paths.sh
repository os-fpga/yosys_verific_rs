#!/bin/bash

set -e 

current_dir=$(pwd)
sed -e "s|yosys_path = \${PATH:OPENFPGA_PATH}|yosys_path = $current_dir|" -i ./OpenFPGA_RS/openfpga_flow/misc/fpgaflow_default_tool_path.conf
sed -e "s|abc_path = \${PATH:OPENFPGA_PATH}|abc_path = $current_dir|" -i ./OpenFPGA_RS/openfpga_flow/misc/fpgaflow_default_tool_path.conf
