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
#include "primitives_extractor.h"
#include "rs_design_edit.h"
#include "rs_primitive.h"
#include <json.hpp>

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

using json = nlohmann::json;

USING_YOSYS_NAMESPACE
using namespace RTLIL;

const std::vector<std::string> CONNECTING_PORTS = {"I", "I_P", "I_N", "O", "O_P", "O_N", "D", "Q"};

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

  std::vector<Cell *> remove_prims;
  std::vector<Cell *> remove_non_prims;
  std::vector<Cell *> remove_wrapper_cells;
  std::unordered_set<Wire *> wires_interface;
  std::unordered_set<Wire *> del_ins;
  std::unordered_set<Wire *> del_outs;
  std::unordered_set<Wire *> del_interface_wires;
  std::unordered_set<Wire *> del_wrapper_wires;
  std::map<std::pair<Yosys::RTLIL::SigSpec, Yosys::RTLIL::SigSpec>,
    std::pair<Yosys::RTLIL::SigSpec, Yosys::RTLIL::SigSpec>> conns_to_update;
  std::set<std::pair<Yosys::RTLIL::SigSpec, Yosys::RTLIL::SigSpec>> connections_to_remove;
  std::set<std::pair<Yosys::RTLIL::SigSpec, Yosys::RTLIL::SigSpec>> additional_connections;
  std::unordered_set<Wire *> orig_intermediate_wires;
  std::unordered_set<Wire *> interface_intermediate_wires;
  std::map<RTLIL::SigBit, std::vector<RTLIL::Wire *>> io_prim_conn;
  pool<SigBit> prim_out_bits;
  pool<SigBit> unused_prim_outs;
  pool<SigBit> in_bits;

  RTLIL::Design *_design;
  RTLIL::Design *new_design = new RTLIL::Design;
  primitives_data io_prim;

  void clear_flags() override { wrapper_files = {}; }

  std::vector<std::string> tokenizeString(const std::string &input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;

    while (iss >> token) {
      tokens.push_back(token);
    }

    return tokens;
  }

  void processSdcFile(std::istream &input) {
    std::string line;
    while (std::getline(input, line)) {
      std::vector<std::string> tokens = tokenizeString(line);
      if (!tokens.size())
        continue;
      if ("set_property" == tokens[0]) {
        if (tokens.size() == 4) {
          location_map[tokens[3]]._properties[tokens[1]] = tokens[2];
          location_map[tokens[3]]._name = tokens[3];
        }
      } else if ("set_pin_loc" == tokens[0]) {
        if (tokens.size() < 3 || tokens.size() > 4) continue;
        constrained_pins.insert(tokens[1]);
        location_map[tokens[2]]._associated_pin = tokens[1];
        location_map[tokens[2]]._name = tokens[2];
        if (tokens.size() == 4) {
          location_map[tokens[2]]._internal_pin = tokens[3];
        }
      }
    }
  }

  void get_loc_map_by_io() {
    for (auto &p : location_map) {
      location_map_by_io[p.second._associated_pin] = p.second;
    }
  }

  std::string id(RTLIL::IdString internal_id)
  {
    const char *str = internal_id.c_str();
    return std::string(str);
  }

  std::string process_connection(const RTLIL::SigChunk &chunk) {
    std::stringstream output;
    if (chunk.width == chunk.wire->width && chunk.offset == 0) {
      output << id(chunk.wire->name);
    } else if (chunk.width == 1) {
      if (chunk.wire->upto)
        output << id(chunk.wire->name) << "[" << (chunk.wire->width - chunk.offset - 1) + chunk.wire->start_offset << "]";
      else
        output << id(chunk.wire->name) << "[" << chunk.offset + chunk.wire->start_offset << "]";
    } else {
      if (chunk.wire->upto)
        output << id(chunk.wire->name) << "[" << (chunk.wire->width - (chunk.offset + chunk.width - 1) - 1) + chunk.wire->start_offset << ":" <<
                  (chunk.wire->width - chunk.offset - 1) + chunk.wire->start_offset << "]";
      else
        output << id(chunk.wire->name) << "[" << (chunk.offset + chunk.width - 1) + chunk.wire->start_offset << ":" <<
                  chunk.offset + chunk.wire->start_offset << "]";
    }
    return output.str();
  }

  void dump_io_config_json(Module* mod, std::string file) {
    std::ofstream json_file(file.c_str());
		json instances;
    instances["instances"] = json::object();
    json instances_array = json::array();
    for(auto cell : mod->cells()) {
      json instance_object;
      instance_object["module"] = remove_backslashes(cell->type.str());
      instance_object["name"] = remove_backslashes(cell->name.str());
      for(auto conn : cell->connections()) {
        IdString port_name = conn.first;
        RTLIL::SigSpec actual = conn.second;
        std::string connection;
        json port_obj;
        if (actual.is_chunk()) {
          if (actual.as_chunk().wire != NULL)
          connection = process_connection(actual.as_chunk());
        } else {
          for (auto it = actual.chunks().rbegin(); 
                it != actual.chunks().rend(); ++it) {
            RTLIL::Wire* wire = (*it).wire;
            if(wire != NULL)
            {
              connection = process_connection(*it);
              break;
            }
          }
        }
        connection = remove_backslashes(connection);
        instance_object["connectivity"][remove_backslashes(port_name.str())] = connection;
        if (location_map_by_io.find(connection) != location_map_by_io.end()) {
          instance_object["location"] = location_map_by_io[connection]._name;
          for (auto &pr : location_map_by_io[connection]._properties) {
            if (!pr.second.empty()) {
              instance_object["properties"][pr.first] = pr.second;
            }
          }
        }
      }
      instances_array.push_back(instance_object);
    }
    // enhancement to auto create wire primitives
    size_t i = 0;
    for (auto it : mod->connections()) {
      std::vector<std::string> lefts;
      std::vector<std::string> rights;
      PRIMITIVES_EXTRACTOR::get_signals(it.first, lefts);
      PRIMITIVES_EXTRACTOR::get_signals(it.second, rights);
      log_assert(lefts.size() == rights.size());
      // break the bus into bit by bit
      for (size_t j = 0; j < lefts.size(); j++) {
        json instance_object;
        instance_object["module"] = (std::string)("WIRE");
        instance_object["name"] = (std::string)(stringf("wire%ld", i));
        instance_object["connectivity"]["I"] = remove_backslashes(rights[j]);
        instance_object["connectivity"]["O"] = remove_backslashes(lefts[j]);
        instances_array.push_back(instance_object);
        i++;
      }
    }
#if 0
    // Starting by marking all the "port" primitives
    // IO bitstream generation will only need a unique name to know which primitives are linked together, any name will do
    // But it does not work for other flow, which they use linked_object name as port name
    i = 0;
    std::vector<std::string> port_primitives = {"I_BUF", "I_BUF_DS", "O_BUF", "O_BUFT", "O_BUF_DS", "O_BUFT_DS", "BOOT_CLOCK"};
    for (auto& inst : instances_array) {
      if (std::find(port_primitives.begin(), port_primitives.end(), inst["module"]) != port_primitives.end()) {
        inst["linked_object"] = std::string(stringf("object%ld", i));
        i++;
      }
    }
#else
    // Use the port name to link the instance
    for (const RTLIL::Wire* wire : mod->wires()) {
      // We can use one line code: !wire->port_input && !wire->port_output
      // But prefer list of all the valid possible of Input, Output, Inout
      std::string dir = "";
      if (wire->port_input && !wire->port_output) {
        dir = "IN";
      } else if (!wire->port_input && wire->port_output) {
        dir = "OUT";
      } else if (wire->port_input && wire->port_output) {
        dir = "INOUT";
      }
      if (dir.size()) {
        for (int index = 0; index < wire->width; index++) {
          std::string portname = wire->name.str();
          if (wire->width > 1) {
            portname = stringf("%s[%d]", wire->name.c_str(), wire->start_offset + index);
          }
          portname = remove_backslashes(portname);
          link_instance(instances_array, portname, portname, dir, 0, false);
        }
      }
    }
#endif
    // Special case for I_BUF_DS and O_BUF_DS, O_BUFT_DS, because they have multiple objects
    // We need to loop this recursive loop twice
    for (i = 0; i < 2; i++) {
      // first time : only link I_BUF_DS and O_BUF_DS, O_BUFT_DS (before they are used to link other instance)
      //              because the name needs to be "p+n"
      // second time: link the rest
      while (true) {
        // Recursively marks other primitives
        size_t linked = 0;
        for (auto& inst : instances_array) {
          if (inst.contains("linked_object")) {
            for (auto& iter : inst["connectivity"].items()) {
              if (std::find(CONNECTING_PORTS.begin(), CONNECTING_PORTS.end(), (std::string)(iter.key())) != 
                  CONNECTING_PORTS.end()) {
                if (i == 0) {
                  linked += link_instance(instances_array, inst["linked_object"], (std::string)(iter.value()), 
                                          inst["direction"], uint32_t(inst["index"]) + 1, true, 
                                          {"I_BUF_DS", "O_BUF_DS", "O_BUFT_DS"});
                } else {
                  // dont set allow_dual_name=true, it might become infinite loop
                  linked += link_instance(instances_array, inst["linked_object"], (std::string)(iter.value()), 
                                          inst["direction"], uint32_t(inst["index"]) + 1, false);
                }
              }
            }
          }
        }
        if (i == 0 || linked == 0) {
          // 1st time: we do not need recursive loop. One time is enough
          // 2nd time: we need recursive until we cannot link anymore
          break;
        }
      }
    }
    instances["instances"] = instances_array;
    if (json_file.is_open()) {
      json_file << std::setw(4) << instances << std::endl;
      json_file.close();
    }
  }
  
  size_t link_instance(json& instances_array, const std::string& object, const std::string& net, const std::string& direction, 
                        uint32_t index, bool allow_dual_name, std::vector<std::string> search_modules = {}) {
    size_t linked = 0;
    for (auto& inst : instances_array) {
      // Only if this instance had not been linked
      if (search_modules.size() > 0 &&
          std::find(search_modules.begin(), search_modules.end(), inst["module"]) == search_modules.end()) {
        continue;
      }
      if (!inst.contains("linked_object") || allow_dual_name) {
        for (auto& iter : inst["connectivity"].items()) {
          if (std::find(CONNECTING_PORTS.begin(), CONNECTING_PORTS.end(), (std::string)(iter.key())) != 
              CONNECTING_PORTS.end() || 
              (inst["module"] == "PLL" && (std::string)(iter.key()) == "CLK_IN")) {
            if ((std::string)(iter.value()) == net) {
              if (inst.contains("linked_object")) {
                inst["linked_object"] = stringf("%s+%s", ((std::string)(inst["linked_object"])).c_str(), object.c_str());
              } else {
                inst["linked_object"] = object;
              }
              inst["direction"] = direction;
              inst["index"] = index;
              linked++;
              break;
            }
          }
        }
      }
    }
    return linked;
  }

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

  void intersect(const std::unordered_set<std::string>& set1,
    const std::unordered_set<std::string>& set2)
  {
    for (const auto& element : set1)
    {
      if (set2.find(element) != set2.end())
      {
        common_clks_resets.insert(element);
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

  void add_wire_btw_prims(Module* mod)
  {
    for (const auto& element : out_prim_ins)
    {
      if (in_prim_outs.find(element) != in_prim_outs.end())
      {
        io_prim_wires.insert(element);
      }
    }
    for (const auto& element : io_prim_wires)
    {
      if (new_ins.find(element) != new_ins.end())
      {
        io_prim_wires.insert(element);
      }
    }
    for (const auto& element : io_prim_wires)
    {
      if (new_outs.find(element) != new_outs.end())
      {
        io_prim_wires.insert(element);
      }
    }
    for (const string& element : io_prim_wires) {
      new_ins.erase(element);
      new_outs.erase(element);
    }

    for (auto cell : mod->cells()) {
      string module_name = remove_backslashes(cell->type.str());
      if (std::find(primitives.begin(), primitives.end(), module_name) !=
          primitives.end()) {
        for (auto conn : cell->connections()) {
          IdString portName = conn.first;
          bool unset_port = true;
          RTLIL::SigSpec sigspec;
          for (SigBit bit : conn.second)
          {
            if (bit.wire != nullptr)
            {
              if (std::find(io_prim_wires.begin(), io_prim_wires.end(), bit.wire->name.str()) !=
                  io_prim_wires.end()) {
                if (unset_port)
                {
                  cell->unsetPort(portName);
                  unset_port = false;
                }
                RTLIL::Wire *new_wire = mod->addWire(NEW_ID, 1);
                auto it = io_prim_conn.find(bit);

                if (it != io_prim_conn.end()) {
                  it->second.push_back(new_wire);
                } else {
                  std::vector<RTLIL::Wire *> new_wires;
                  new_wires.push_back(new_wire);
                  io_prim_conn.insert({bit, new_wires});
                }
                if (cell->input(portName)) {
                    new_outs.insert(new_wire->name.str());
                } else if (cell->output(portName)) {
                    new_ins.insert(new_wire->name.str());
                }
                sigspec.append(new_wire);

              }
              else {
                sigspec.append(bit);
              }
            }
            else {
              sigspec.append(bit);
            }
          }
          if (!unset_port)
          {
            cell->setPort(portName, sigspec);
          }
        }
      }
    }

    for (auto cell : mod->cells()) {
      string module_name = remove_backslashes(cell->type.str());
      if (std::find(primitives.begin(), primitives.end(), module_name) ==
          primitives.end()) {
        for (auto conn : cell->connections()) {
          IdString portName = conn.first;
          RTLIL::SigSpec actual = conn.second;
          if (actual.is_chunk()) {
            RTLIL::Wire *wire = actual.as_chunk().wire;
            if (wire != NULL) {
              if (std::find(io_prim_wires.begin(), io_prim_wires.end(), wire->name.str()) !=
                  io_prim_wires.end()) {
                auto it = io_prim_conn.find(wire);
                if (it != io_prim_conn.end()) {
                  const std::vector<RTLIL::Wire *>& connected_wires = it->second;
                  if (cell->input(portName)) {
                    RTLIL::SigSig new_conn;
                    for(const auto conn_wire : connected_wires) {
                      std::string wire_name = conn_wire->name.str();
                      if (new_ins.find(wire_name) != new_ins.end()) {
                        new_conn.second = conn_wire;
                        new_conn.first = wire;
                        additional_connections.insert(new_conn);
                      }
                    }
                  }
                  if (cell->output(portName)) {
                    std::cerr << "Error: Multiple drivers for " << wire->name.str() << std::endl ;
                  }
                }
              }
            }
          } else {
            for (auto iter = actual.chunks().rbegin();
                 iter != actual.chunks().rend(); ++iter) {
              RTLIL::Wire *wire = (*iter).wire;
              if (wire != NULL) {
                if (std::find(io_prim_wires.begin(), io_prim_wires.end(), wire->name.str()) !=
                  io_prim_wires.end()) {
                  auto it = io_prim_conn.find(wire);
                  if (it != io_prim_conn.end()) {
                    const std::vector<RTLIL::Wire *>& connected_wires = it->second;
                    if (cell->input(portName)) {
                      RTLIL::SigSig new_conn;
                      for(const auto conn_wire : connected_wires) {
                        std::string wire_name = conn_wire->name.str();
                        if (new_ins.find(wire_name) != new_ins.end()) {
                          new_conn.second = conn_wire;
                          new_conn.first = wire;
                          additional_connections.insert(new_conn);
                        }
                      }
                    }
                    if (cell->output(portName)) {
                      std::cerr << "Error: Multiple drivers for " << wire->name.str() << std::endl;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    for (auto &conn : mod->connections()) {
      std::vector<RTLIL::SigBit> conn_lhs = conn.first.to_sigbit_vector();
      std::vector<RTLIL::SigBit> conn_rhs = conn.second.to_sigbit_vector();
      RTLIL::SigSpec rhsSigSpec;
      RTLIL::SigSpec lhsSigSpec;
      for (size_t i = 0; i < conn_lhs.size(); i++) {
        if (conn_lhs[i].wire != nullptr) {
          auto it = io_prim_conn.find(conn_lhs[i]);
          if (it != io_prim_conn.end())
          {
            std::cerr << "Error: Multiple drivers for " << conn_lhs[i].wire->name.str() << std::endl;
          }
        }
        if (conn_rhs[i].wire != nullptr) {
          auto it = io_prim_conn.find(conn_rhs[i]);
          if (it != io_prim_conn.end())
          {
            const std::vector<RTLIL::Wire *>& wires = it->second;
            for(const auto conn_wire : wires)
            {
              std::string wire_name = conn_wire->name.str();
              if (new_ins.find(wire_name) != new_ins.end())
              {
                conn_rhs[i] = conn_wire;
              }
            }
          }
        }
        rhsSigSpec.append(conn_rhs[i]);
        lhsSigSpec.append(conn_lhs[i]);
      }
      if (conn_rhs != conn.second.to_sigbit_vector())
      {
        std::pair<Yosys::RTLIL::SigSpec, Yosys::RTLIL::SigSpec> new_conn;
        new_conn.first = lhsSigSpec;
        new_conn.second = rhsSigSpec;
        auto it = conns_to_update.find(conn);
        if (it == conns_to_update.end())
        {
          conns_to_update.insert(std::make_pair(conn, new_conn));
        }
      }
    }

    for (auto it = conns_to_update.begin(); it != conns_to_update.end(); ++it)
    {
      const auto& conn = it->first;
      const auto& new_conn = it->second;
      mod->connections_.erase(std::remove_if(mod->connections_.begin(),
        mod->connections_.end(),
        [&](const std::pair<Yosys::RTLIL::SigSpec, Yosys::RTLIL::SigSpec>& p) {
            return p == conn;
        }), mod->connections_.end());
        mod->connect(new_conn);
    }

    for (const auto& conn : additional_connections) {
      mod->connect(conn);
    }

    for (const auto& [key, value] : io_prim_conn) {
      const std::vector<RTLIL::Wire *>& connected_wires = value;
      if(connected_wires.size() != 2) continue;
      RTLIL::SigSig new_conn;
      for(const auto conn_wire : connected_wires) {
        std::string wire_name = conn_wire->name.str();
        if (new_outs.find(wire_name) != new_outs.end()) {
          new_conn.first = conn_wire;
        } else if (new_ins.find(wire_name) != new_ins.end()) {
          new_conn.second = conn_wire;
        }
      }
      mod->connect(new_conn);
    }

  }

  void check_undriven_IO(Module *mod, std::unordered_set<std::string> &primitives){
    for (auto cell : mod->cells()){
      string module_name = remove_backslashes(cell->type.str());
      if (std::find(primitives.begin(), primitives.end(), module_name) !=
          primitives.end()) {
        bool is_out_prim = (module_name.substr(0, 2) == "O_") ? true : false;
        if (is_out_prim) continue;
        for (auto port : cell->connections()){
          IdString portName = port.first;
          for (SigBit bit : port.second){
            if(unused_prim_outs.count(bit) && cell->output(portName)){
              RTLIL::SigSig new_conn;
              RTLIL::Wire *new_wire = mod->addWire(NEW_ID, 1);
              new_wire->port_output = true;
              new_conn.first = new_wire;
              new_conn.second = bit;
              mod->connect(new_conn);
            }
          }
        }
      }
    } 
  }

  static bool fixup_ports_compare_(const RTLIL::Wire *a, const RTLIL::Wire *b)
  {
  	size_t pos_a = a->name.str().find("[");
    size_t pos_b = b->name.str().find("[");
    std::string prefix_a = pos_a == std::string::npos ? a->name.str() : a->name.str().substr(0, pos_a);
    std::string prefix_b = pos_b == std::string::npos ? b->name.str() : b->name.str().substr(0, pos_b);

    if (prefix_a != prefix_b)
    {
      return prefix_a < prefix_b;
    }

    if (pos_a == std::string::npos || pos_b == std::string::npos)
    {
      return false;
    }

    std::string a_index_str = a->name.str().substr(pos_a + 1, a->name.str().find("]"));
    std::string b_index_str = b->name.str().substr(pos_b + 1, b->name.str().find("]"));
    return std::stoi(a_index_str) < std::stoi(b_index_str);
  }

  void fixup_mod_ports (Module* mod)
  {
    std::vector<RTLIL::Wire*> all_ports;

    for (auto w : mod->wires())
    {
      if (w->port_input || w->port_output)
        all_ports.push_back(w);
      else
        w->port_id = 0;
    }

    std::sort(all_ports.begin(), all_ports.end(), fixup_ports_compare_);
    mod->ports.clear();
    for (size_t i = 0; i < all_ports.size(); i++) 
    {
      mod->ports.push_back(all_ports[i]->name);
      all_ports[i]->port_id = i+1;
    }
  }

  void remove_extra_conns(Module* mod)
  {
    for (const auto& conn : connections_to_remove) {
    mod->connections_.erase(std::remove_if(mod->connections_.begin(),
      mod->connections_.end(),
      [&](const std::pair<Yosys::RTLIL::SigSpec, Yosys::RTLIL::SigSpec>& p) {
          return p == conn;
      }), mod->connections_.end());
    }
  }

  bool is_clk_out(Module *mod, Wire* rhs_wire, std::unordered_set<std::string> &prims)
  {
    bool is_clk_output = false;
    for (auto cell : mod->cells()) {
      string module_name = remove_backslashes(cell->type.str());
      if (std::find(prims.begin(), prims.end(), module_name) !=
          prims.end()) {
        for (auto conn : cell->connections()) {
          IdString portName = conn.first;
          RTLIL::SigSpec actual = conn.second;
          if (actual.is_chunk()) {
            const RTLIL::SigChunk chunk = actual.as_chunk();
            if(chunk.wire == NULL) continue;
            if(chunk.wire->name.str() == rhs_wire->name.str() &&
              (module_name.substr(0, 4) == "CLK_"))
            {
              is_clk_output = true;
            }
          }
        }
      }
    }
    return is_clk_output;
  }

  void update_prim_connections(Module* mod, std::unordered_set<std::string> &prims, std::unordered_set<Wire *> &del_intermediate_wires)
  {
    for (auto cell : mod->cells()) {
      string module_name = remove_backslashes(cell->type.str());
      if (std::find(prims.begin(), prims.end(), module_name) !=
          prims.end()) {
        for (auto conn : cell->connections()) {
          IdString portName = conn.first;
          RTLIL::SigSpec actual = conn.second;
          if (actual.is_chunk()) {
            const RTLIL::SigChunk chunk = actual.as_chunk();
            RTLIL::Wire *wire = actual.as_chunk().wire;
            if(chunk.wire == NULL) continue;
            for (const auto& connection : connections_to_remove)
            {
              const Yosys::RTLIL::SigSpec lhs = connection.first;
              const Yosys::RTLIL::SigSpec rhs = connection.second;
              const RTLIL::SigChunk lhs_chunk = lhs.as_chunk();
              const RTLIL::SigChunk rhs_chunk = rhs.as_chunk();
              if ((chunk.width == chunk.wire->width && chunk.offset == 0) &&
                (lhs_chunk.width == lhs_chunk.wire->width && lhs_chunk.offset == 0) &&
                (lhs_chunk.wire->name.str() == chunk.wire->name.str()))
              {
                cell->unsetPort(portName);
                cell->setPort(portName, rhs);
                del_intermediate_wires.insert(wire);
              } else if ((chunk.width == 1) &&
                (lhs_chunk.wire->name.str() == chunk.wire->name.str()))
              {
                if (lhs_chunk.width == 1)
                {
                  cell->unsetPort(portName);
                  cell->setPort(portName, rhs);
                  del_intermediate_wires.insert(wire);
                } else if (lhs_chunk.width == lhs_chunk.wire->width && lhs_chunk.offset == 0) {
                  unsigned offset = chunk.offset + chunk.wire->start_offset ;
                  auto conn_rhs = connection.second.to_sigbit_vector();
                  cell->unsetPort(portName);
                  cell->setPort(portName, conn_rhs.at(offset));
                  del_intermediate_wires.insert(wire);
                }
              }
            }
          }
        }
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
      if (args[argidx] == "-pr" && argidx + 1 < args.size()) {
        size_t next_argidx = argidx + 1;
        while (next_argidx < args.size() && !is_flag(args[next_argidx])) {
          post_route_wrapper.push_back(args[next_argidx]);
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
      if (args[argidx] == "-json" && argidx + 1 < args.size())
      {
        io_config_json = args[++argidx];
        continue;
      }
      if (args[argidx] == "-sdc" && argidx + 1 < args.size())
      {
        sdc_file = args[++argidx];
        sdc_passed = true;
        continue;
      }
      break;
    }
    primitives = io_prim.get_primitives(tech);

    // Extract the primitive information (before anything is modified)
    PRIMITIVES_EXTRACTOR extractor(tech);
    extractor.extract(_design);

    Pass::call(_design, "splitnets");
    Module *original_mod = _design->top_module();
    std::string original_mod_name =
      remove_backslashes(_design->top_module()->name.str());
    if (original_mod_name.find("fabric_") == std::string::npos) {
      design->rename(original_mod, "\\fabric_" + original_mod_name);   
    }

    for (auto cell : original_mod->cells()) {
      string module_name = remove_backslashes(cell->type.str());
      if (std::find(primitives.begin(), primitives.end(), module_name) !=
          primitives.end()) {
        io_prim.contains_io_prem = true;
        bool is_out_prim = (module_name.substr(0, 2) == "O_") ? true : false;
        remove_prims.push_back(cell);
        for (auto conn : cell->connections()) {
          IdString portName = conn.first;
          RTLIL::SigSpec actual = conn.second;
          if (actual.is_chunk()) {
            RTLIL::Wire *wire = actual.as_chunk().wire;
            if (wire != NULL) {
              process_wire(cell, portName, wire);
              if (is_out_prim) {
                if (cell->input(portName)) {
                  out_prim_ins.insert(wire->name.str());
                  for (auto bit : conn.second){
                    in_bits.insert(bit);
                  }
                }
              } else {
                if (cell->output(portName)) {
                  in_prim_outs.insert(wire->name.str());
                  for (auto bit : conn.second){
                    prim_out_bits.insert(bit);
                  }
                } else if (cell->input(portName)) {
                  for (auto bit : conn.second){
                    in_bits.insert(bit);
                  }
                }
              }
            } else {
              RTLIL::SigSpec const_sig = actual;
              if (GetSize(const_sig) != 0)
              {
                RTLIL::SigSig new_conn;
                RTLIL::Wire *new_wire = original_mod->addWire(NEW_ID, GetSize(const_sig));
                cell->unsetPort(portName);
                cell->setPort(portName, new_wire);
                new_conn.first = new_wire;
                new_conn.second = const_sig;
                original_mod->connect(new_conn);
                process_wire(cell, portName, new_wire);
              }
            }
          } else {
            for (auto it = actual.chunks().rbegin();
                 it != actual.chunks().rend(); ++it) {
              RTLIL::Wire *wire = (*it).wire;
              if (wire != NULL) {
                process_wire(cell, portName, wire);
                if (is_out_prim) {
                  if (cell->input(portName)) {
                    out_prim_ins.insert(wire->name.str());
                    for (auto bit : conn.second){
                      in_bits.insert(bit);
                    }
                  }
                } else {
                  if (cell->output(portName)) {
                    in_prim_outs.insert(wire->name.str());
                    for (auto bit : conn.second){
                      prim_out_bits.insert(bit);
                    }
                  } else if (cell->input(portName)) {
                    for (auto bit : conn.second){
                      in_bits.insert(bit);
                    }
                  }
                }
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
              if (cell->input(portName)) {
                for (auto bit : conn.second){
                  in_bits.insert(bit);
                }
              }
            }
          } else {
            for (auto it = actual.chunks().rbegin();
                 it != actual.chunks().rend(); ++it) {
              RTLIL::Wire *wire = (*it).wire;
              if (wire != NULL) {
                keep_wires.insert(wire->name.str());
                if (cell->input(portName)) {
                  for (auto bit : conn.second){
                    in_bits.insert(bit);
                  }
                }
              }
            }
          }
        }
      }
    }

    for (auto bit : prim_out_bits) {
			if(!in_bits.count(bit)) {
        unused_prim_outs.insert(bit);
      }
    }

    add_wire_btw_prims(original_mod);
    intersection_copy_remove(new_ins, new_outs, interface_wires);
    intersect(interface_wires, keep_wires);
    
    Module *interface_mod = _design->top_module()->clone();
    std::string interface_mod_name = "\\interface_" + original_mod_name;
    interface_mod->name = interface_mod_name;
    Module *wrapper_mod = original_mod->clone();
    std::string wrapper_mod_name = "\\" + original_mod_name;
    wrapper_mod->name = wrapper_mod_name;

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
      if (common_clks_resets.find(wire_name) != common_clks_resets.end())
      {
        wire->port_input = true;
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
      RTLIL::SigSpec lhs = conn.first;
      RTLIL::SigSpec rhs = conn.second;
      if(lhs.is_chunk() && rhs.is_chunk())
      {
        const RTLIL::SigChunk lhs_chunk = lhs.as_chunk();
        const RTLIL::SigChunk rhs_chunk = rhs.as_chunk();
        if((lhs_chunk.wire != nullptr) && (rhs_chunk.wire != nullptr))
        {
          if((lhs_chunk.wire->port_input || lhs_chunk.wire->port_output) &&
            (rhs_chunk.wire->port_input || rhs_chunk.wire->port_output) &&
            (outputs.find(lhs_chunk.wire->name.str()) == outputs.end()))
          {
            if(is_clk_out(original_mod, rhs_chunk.wire, primitives) &&
              inputs.find(rhs_chunk.wire->name.str()) == inputs.end())
            {
              lhs_chunk.wire->port_input = false;
              lhs_chunk.wire->port_output = false;
              rhs_chunk.wire->port_input = false;
              rhs_chunk.wire->port_output = false;
              connections_to_remove.insert(conn);
            }
          }
        }
      }
    }

    remove_extra_conns(original_mod);
    update_prim_connections(original_mod, primitives, orig_intermediate_wires);
    check_undriven_IO(original_mod, primitives);
    delete_cells(original_mod, remove_prims);

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
    connections_to_remove.clear();

    fixup_mod_ports(original_mod);

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
      if (common_clks_resets.find(wire_name) != common_clks_resets.end())
      {
        wire->port_output = true;
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

    for (auto &conn : interface_mod->connections()) {
      RTLIL::SigSpec lhs = conn.first;
      RTLIL::SigSpec rhs = conn.second;
      if(lhs.is_chunk() && rhs.is_chunk())
      {
        const RTLIL::SigChunk lhs_chunk = lhs.as_chunk();
        const RTLIL::SigChunk rhs_chunk = rhs.as_chunk();
        if((lhs_chunk.wire != nullptr) && (rhs_chunk.wire != nullptr))
        {
          if((lhs_chunk.wire->port_input || lhs_chunk.wire->port_output) &&
            (rhs_chunk.wire->port_input || rhs_chunk.wire->port_output) &&
            (outputs.find(lhs_chunk.wire->name.str()) == outputs.end()))
          {
            if(is_clk_out(interface_mod, lhs_chunk.wire, primitives) &&
              inputs.find(rhs_chunk.wire->name.str()) == inputs.end())
            {
              lhs_chunk.wire->port_input = false;
              lhs_chunk.wire->port_output = false;
              rhs_chunk.wire->port_input = false;
              rhs_chunk.wire->port_output = false;
              connections_to_remove.insert(conn);
            }
          }
        }
      }
    }

    update_prim_connections(interface_mod, primitives, interface_intermediate_wires);

    interface_mod->connections_.clear();
    for (auto wire : del_interface_wires) {
      interface_mod->remove({wire});
    }
    
    delete_wires(original_mod, orig_intermediate_wires);
    fixup_mod_ports(original_mod);
    delete_wires(interface_mod, interface_intermediate_wires);
    interface_mod->fixup_ports();
    if(sdc_passed) {
      std::ifstream input_sdc(sdc_file);
      if (!input_sdc.is_open()) {
        std::cerr << "Error opening input sdc file: " << sdc_file << std::endl;
      }
      processSdcFile(input_sdc);
      get_loc_map_by_io();
      for (auto &p : location_map_by_io) {
        extractor.assign_location(p.second._associated_pin, p.second._name, p.second._properties);
      }
    }

    std::string io_file = "io_" + io_config_json;
    extractor.write_json(io_file);
    if (io_file.size() > 5 &&
        io_file.rfind(".json") == (io_file.size() - 5)) {
      std::string simple_file =
          io_file.substr(0, io_file.size() - 5) + ".simple.json";
      extractor.write_json(simple_file, true);
    } else {
      extractor.write_json("io_config.simple.json", true);
    }
    extractor.write_sdc("design_edit.sdc");

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

    new_design->add(wrapper_mod);
    new_design->add(interface_mod);
    Pass::call(new_design, "flatten");

    for (auto file : wrapper_files) {
      std::string extension = get_extension(file);
      if (!extension.empty()) {
        if (extension == ".v") {
          Pass::call(new_design, "write_verilog -noexpr -norename " + file);
          continue;
        }
        if (extension == ".eblif") {
          Pass::call(new_design, "write_blif -param " + file);
          continue;
        }
      }
    }

    for(auto cell : wrapper_mod->cells())
    {
      if(cell->type.str() == orig_mod_inst->type.str())
      {
        for(const auto& conn : cell->connections())
        {
          RTLIL::SigSpec actual = conn.second;
          IdString portName = conn.first;
          if (actual.is_chunk())
          {
            const RTLIL::SigChunk chunk = actual.as_chunk();
            RTLIL::Wire *wire = actual.as_chunk().wire;
            if(chunk.wire == NULL) continue;
            if(wire->width > 1)
            {
              cell->unsetPort(portName);
              RTLIL::SigSpec conn = wire;
              int width = wire->width;
              for(int i=0; i<width; i++)
              {
                IdString nportName = std::string(portName.c_str()) + "[" + std::to_string(i) + "]";
                unsigned index = (wire->upto) ? (width - 1 - i) : i;
                cell->setPort(nportName, conn[index]);
              }
            }
          }
        }
      }
    }
    run_script(new_design);
    // Dump entire wrap design using "config.json" naming (by default)
    dump_io_config_json(wrapper_mod, io_config_json);
  }

  void script() override {
    std::cout << "Run Script" << std::endl;
    for (auto file : post_route_wrapper) {
      std::string extension = get_extension(file);
      if (!extension.empty()) {
        if (extension == ".v") {
          run("write_verilog -noexpr -norename " + file);
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
