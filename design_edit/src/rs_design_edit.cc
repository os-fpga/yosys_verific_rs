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

const std::vector<std::string> IN_PORTS = {"I", "I_P", "I_N", "D"};
const std::vector<std::string> DATA_OUT_PORTS = {"O", "O_P", "O_N", "Q"};
const std::vector<std::string> DATA_CLK_OUT_PORTS = {"O", "O_P", "O_N", "Q", "CLK_OUT", "CLK_OUT_DIV2", "CLK_OUT_DIV3", "CLK_OUT_DIV4", "OUTPUT_CLK"};

struct DesignEditRapidSilicon : public ScriptPass {
  DesignEditRapidSilicon()
      : ScriptPass("design_edit", "Netlist Editing Tool") {}
  
  ~DesignEditRapidSilicon() {
    while (pins.size()) {
      delete pins.back();
      pins.pop_back();
    }
  }

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

  struct instance_connection {
    std::string inst;
    Yosys::RTLIL::SigSpec sigspec;

    instance_connection(const std::string& inst, const Yosys::RTLIL::SigSpec& sigspec)
        : inst(inst), sigspec(sigspec) {}
  };
  std::vector<Cell *> remove_prims;
  std::vector<Cell *> remove_non_prims;
  std::vector<Cell *> remove_wrapper_cells;
  std::unordered_set<Wire *> wires_interface;
  std::unordered_set<Wire *> del_ins;
  std::unordered_set<Wire *> del_outs;
  std::unordered_set<Wire *> del_interface_wires;
  std::unordered_set<Wire *> del_wrapper_wires;
  std::unordered_set<Wire *> del_unused;
  std::set<std::pair<Yosys::RTLIL::SigSpec, Yosys::RTLIL::SigSpec>> connections_to_remove;
  std::unordered_set<Wire *> orig_intermediate_wires;
  std::unordered_set<Wire *> interface_intermediate_wires;
  std::map<RTLIL::SigBit, std::vector<RTLIL::Wire *>> io_prim_conn;
  pool<SigBit> prim_out_bits;
  pool<SigBit> unused_prim_outs;
  pool<SigBit> used_bits;
  pool<SigBit> orig_ins, orig_outs, fab_outs, ofab_outs, ifab_ins;
  pool<SigBit> i_buf_ins, i_buf_outs, o_buf_outs, i_buf_ctrls, o_buf_ctrls;
  pool<SigBit> clk_buf_ins;
  pool<SigBit> fclk_buf_ins;
  pool<SigBit> diff;
  std::map<std::string, std::map<std::string, std::vector<instance_connection>>> shared_ports_map;

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
  
  pin_data* get_pin(std::string name, bool create_new_if_not_exist = true) {
    pin_data* pin = nullptr;
    for (auto& p : pins) {
      if (p->_name == name) {
        pin = p;
        break;
      }
    }
    if (pin == nullptr && create_new_if_not_exist) {
      pin = new pin_data(name);
      pins.push_back(pin);
    }
    return pin;
  }
  void processSdcFile(std::istream &input) {
    std::string line;
    while (std::getline(input, line)) {
      std::vector<std::string> tokens = tokenizeString(line);
      if (!tokens.size())
        continue;
      if ("set_property" == tokens[0]) {
        if (tokens.size() == 4) {
          pin_data* pin = get_pin(tokens[3]);
          log_assert(pin != nullptr);
          pin->_properties[tokens[1]] = tokens[2];
        }
      } else if ("set_pin_loc" == tokens[0]) {
        if (tokens.size() < 3 || tokens.size() > 4) continue;
        pin_data* pin = get_pin(tokens[1]);
        log_assert(pin != nullptr);
        pin->_location = tokens[2];
        if (tokens.size() == 4) {
          pin->_internal_pin = tokens[3];
        }
      }
    }
  }

  bool contains_shared_ports(const std::string& mod)
  {
    return mod_shared_ports.count(mod) > 0;
  }

  void write_checker_file()
  {
    std::ofstream netlist_checker_file("netlist_checker.log");
    if (netlist_checker_file.is_open())
    {
      netlist_checker_file << netlist_checker.str();
      netlist_checker_file.close();
    }

    netlist_checker.str("");
    netlist_checker.clear();
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
    json instances = json::object();
    json instances_array = json::array();
    for(auto cell : mod->cells()) {
      json instance_object;
      instance_object["module"] = remove_backslashes(cell->type.str());
      instance_object["name"] = remove_backslashes(cell->name.str());
      for(auto conn : cell->connections()) {
        std::string port_name = remove_backslashes(conn.first.str());
        std::vector<std::string> signals;
        PRIMITIVES_EXTRACTOR::get_signals(conn.second, signals);
        std::string connection = "";
        for (size_t i = 0; i < signals.size(); i++) {
          signals[i] = remove_backslashes(signals[i]);
        }
        if (signals.size() == 0) {
          instance_object["connectivity"][port_name] = "";
        } else if (signals.size() == 1) {
          instance_object["connectivity"][port_name] = signals[0];
          connection = signals[0];
        } else {
          // array of signals
          instance_object["connectivity"][port_name] = nlohmann::json::array();
          for (auto& s : signals) {
            instance_object["connectivity"][port_name].push_back(s);
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
          link_instance(dir == "IN", instances_array, portname, portname, dir, 0, false, DATA_OUT_PORTS);
        }
      }
    }
#endif
	// Handle pure-data
    link_instance_recursively(instances_array, 0, DATA_OUT_PORTS);
    // Handle clock
    for (std::string module : std::vector<std::string>({"BOOT_CLOCK", "FCLK_BUF"})) {
      i = 0;
      for (auto& inst : instances_array) {
        if (inst["module"] == module) {
          if (inst["connectivity"].contains("O")) {
            inst["linked_object"] = inst["connectivity"]["O"];
          } else {
            inst["linked_object"] = stringf("%s#%ld", module.c_str(), i);
          }
          inst["direction"] = "IN";
          inst["index"] = 0;
          i++;
        }
      }
    }
	// Hnadle clock-data
    link_instance_recursively(instances_array, 1, DATA_CLK_OUT_PORTS);

    instances["instances"] = instances_array;
    if (json_file.is_open()) {
      json_file << std::setw(4) << instances << std::endl;
      json_file.close();
    }
  }
  
  void link_instance_recursively(json& instances_array, int retry_start_index, const std::vector<std::string>& OUT_PORTS) {
    log_assert(retry_start_index == 0 || retry_start_index == 1);
    // Special case for I_BUF_DS and O_BUF_DS, O_BUFT_DS, because they have multiple objects
    // We need to loop this recursive loop twice
    for (int i = retry_start_index; i < 2; i++) {
      // first time : only link I_BUF_DS and O_BUF_DS, O_BUFT_DS (before they are used to link other instance)
      //              because the name needs to be "p+n"
      // second time: link the rest
      while (true) {
        // Recursively marks other primitives
        size_t linked = 0;
        for (auto& inst : instances_array) {
          if (inst.contains("linked_object")) {
            for (auto& iter : inst["connectivity"].items()) {
              bool src_is_in = std::find(IN_PORTS.begin(), IN_PORTS.end(), (std::string)(iter.key())) != IN_PORTS.end();
              bool src_is_out = std::find(OUT_PORTS.begin(), OUT_PORTS.end(), (std::string)(iter.key())) !=  OUT_PORTS.end();
              if (src_is_in || src_is_out) {
                log_assert((src_is_in & src_is_out) == false);
                nlohmann::json signals = iter.value();
                if (signals.is_string()) {
                  signals = nlohmann::json::array();
                  signals.push_back((std::string)(iter.value()));
                } else {
                  log_assert(signals.is_array());
                }
                for (auto& s : signals) {
                  std::string net = (std::string)(s);
                  if (!PRIMITIVES_EXTRACTOR::is_real_net(net)) {
                    continue;
                  }
                  if (i == 0) {
                    linked += link_instance(!src_is_in, instances_array, inst["linked_object"], net, 
                                            inst["direction"], uint32_t(inst["index"]) + 1, true, OUT_PORTS,
                                            {"I_BUF_DS", "O_BUF_DS", "O_BUFT_DS"});
                  } else {
                    // dont set allow_dual_name=true, it might become infinite loop
                    linked += link_instance(!src_is_in, instances_array, inst["linked_object"], net, 
                                            inst["direction"], uint32_t(inst["index"]) + 1, false, OUT_PORTS);
                  }
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
  }
  
  size_t link_instance(bool use_in_port, json& instances_array, const std::string& object, 
                        const std::string& net, const std::string& direction, uint32_t index, bool allow_dual_name, 
                        const std::vector<std::string>& OUT_PORTS, std::vector<std::string> search_modules = {}) {
    size_t linked = 0;
    for (auto& inst : instances_array) {
      // Only if this instance had not been linked
      if (search_modules.size() > 0 &&
          std::find(search_modules.begin(), search_modules.end(), inst["module"]) == search_modules.end()) {
        continue;
      }
      if (!inst.contains("linked_object") || allow_dual_name) {
        for (auto& iter : inst["connectivity"].items()) {
          if (!iter.value().is_string()) {
            continue;
          }
          std::string inst_net = (std::string)(iter.value());
          if (!PRIMITIVES_EXTRACTOR::is_real_net(inst_net)) {
            continue;
          }
          bool match = false;
          if (use_in_port && 
              (std::find(IN_PORTS.begin(), IN_PORTS.end(), (std::string)(iter.key())) != IN_PORTS.end() || 
               (inst["module"] == "PLL" && (std::string)(iter.key()) == "CLK_IN"))) {
            match = true;
          }
          if (!use_in_port && 
              (std::find(OUT_PORTS.begin(), OUT_PORTS.end(), (std::string)(iter.key())) !=  OUT_PORTS.end())) {
            match = true;
          }
          if (match) {
            if (inst_net == net) {
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

  void handle_dangling_outs(Module *module)
  {
    for(auto cell : module->cells())
    {
      for (auto &conn : cell->connections())
      {
        IdString portName = conn.first;
        if (cell->input(portName))
        {
          for (SigBit bit : conn.second)
          {
            if (bit.wire != nullptr) used_bits.insert(bit);
          }
        }
      }
    }
    
    while(true)
    {
      unsigned unused_assign = 0;
      pool<SigBit> assign_used_bits;
      for(auto &conn : module->connections())
      {
        for (SigBit bit : conn.second)
        {
          if (bit.wire != nullptr) assign_used_bits.insert(bit);
        }
      }

      for(auto &conn : module->connections())
      {
        std::vector<RTLIL::SigBit> conn_lhs = conn.first.to_sigbit_vector();
        std::vector<RTLIL::SigBit> unused_bits;
        for (SigBit bit : conn.first)
        {
          if (bit.wire != nullptr)
          {
            if(!used_bits.count(bit) && !assign_used_bits.count(bit) && !bit.wire->port_output)
            {
              unused_bits.push_back(bit);
              if(conn.first.is_chunk())
              {
                if(conn.first.as_chunk().width == conn.first.as_chunk().wire->width)
                  del_unused.insert(bit.wire);
              }
            }
          }
        }
        if(unused_bits.size())
        {
          unused_assign++;
          if(unused_bits.size() == conn_lhs.size())
            connections_to_remove.insert(conn);
          else
            std::cerr << "Unused bits in assignement" << std::endl;
        }
      }
      remove_extra_conns(module);
      connections_to_remove.clear();
      for (auto wire : del_unused) {
        module->remove({wire});
      }
      del_unused.clear();
      if (!unused_assign) break;
    }

    for(auto &conn : module->connections())
    {
      for (SigBit bit : conn.second)
      {
        if (bit.wire != nullptr) used_bits.insert(bit);
      }
    }

    for (auto cell : module->cells()){
      string module_name = remove_backslashes(cell->type.str());
      if (std::find(primitives.begin(), primitives.end(), module_name) !=
          primitives.end()) {
        //EDA-3010: output primitives cal also have danlging output wire 
        //bool is_out_prim = (module_name.substr(0, 2) == "O_") ? true : false;
        //if (is_out_prim) continue;
        // Upgrading dangling outs of input primtives to output ports
        for (auto port : cell->connections()){
          IdString portName = port.first;
          for (SigBit bit : port.second){
            if(!used_bits.count(bit) && cell->output(portName)
              && !bit.wire->port_output){
              RTLIL::SigSig new_conn;
              RTLIL::Wire *new_wire = module->addWire(NEW_ID, 1);
              new_wire->port_output = true;
              new_conn.first = new_wire;
              new_conn.second = bit;
              module->connect(new_conn);
            }
          }
        }
      // Upgrading dangling outs of fabric primitives like TDPRAM is causing issues for AES_DECRYPT_partitioner
      //} else {
      //  for (auto port : cell->connections()){
      //    IdString portName = port.first;
      //    for (SigBit bit : port.second){
      //      if(!used_bits.count(bit) && cell->output(portName)
      //       && !bit.wire->port_output){
      //        bit.wire->port_output = true;
      //      }
      //    }
      //  }
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
      bool is_out_prim = (module_name.substr(0, 2) == "O_") ? true : false;
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
                if (cell->input(portName) &&
                  portName.str() != "\\CLK_IN" &&
                  portName.str() != "\\C" && is_out_prim) {
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
                  new_outs.insert(new_wire->name.str());
                  sigspec.append(new_wire);
                } else if (cell->output(portName)) {
                  new_ins.insert(bit.wire->name.str());
                  keep_wires.insert(bit.wire->name.str());
                  sigspec.append(bit.wire);
                }
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

  void set_difference(const pool<SigBit>& set1,
    const pool<SigBit>& set2)
  {
    for (auto &bit : set1)
    {
      if (!set2.count(bit))
      {
        diff.insert(bit);
      }
    }
  }

  void get_fabric_outputs(Module* mod)
  {
    for (auto wire : mod->wires())
    {
      bool is_output = wire->port_output ? true :false;
      if (!is_output) continue;

      RTLIL::SigSpec wire_ = wire;
      for (auto bit : wire_)
      {
        if(!orig_outs.count(bit)) fab_outs.insert(bit);
      }
    }
  }

  void check_buf_cntrls()
  {
    netlist_checker << "\nChecking Buffer control signals\n";
    netlist_checker << "================================================================\n";
    for (auto &bit : i_buf_ctrls)
    {
      if (!ofab_outs.count(bit))
      {
        netlist_checker << log_signal(bit) << " is an input control signal and must be connected to O_FAB\n";
        netlist_error = true;
      }
    }

    for (auto &bit : o_buf_ctrls)
    {
      if (!ofab_outs.count(bit))
      {
        netlist_checker << log_signal(bit) << " is an input control signal and must be connected to O_FAB\n";
        netlist_error = true;
      }
    }
    netlist_checker << "================================================================\n";
  }

  void check_fclkbuf_conns()
  {
    netlist_checker << "\nChecking FCLK_BUF connections\n";
    netlist_checker << "================================================================\n";
    set_difference(fclk_buf_ins, fab_outs);
    if(!diff.empty())
    {
      netlist_checker << "The following fclk_buf_outputs are not fabric outputs\n";
      for (const auto &elem : diff)
      {
        netlist_checker << "FCLK_BUF_IN : " << log_signal(elem) << "\n";
      }
      netlist_error = true;
      diff.clear();
    }
    netlist_checker << "================================================================\n";
  }

  void check_clkbuf_conns()
  {
    set_difference(clk_buf_ins, i_buf_outs);
    if(!diff.empty())
    {
      netlist_checker << "================================================================\n";
      netlist_checker << "The following CLK_BUF inputs are not connected to I_BUF outputs\n";
      for (const auto &elem : diff)
      {
        netlist_checker << "CLK_BUF Input : " << log_signal(elem) << "\n";
      }
      netlist_checker << "================================================================\n";
      netlist_error = true;
    }

    diff.clear();
  }

  void check_buf_conns()
  {
    netlist_checker << "Checking Buffer connections\n";
    if (orig_ins == i_buf_ins && orig_outs == o_buf_outs)
    {
      netlist_checker << "All IO connections are correct.\n";
      return;
    }

    set_difference(orig_ins, i_buf_ins);
    if(!diff.empty())
    {
      netlist_checker << "================================================================\n";
      netlist_checker << "The following inputs are not connected to I_BUFs\n";
      for (const auto &elem : diff)
      {
        netlist_checker << "Input : " << log_signal(elem) << "\n";
      }
      netlist_checker << "================================================================\n";
      netlist_error = true;
    }

    diff.clear();
    set_difference(i_buf_ins, orig_ins);
    if(!diff.empty())
    {
      netlist_checker << "================================================================\n";
      netlist_checker << "The following I_BUF inputs are not connected to the design inputs\n";
      for (const auto &elem : diff)
      {
        netlist_checker << "I_BUF Input : " << log_signal(elem) << "\n";
      }
      netlist_checker << "================================================================\n";
      netlist_error = true;
    }

    diff.clear();
    set_difference(orig_outs, o_buf_outs);
    if(!diff.empty())
    {
      netlist_checker << "================================================================\n";
      netlist_checker << "The following outputs are not connected to O_BUFs\n";
      for (const auto &elem : diff)
      {
        netlist_checker << "Output : " << log_signal(elem) << "\n";
      }
      netlist_checker << "================================================================\n";
      netlist_error = true;
    }

    diff.clear();
    set_difference(o_buf_outs, orig_outs);
    if(!diff.empty())
    {
      netlist_checker << "================================================================\n";
      netlist_checker << "The following O_BUF outputs are not connected to the design outputs\n";
      for (const auto &elem : diff)
      {
        netlist_checker << "O_BUF Output : " << log_signal(elem) << "\n";
      }
      netlist_checker << "================================================================\n";
      netlist_error = true;
    }

    diff.clear();
    return;
  }

  std::string extract_half_bank(const string& str)
  {
    size_t last_underscore = str.rfind('_');
    if (last_underscore == std::string::npos)
      log_error("Invalid pin location");
    string pin_index = str.substr(last_underscore + 1, str.length() - last_underscore);
    pin_index = (pin_index.back() == 'P' || pin_index.back() == 'N') ?
                  pin_index.substr(0, pin_index.size() - 1) : pin_index;
    string bank_pin = str.substr(0, last_underscore);
    last_underscore = bank_pin.rfind('_');
    if (last_underscore == std::string::npos)
      log_error("Invalid pin location");
    string bank_name = bank_pin.substr(0, last_underscore);
    int half_bank = stoi(pin_index);
    return bank_name + (half_bank < 10 ? "_firsthalf" : "_secondhalf");
  }

  bool is_identical_sig(const std::vector<Yosys::RTLIL::SigSpec>& signals)
  {
    const Yosys::RTLIL::SigSpec first = signals[0];
    for (size_t i = 1; i < signals.size(); ++i)
    {
      if (signals[i] != first) 
      {
        return false;
      }
    }
    return true;
  }

  void handle_shared_ports()
  {
    for (const auto& p : shared_ports_map)
    {
      for (const auto& bank : p.second)
      {
        std::vector<Yosys::RTLIL::SigSpec> signals;
        std::unordered_set<string> insts;
        for (const auto& connection : bank.second)
        {
          insts.insert(connection.inst);
          signals.push_back(connection.sigspec);
        }
        if(signals.empty()) continue;
        if(is_identical_sig(signals))
        {
          std::cout << " cons are same in : " << bank.first << std::endl;
        }
      }
    }
  }

  static bool sigName(const RTLIL::SigSpec &sig, std::string &name)
  {
    if (!sig.is_chunk())
    {
      return false;
    }

    const RTLIL::SigChunk chunk = sig.as_chunk();

    if (chunk.wire == NULL)
    {
      return false;
    }

    if (chunk.width == chunk.wire->width && chunk.offset == 0)
    {
      name = (chunk.wire->name).substr(0);
    }
    else
    {
      name = "";
    }

    return true;
  }

  static int checkCell(Cell *cell, const string cellName,
                const string &port, string &actual_name)
  {
    if (cell->type != RTLIL::escape_id(cellName))
    {
      return 0;
    }

    std::string name;
    for (auto &conn : cell->connections())
    {

      IdString portName = conn.first;
      RTLIL::SigSpec actual = conn.second;

      if (portName == RTLIL::escape_id(port))
      {
        if (sigName(actual, name))
        {
          actual_name = name;
          if (actual_name[0] == '\\') {
            actual_name = actual_name.substr(1);
          }
          return 1;
        }
      }
    }
    return 1;
  }

  bool is_flag(const std::string &arg) { return !arg.empty() && arg[0] == '-'; }

  std::string get_extension(const std::string &filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
      return filename.substr(dot_pos);
    }
    return ""; // If no extension found
  }

 static void reportInfoFabricClocks(Module *original_mod) {
  std::ofstream fabric_clocks("fabric_netlist_info.json");
    json ports = json::object();
    json ports_array = json::array();
    std::set<std::string> reported;
    for (auto cell : original_mod->cells())
    {
      string module_name = cell->type.str();
      string actual_clock;
      if (checkCell(cell, "DFFRE",
                    "C", actual_clock))
      {
        if (reported.find(actual_clock) == reported.end())
        {
          json port_object;
          port_object["name"] = actual_clock;
          port_object["direction"] = (std::string)("input");
          port_object["clock"] = (std::string)("active_high");
          ports_array.push_back(port_object);
          reported.insert(actual_clock);
        }
        continue;
      }

      if (checkCell(cell, "DFFNRE",
                    "C", actual_clock))
      {
        if (reported.find(actual_clock) == reported.end())
        {
          json port_object;
          port_object["name"] = actual_clock;
          port_object["direction"] = (std::string)("input");
          port_object["clock"] = (std::string)("active_low");
          ports_array.push_back(port_object);
          reported.insert(actual_clock);
        }
        continue;
      }
      if (checkCell(cell, "DSP38",
                    "CLK", actual_clock) || checkCell(cell, "DSP19x2",
                    "CLK", actual_clock))
      {
        if (reported.find(actual_clock) == reported.end())
        {
          json port_object;
          port_object["name"] = actual_clock;
          port_object["direction"] = (std::string)("input");
          port_object["clock"] = (std::string)("active_high");
          ports_array.push_back(port_object);
          reported.insert(actual_clock);
        }
        continue;
      }
      for (auto formal_clock : {"CLK_A", "CLK_B"})
      {
        if (checkCell(cell, "TDP_RAM36K",
                      formal_clock, actual_clock))
        {
          if (reported.find(actual_clock) == reported.end())
          {
            json port_object;
            port_object["name"] = actual_clock;
            port_object["direction"] = (std::string)("input");
            port_object["clock"] = (std::string)("active_high");
            ports_array.push_back(port_object);
            reported.insert(actual_clock);
          }
        }
      }
      for (auto formal_clock : {"CLK_A1", "CLK_B1", "CLK_A2", "CLK_B2"})
      {
        if (checkCell(cell, "TDP_RAM18KX2",
                      formal_clock, actual_clock))
        {
          if (reported.find(actual_clock) == reported.end())
          {
            json port_object;
            port_object["name"] = actual_clock;
            port_object["direction"] = (std::string)("input");
            port_object["clock"] = (std::string)("active_high");
            ports_array.push_back(port_object);
            reported.insert(actual_clock);
          }
        }
      }
    }
    ports["ports"] = ports_array;
    if (fabric_clocks.is_open()) {
      fabric_clocks << std::setw(4) << ports << std::endl;
      fabric_clocks.close();
    }
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
    bool supported_tech = io_prim.supported_tech;

    // Extract the primitive information (before anything is modified)
    PRIMITIVES_EXTRACTOR* extractor = new PRIMITIVES_EXTRACTOR(tech);
    extractor->extract(_design);
    
    if (sdc_passed) {
      std::ifstream input_sdc(sdc_file);
      if (!input_sdc.is_open()) {
        std::cerr << "Error opening input sdc file: " << sdc_file << std::endl;
      }
      processSdcFile(input_sdc);
      for (auto &p : pins) {
        extractor->assign_location(p->_name, p->_location, p->_properties, p->_internal_pin);
      }
    }

    Pass::call(_design, "splitnets");
    Module *original_mod = _design->top_module();
    std::string original_mod_name =
      remove_backslashes(_design->top_module()->name.str());
    design->rename(original_mod, "\\fabric_" + original_mod_name);

    for (auto wire : original_mod->wires())
    {
      bool is_input = wire->port_input ? true :false;
      bool is_output = wire->port_output ? true :false;
      if (!is_input && !is_output) continue;

      RTLIL::SigSpec wire_ = wire;
      for (auto bit : wire_)
      {
        if (is_input) orig_ins.insert(bit);
        if (is_output) orig_outs.insert(bit);
      }
    }

    if (supported_tech)
    {
      for (auto cell : original_mod->cells()) {
        string module_name = remove_backslashes(cell->type.str());
        if (std::find(primitives.begin(), primitives.end(), module_name) !=
            primitives.end()) {
          io_prim.contains_io_prem = true;
          bool is_out_prim = (module_name.substr(0, 2) == "O_") ? true : false;
          bool has_shared_ports = contains_shared_ports(module_name);
          std::string loc;
          remove_prims.push_back(cell);

          if (cell->type == RTLIL::escape_id("I_BUF") ||
            cell->type == RTLIL::escape_id("I_BUF_DS"))
          {
            for (auto conn : cell->connections())
            {
              IdString portName = conn.first;
              for (SigBit bit : conn.second)
              {
                if (bit.wire != nullptr)
                {
                  if (cell->input(portName) )
                    (remove_backslashes(portName.str()) != "EN") ? i_buf_ins.insert(bit) : i_buf_ctrls.insert(bit);
                  if (cell->output(portName)) i_buf_outs.insert(bit);
                }
              }
            }
          } else if (cell->type == RTLIL::escape_id("O_BUF") ||
            cell->type == RTLIL::escape_id("O_BUF_DS"))
          {
            for (auto conn : cell->connections())
            {
              IdString portName = conn.first;
              for (SigBit bit : conn.second)
              {
                if (bit.wire != nullptr)
                {
                  if(cell->output(portName)) o_buf_outs.insert(bit);
                }
              }
            }
          } else if (cell->type == RTLIL::escape_id("O_BUFT") ||
            cell->type == RTLIL::escape_id("O_BUFT_DS"))
          {
            for (auto conn : cell->connections())
            {
              IdString portName = conn.first;
              for (SigBit bit : conn.second)
              {
                if (bit.wire != nullptr)
                {
                  if(cell->output(portName)) o_buf_outs.insert(bit);
                  if (remove_backslashes(portName.str()) == "T") o_buf_ctrls.insert(bit);
                }
              }
            }
          } else if (cell->type == RTLIL::escape_id("CLK_BUF"))
          {
            for (auto conn : cell->connections())
            {
              IdString portName = conn.first;
              if(cell->input(portName))
              {
                for (SigBit bit : conn.second)
                {
                  if (bit.wire != nullptr)
                  {
                    clk_buf_ins.insert(bit);
                  }
                }
              }
            }
          } else if (cell->type == RTLIL::escape_id("FCLK_BUF"))
          {
            for (auto conn : cell->connections())
            {
              IdString portName = conn.first;
              if(cell->input(portName))
              {
                for (SigBit bit : conn.second)
                {
                  if (bit.wire != nullptr)
                  {
                    fclk_buf_ins.insert(bit);
                  }
                }
              }
            }
          }
          
          if (has_shared_ports)
          {
            std::map<std::string, std::vector<Yosys::RTLIL::SigSpec>> port_location;
            
            std::vector<std::string> locs = extractor->get_primitive_locations_by_name(remove_backslashes(cell->name.str()));
            if (!locs.empty())
            {
              loc = locs[0];
              std::cout << "LOCATION :: " << loc << std::endl;
            }

            for (auto conn : cell->connections())
            {
              IdString portName = conn.first;
              RTLIL::SigSpec actual = conn.second;
              std::string pName = remove_backslashes(portName.str());

              if(shared_ports.count(pName))
              {
                if(!loc.empty())
                {
                  std::string half_bank_name = extract_half_bank(loc);
                  shared_ports_map[pName][half_bank_name].push_back(instance_connection(cell->name.str(), actual));
                }
              } else {
                if (actual.is_chunk()) {
                  RTLIL::Wire *wire = actual.as_chunk().wire;
                  if (wire != NULL) {
                    process_wire(cell, portName, wire);
                    if (is_out_prim) {
                      if (cell->input(portName)) {
                        if (portName.str() != "\\CLK_IN" &&
                          portName.str() != "\\C")
                          out_prim_ins.insert(wire->name.str());
                      }
                    } else {
                      if (cell->output(portName)) {
                        in_prim_outs.insert(wire->name.str());
                        for (auto bit : conn.second){
                          prim_out_bits.insert(bit);
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
                          if (portName.str() != "\\CLK_IN" &&
                            portName.str() != "\\C")
                            out_prim_ins.insert(wire->name.str());
                        }
                      } else {
                        if (cell->output(portName)) {
                          in_prim_outs.insert(wire->name.str());
                          for (auto bit : conn.second){
                            prim_out_bits.insert(bit);
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          else{
            for (auto conn : cell->connections()) {
              IdString portName = conn.first;
              RTLIL::SigSpec actual = conn.second;

              if (actual.is_chunk()) {
                RTLIL::Wire *wire = actual.as_chunk().wire;
                if (wire != NULL) {
                  process_wire(cell, portName, wire);
                  if (is_out_prim) {
                    if (cell->input(portName)) {
                      if (portName.str() != "\\CLK_IN" &&
                        portName.str() != "\\C")
                        out_prim_ins.insert(wire->name.str());
                    }
                  } else {
                    if (cell->output(portName)) {
                      in_prim_outs.insert(wire->name.str());
                      for (auto bit : conn.second){
                        prim_out_bits.insert(bit);
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
                        if (portName.str() != "\\CLK_IN" &&
                          portName.str() != "\\C")
                          out_prim_ins.insert(wire->name.str());
                      }
                    } else {
                      if (cell->output(portName)) {
                        in_prim_outs.insert(wire->name.str());
                        for (auto bit : conn.second){
                          prim_out_bits.insert(bit);
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        } else {
          if (cell->type == RTLIL::escape_id("I_FAB"))
          {
            for (auto conn : cell->connections())
            {
              IdString portName = conn.first;
              if(remove_backslashes(portName.str()) == "I")
              {
                for (SigBit bit : conn.second)
                {
                  if (bit.wire != nullptr) ifab_ins.insert(bit);
                }
              }
            }
          }
          if (cell->type == RTLIL::escape_id("O_FAB"))
          {
            for (auto conn : cell->connections())
            {
              IdString portName = conn.first;
              if(remove_backslashes(portName.str()) == "O")
              {
                for (SigBit bit : conn.second)
                {
                  if (bit.wire != nullptr) ofab_outs.insert(bit);
                }
              }
            }
          }
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

      check_buf_conns();
      check_clkbuf_conns();
      check_buf_cntrls();
      handle_shared_ports();
      add_wire_btw_prims(original_mod);
      intersection_copy_remove(new_ins, new_outs, interface_wires);
      intersect(interface_wires, keep_wires);
    }
    
    Module *interface_mod = _design->top_module()->clone();
    std::string interface_mod_name = "\\interface_" + original_mod_name;
    interface_mod->name = interface_mod_name;
    Module *wrapper_mod = original_mod->clone();
    std::string wrapper_mod_name = "\\" + original_mod_name;
    wrapper_mod->name = wrapper_mod_name;

    if (supported_tech)
    {
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
      connections_to_remove.clear();
      update_prim_connections(original_mod, primitives, orig_intermediate_wires);
      handle_dangling_outs(original_mod);
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

      get_fabric_outputs(original_mod);
      check_fclkbuf_conns();
      delete_wires(original_mod, wires_interface);
      delete_wires(original_mod, del_ins);
      delete_wires(original_mod, del_outs);

      for (const auto& prim_conn : io_prim_conn) {
        const std::vector<RTLIL::Wire *>& connected_wires = prim_conn.second;
        if(connected_wires.size() < 1) continue;
        RTLIL::SigSpec in_prim_out;
        pool<RTLIL::SigSpec> out_prim_in;
        for(const auto conn_wire : connected_wires) {
          std::string wire_name = conn_wire->name.str();
          out_prim_in.insert(conn_wire);
        }
        for(const auto& prim_in : out_prim_in)
        {
          RTLIL::SigSig new_conn;
          new_conn.first = prim_in;
          new_conn.second = prim_conn.first;
          original_mod->connect(new_conn);
        }
      }

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
      connections_to_remove.clear();
      for (auto wire : del_interface_wires) {
        interface_mod->remove({wire});
      }

      delete_wires(original_mod, orig_intermediate_wires);
      fixup_mod_ports(original_mod);
      Pass::call(_design, "clean");

      reportInfoFabricClocks(original_mod);

      delete_wires(interface_mod, interface_intermediate_wires);
      interface_mod->fixup_ports();
    }

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

    if (supported_tech)
    {
      Cell *interface_mod_inst =
        wrapper_mod->addCell(NEW_ID, interface_mod->name);
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
          if (supported_tech)
          {
            if (interface_inst_conns.find(wire_name) !=
              interface_inst_conns.end()) {
            interface_mod_inst->setPort(wire_name, conn);
            }
          }
        }
      }
    } else {
      for (auto wire : wrapper_mod->wires()) {
        RTLIL::SigSpec conn = wire;
        std::string wire_name = wire->name.str();
        if (orig_inst_conns.find(wire_name) == orig_inst_conns.end()) {
          del_wrapper_wires.insert(wire);
        } else {
          orig_mod_inst->setPort(wire_name, conn);
        }
      }
    }

    for (auto wire : del_wrapper_wires) {
      wrapper_mod->remove({wire});
    }

    wrapper_mod->fixup_ports();

    new_design->add(wrapper_mod);
    if (supported_tech)
    {
      new_design->add(interface_mod);
    }
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
    if (supported_tech)
    {
      write_checker_file();
      // Dump entire wrap design using "config.json" naming (by default)
      dump_io_config_json(wrapper_mod, io_config_json);
      std::ifstream input(io_config_json.c_str());
      log_assert(input.is_open() && input.good());
      nlohmann::json instances = nlohmann::json::parse(input);
      input.close();
      log_assert(instances.is_object());
      log_assert(instances.contains("instances"));
      extractor->write_sdc("design_edit.sdc", instances["instances"]);
      std::string io_file = "io_" + io_config_json;
      extractor->write_json(io_file);
      if (io_file.size() > 5 &&
          io_file.rfind(".json") == (io_file.size() - 5)) {
        std::string simple_file =
            io_file.substr(0, io_file.size() - 5) + ".simple.json";
        extractor->write_json(simple_file, true);
      } else {
        extractor->write_json("io_config.simple.json", true);
      }
      delete extractor;
      if(netlist_error)
        log_error("Netlist is illegal, check netlist_checker.log for more details.\n");
    }
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
