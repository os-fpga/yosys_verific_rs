/**
 * @file rs_design_edit.cc
 * @author Behzad Mehmood (behzadmehmood82@gmail.com)
 * @author Manadher Kharroubi (manadher@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02
 *
 * @copyright Copyright (c) 2024
 */
#include "backends/rtlil/rtlil_backend.h"
#include "kernel/celltypes.h"
#include "kernel/ff.h"
#include "kernel/ffinit.h"
#include "kernel/log.h"
#include "kernel/mem.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
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

#ifdef PRODUCTION_BUILD
#include "License_manager.hpp"
#endif

int DSP_COUNTER;
USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

#define XSTR(val) #val
#define STR(val) XSTR(val)

#ifndef PASS_NAME
#define PASS_NAME design_edit
#endif

#define GENESIS_DIR genesis
#define GENESIS_2_DIR genesis2
#define GENESIS_3_DIR genesis3
#define COMMON_DIR common
#define VERSION_MAJOR 0 // 0 - beta
#define VERSION_MINOR 0
#define VERSION_PATCH 1

using namespace std;

struct primitives_data {
  std::map<std::string, std::unordered_set<std::string>> io_primitives =
      { // TO_DO Read from Yaml
          {"genesis3",
           {"CLK_BUF", "I_BUF", "I_BUF_DS", "I_DDR", "I_DELAY", "I_SERDES",
            "O_BUF", "O_BUFT", "O_BUFT_DS", "O_BUF_DS", "O_DDR", "O_DELAY",
            "O_SERDES", "O_SERDES_CLK", "PLL"}}};
  bool contains_io_prem = false;

  // Function to get the primitive names for a specific cell library
  std::unordered_set<std::string> get_primitives(const std::string &lib) {
    std::unordered_set<std::string> primitive_names;
    auto it = io_primitives.find(lib);
    if (it != io_primitives.end()) {
      primitive_names = it->second;
    }
    return primitive_names;
  }
};

enum Technologies { GENERIC, GENESIS, GENESIS_2, GENESIS_3 };

USING_YOSYS_NAMESPACE
using namespace RTLIL;

struct DesignEditRapidSilicon : public ScriptPass {
  DesignEditRapidSilicon()
      : ScriptPass("design_edit", "Netlist Editing Tool") {}

  void help() override {
    log("\n");
    log("This command runs Netlist editing tool\n");
    log("\n");
    log("    -verilog <file>\n");
    log("        Write the design to the specified verilog file. writing of an "
        "output file\n");
    log("        is omitted if this parameter is not specified.\n");
    log("\n");
    log("\n");
  }

  std::vector<std::string> wrapper_files;
  std::string tech;
  std::vector<Cell *> remove_prims;
  std::vector<Cell *> remove_non_prims;
  std::vector<Cell *> remove_wrapper_cells;
  std::unordered_set<std::string> primitives;
  std::unordered_set<std::string> new_ins;
  std::unordered_set<std::string> new_outs;
  std::unordered_set<std::string> interface_wires;
  std::unordered_set<std::string> inputs;
  std::unordered_set<std::string> outputs;
  std::unordered_set<std::string> orig_inst_conns;
  std::unordered_set<std::string> interface_inst_conns;
  std::unordered_set<std::string> keep_wires;
  std::unordered_set<Wire *> wires_interface;
  std::unordered_set<Wire *> del_ins;
  std::unordered_set<Wire *> del_outs;
  std::unordered_set<Wire *> del_interface_wires;
  std::unordered_set<Wire *> del_wrapper_wires;

  RTLIL::Design *_design;
  RTLIL::Design *new_design = new RTLIL::Design;
  ;
  primitives_data io_prim;

  void clear_flags() override { wrapper_files = {}; }

  std::string remove_backslashes(const std::string &input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
      if (c != '\\') {
        result.push_back(c); 
      }
    }
    return result;
  }

  void delete_cells(Module *module, vector<Cell *> cells) {
    for (auto cell : cells) {
      module->remove(cell);
    }
  }

  void delete_wires(Module *module, std::unordered_set<Wire *> wires) {
    for (auto wire : wires) {
      std::string wire_name = wire->name.str();
      if (keep_wires.find(wire_name) == keep_wires.end()) {
        module->remove({wire});
      }
    }
  }

  void intersection_copy_remove(std::unordered_set<std::string> &set1,
                  std::unordered_set<std::string> &set2,
                  std::unordered_set<std::string> &wires) {
    for (auto it = set1.begin(); it != set1.end();) {
      if (set2.find(*it) != set2.end()) {
        wires.insert(*it);
        it = set1.erase(it);
      } else {
        ++it;
      }
    }
    // Remove elements from set2 that are already moved to wires
    for (auto it = set2.begin(); it != set2.end();) {
      if (wires.find(*it) != wires.end()) {
        it = set2.erase(it);
      } else {
        ++it;
      }
    }
  }

  void process_wire(Cell *cell, const IdString &portName, RTLIL::Wire *wire) {
    if (cell->input(portName)) {
      if (wire->port_input) {
        inputs.insert(wire->name.str());
      } else {
        new_outs.insert(wire->name.str());
      }
    } else if (cell->output(portName)) {
      if (wire->port_output) {
        outputs.insert(wire->name.str());
      } else {
        new_ins.insert(wire->name.str());
      }
    }
  }

  bool is_flag(const std::string &arg) { return !arg.empty() && arg[0] == '-'; }

  std::string get_extension(const std::string &filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
      return filename.substr(dot_pos);
    }
    return ""; // If no extension found
  }

  void execute(std::vector<std::string> args, RTLIL::Design *design) override {
    std::string run_from, run_to;
    clear_flags();
    _design = design;

    size_t argidx;
    // TODO: Will send the arguments and test after parsing is done
    for (argidx = 1; argidx < args.size(); argidx++) {
      if (args[argidx] == "-w" && argidx + 1 < args.size()) {
        size_t next_argidx = argidx + 1;
        while (next_argidx < args.size() && !is_flag(args[next_argidx])) {
          wrapper_files.push_back(args[next_argidx]);
          ++next_argidx;
        }
        argidx = next_argidx - 1;
        continue;
      }
			if (args[argidx] == "-tech" && argidx + 1 < args.size())
			{
				tech = args[++argidx];
        continue;
			}
      break;
    }
    primitives = io_prim.get_primitives(tech);

    Module *original_mod = _design->top_module();
    std::string original_mod_name =
        remove_backslashes(_design->top_module()->name.str());
    Module *interface_mod = _design->top_module()->clone();
    std::string interface_mod_name = "\\interface_" + original_mod_name;
    interface_mod->name = interface_mod_name;
    Module *wrapper_mod = original_mod->clone();
    std::string wrapper_mod_name = "\\wrapper_" + original_mod_name;
    wrapper_mod->name = wrapper_mod_name;
    for (auto cell : original_mod->cells()) {
      string module_name = remove_backslashes(cell->type.str());
      if (std::find(primitives.begin(), primitives.end(), module_name) !=
          primitives.end()) {
        io_prim.contains_io_prem = true;
        remove_prims.push_back(cell);
        for (auto conn : cell->connections()) {
          IdString portName = conn.first;
          RTLIL::SigSpec actual = conn.second;
          if (actual.is_chunk()) {
            RTLIL::Wire *wire = actual.as_chunk().wire;
            if (wire != NULL) {
              process_wire(cell, portName, wire);
            }
          } else {
            for (auto it = actual.chunks().rbegin();
                 it != actual.chunks().rend(); ++it) {
              RTLIL::Wire *wire = (*it).wire;
              if (wire != NULL) {
                process_wire(cell, portName, wire);
              }
            }
          }
        }
      } else {
        for (auto conn : cell->connections()) {
          IdString portName = conn.first;
          RTLIL::SigSpec actual = conn.second;
          if (actual.is_chunk()) {
            RTLIL::Wire *wire = actual.as_chunk().wire;
            if (wire != NULL) {
              keep_wires.insert(wire->name.str());
            }
          } else {
            for (auto it = actual.chunks().rbegin();
                 it != actual.chunks().rend(); ++it) {
              RTLIL::Wire *wire = (*it).wire;
              if (wire != NULL) {
                keep_wires.insert(wire->name.str());
              }
            }
          }
        }
      }
    }
    if (!io_prim.contains_io_prem) {
      return;
    }

    delete_cells(original_mod, remove_prims);

    intersection_copy_remove(new_ins, new_outs, interface_wires);

    for (auto wire : original_mod->wires()) {
      std::string wire_name = wire->name.str();
      if (new_ins.find(wire_name) != new_ins.end()) {
        wire->port_input = true;
        continue;
      }
      if (new_outs.find(wire_name) != new_outs.end()) {
        wire->port_output = true;
        continue;
      }
      if (interface_wires.find(wire_name) != interface_wires.end()) {
        wires_interface.insert(wire);
        continue;
      }
      if (inputs.find(wire_name) != inputs.end()) {
        del_ins.insert(wire);
        continue;
      }
      if (outputs.find(wire_name) != outputs.end()) {
        del_outs.insert(wire);
        continue;
      }
    }

    for (auto &conn : original_mod->connections()) {
      std::vector<RTLIL::SigBit> conn_lhs = conn.first.to_sigbit_vector();
      std::vector<RTLIL::SigBit> conn_rhs = conn.second.to_sigbit_vector();
      for (size_t i = 0; i < conn_lhs.size(); i++) {
        if (conn_lhs[i].wire != nullptr) {
          keep_wires.insert(conn_lhs[i].wire->name.str());
        }
        if (conn_rhs[i].wire != nullptr) {
          keep_wires.insert(conn_rhs[i].wire->name.str());
        }
      }
    }

    delete_wires(original_mod, wires_interface);
    delete_wires(original_mod, del_ins);
    delete_wires(original_mod, del_outs);

    original_mod->fixup_ports();

    for (auto cell : interface_mod->cells()) {
      string module_name = remove_backslashes(cell->type.str());
      if (std::find(primitives.begin(), primitives.end(), module_name) ==
          primitives.end()) {
        remove_non_prims.push_back(cell);
      }
    }

    delete_cells(interface_mod, remove_non_prims);

    for (auto wire : interface_mod->wires()) {
      std::string wire_name = wire->name.str();
      if (new_ins.find(wire_name) != new_ins.end()) {
        wire->port_output = true;
        continue;
      }
      if (new_outs.find(wire_name) != new_outs.end()) {
        wire->port_input = true;
        continue;
      }
      if (interface_wires.find(wire_name) != interface_wires.end()) {
        continue;
      }
      if (inputs.find(wire_name) != inputs.end()) {
        continue;
      }
      if (outputs.find(wire_name) != outputs.end()) {
        continue;
      }
      del_interface_wires.insert(wire);
    }

    interface_mod->connections_.clear();
    for (auto wire : del_interface_wires) {
      interface_mod->remove({wire});
    }
    interface_mod->fixup_ports();

    for (auto cell : wrapper_mod->cells()) {
      string module_name = cell->type.str();
      remove_wrapper_cells.push_back(cell);
    }

    for (auto cell : remove_wrapper_cells) {
      wrapper_mod->remove(cell);
    }

    wrapper_mod->connections_.clear();

    // Add instances of the original and interface modules to the wrapper module
    Cell *orig_mod_inst = wrapper_mod->addCell(NEW_ID, original_mod->name);
    Cell *interface_mod_inst =
        wrapper_mod->addCell(NEW_ID, interface_mod->name);
    for (auto wire : original_mod->wires()) {
      RTLIL::SigSpec conn = wire;
      std::string wire_name = wire->name.str();
      if (wire->port_input || wire->port_output) {
        orig_inst_conns.insert(wire_name);
      }
    }

    for (auto wire : interface_mod->wires()) {
      RTLIL::SigSpec conn = wire;
      std::string wire_name = wire->name.str();
      if (wire->port_input || wire->port_output) {
        interface_inst_conns.insert(wire_name);
      }
    }

    for (auto wire : wrapper_mod->wires()) {
      RTLIL::SigSpec conn = wire;
      std::string wire_name = wire->name.str();
      if (orig_inst_conns.find(wire_name) == orig_inst_conns.end() &&
          interface_inst_conns.find(wire_name) == interface_inst_conns.end() &&
          interface_wires.find(wire_name) == interface_wires.end()) {
        del_wrapper_wires.insert(wire);
      } else {
        if (orig_inst_conns.find(wire_name) != orig_inst_conns.end()) {
          orig_mod_inst->setPort(wire_name, conn);
        }
        if (interface_inst_conns.find(wire_name) !=
            interface_inst_conns.end()) {
          interface_mod_inst->setPort(wire_name, conn);
        }
      }
    }

    for (auto wire : del_wrapper_wires) {
      wrapper_mod->remove({wire});
    }

    wrapper_mod->fixup_ports();

    new_design->add(interface_mod->clone());
    new_design->add(wrapper_mod->clone());

    run_script(new_design);
  }

  void script() override {
    std::cout << "Run Script" << std::endl;
    for (auto file : wrapper_files) {
      std::string extension = get_extension(file);
      if (!extension.empty()) {
        if (extension == ".v") {
          run("write_verilog -noexpr -simple-lhs " + file);
          continue;
        }
        if (extension == ".eblif") {
          run("write_blif -param " + file);
          continue;
        }
      }
    }
  }
} DesignEditRapidSilicon;

PRIVATE_NAMESPACE_END