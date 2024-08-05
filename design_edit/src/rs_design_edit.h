#ifndef DESIGN_EDIT_UTILS_H
#define DESIGN_EDIT_UTILS_H

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <regex>
#include <set>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

using namespace std;

struct primitives_data_default {
  std::map<std::string, std::unordered_set<std::string>> io_primitives =
      { // TO_DO Read from Yaml
          {"genesis3",
           {"CLK_BUF", "I_BUF", "I_BUF_DS", "I_DDR", "I_DELAY", "I_SERDES",
            "O_BUF", "O_BUFT", "O_BUFT_DS", "O_BUF_DS", "O_DDR", "O_DELAY",
            "O_SERDES", "O_SERDES_CLK", "PLL"}}};
  bool contains_io_prem = false;
  bool supported_tech = false;

  // Function to get the primitive names for a specific cell library
  std::unordered_set<std::string> get_primitives(const std::string &lib) {
    std::unordered_set<std::string> primitive_names;
    auto it = io_primitives.find(lib);
    if (it != io_primitives.end()) {
      supported_tech = true;
      primitive_names = it->second;
    }
    return primitive_names;
  }
};

struct pin_data {
  pin_data(const std::string& n) : _name(n) {
    log_assert(_name.size());
  }
  const std::string _name = "";
  std::string _location = "";
  std::string _internal_pin = "";
  std::unordered_map<string, std::string> _properties;
  void print(std::ostream &output) {
    output << "name: " << _name << std::endl;
    output << "  pin location: " << _location << std::endl;
    output << "  internal pin: " << _internal_pin << std::endl;
    output << "  properties: " << std::endl;
    for (auto &pr : _properties) {
      output << "    " << pr.first << " : " << pr.second << std::endl;
    }
  }
};

enum Technologies { GENERIC, GENESIS, GENESIS_2, GENESIS_3 };
std::vector<pin_data*> pins;
std::vector<std::string> wrapper_files;
std::vector<std::string> post_route_wrapper;
std::unordered_set<std::string> clk_outs;
std::unordered_set<std::string> primitives;
std::unordered_set<std::string> new_ins;
std::unordered_set<std::string> new_outs;
std::unordered_set<std::string> interface_wires;
std::unordered_set<std::string> inputs;
std::unordered_set<std::string> outputs;
std::unordered_set<std::string> out_prim_ins;
std::unordered_set<std::string> in_prim_outs;
std::unordered_set<std::string> io_prim_wires;
std::unordered_set<std::string> common_clks_resets;
std::unordered_set<std::string> orig_inst_conns;
std::unordered_set<std::string> interface_inst_conns;
std::unordered_set<std::string> keep_wires;
std::string io_config_json;
std::string sdc_file;
bool sdc_passed = false;
std::string tech;
std::stringstream netlist_checker;
bool netlist_error = false;
std::unordered_set<std::string> dly_controls =
      {"DLY_LOAD", "DLY_ADJ", "DLY_INCDEC", "DLY_TAP_VALUE"};

std::vector<std::string> tokenizeString(const std::string &input);
void processSdcFile(std::istream &input);
void get_loc_map_by_io();
void write_checker_file();

#endif // DESIGN_EDIT_UTILS_H