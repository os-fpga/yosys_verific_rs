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

#define GEN_JSON_METHOD 1

using json = nlohmann::json;


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

  std::vector<Cell *> remove_prims;
  std::vector<Cell *> remove_non_prims;
  std::vector<Cell *> remove_wrapper_cells;
  std::unordered_set<Wire *> wires_interface;
  std::unordered_set<Wire *> del_ins;
  std::unordered_set<Wire *> del_outs;
  std::unordered_set<Wire *> del_interface_wires;
  std::unordered_set<Wire *> del_wrapper_wires;
  std::set<std::pair<Yosys::RTLIL::SigSpec, Yosys::RTLIL::SigSpec>> connections_to_remove;
  std::unordered_set<Wire *> orig_intermediate_wires;
  std::unordered_set<Wire *> interface_intermediate_wires;
  std::map<RTLIL::SigSpec, std::vector<RTLIL::Wire *>> io_prim_conn;

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
        if (tokens.size() == 3) {
          constrained_pins.insert(tokens[1]);
          location_map[tokens[2]]._associated_pin = tokens[1];
          location_map[tokens[2]]._name = tokens[2];
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

        if (actual.is_chunk())
        {
          if (actual.as_chunk().wire != NULL)
          connection = process_connection(actual.as_chunk());
        } else {
	for (auto it = actual.chunks().rbegin(); 
          it != actual.chunks().rend(); ++it)
	{
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
    instances["instances"] = instances_array;

    if (json_file.is_open()) {
      json_file << std::setw(4) << instances << std::endl;
      json_file.close();
    }
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
    set_intersection(out_prim_ins.begin(), out_prim_ins.end(),
                    in_prim_outs.begin(), in_prim_outs.end(),
                    inserter(io_prim_wires, io_prim_wires.begin()));
    set_intersection(io_prim_wires.begin(), io_prim_wires.end(),
                    new_ins.begin(), new_ins.end(),
                    inserter(io_prim_wires, io_prim_wires.begin()));
    set_intersection(io_prim_wires.begin(), io_prim_wires.end(),
                    new_outs.begin(), new_outs.end(),
                    inserter(io_prim_wires, io_prim_wires.begin()));
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
          RTLIL::SigSpec actual = conn.second;
          if (actual.is_chunk()) {
            RTLIL::Wire *wire = actual.as_chunk().wire;
            if (wire != NULL) {
              if (std::find(io_prim_wires.begin(), io_prim_wires.end(), wire->name.str()) !=
                  io_prim_wires.end()) {
                RTLIL::Wire *new_wire = mod->addWire(NEW_ID, GetSize(actual));
                cell->unsetPort(portName);
                cell->setPort(portName, new_wire);
                auto it = io_prim_conn.find(actual);
                if (it != io_prim_conn.end()) {
                  it->second.push_back(new_wire);
                } else {
                  std::vector<RTLIL::Wire *> new_wires;
                  new_wires.push_back(new_wire);
                  io_prim_conn.insert({actual, new_wires});
                }
                if (cell->input(portName)) {
                    new_outs.insert(new_wire->name.str());
                } else if (cell->output(portName)) {
                    new_ins.insert(new_wire->name.str());
                }
              }
            }
          }
        }
      }
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

  void check_undriven_IO(Module *mod){
        pool<SigBit> driven_bits_list;
        RTLIL::Cell *remove_I_BUF = nullptr;
        //Collected all driven bits
        for (auto cell : mod->cells()){
            if (cell->type == RTLIL::escape_id("I_BUF"))
                continue;
			for (auto port : cell->connections())
				for (auto bit : port.second){
					driven_bits_list.insert(bit);
				}
		}
        //Remove undriven I_BUF having output port which is nt driving any cell
        for (auto cell : mod->cells()){
            if (cell->type != RTLIL::escape_id("I_BUF"))
                continue;
            for (auto port : cell->connections()){
                IdString portName = port.first;
                if(!driven_bits_list.count(port.second) && (portName == RTLIL::escape_id("O")) ){
                    remove_I_BUF=cell;
				}
            }
		}
        SigBit data_sig_out= remove_I_BUF->getPort(ID::O).as_bit();

        RTLIL::SigSig new_conn;
        RTLIL::Wire *new_wire = mod->addWire(NEW_ID,GetSize(remove_I_BUF->getPort(ID::O)));
        new_wire->port_output = true;
        new_conn.first = new_wire;
        new_conn.second = data_sig_out;
        mod->connect(new_conn);
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

  bool is_fab_out(Module *mod, Wire* lhs_wire, std::unordered_set<std::string> &prims)
  {
    bool is_fab_output = false;
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
            if(chunk.wire->name.str() == lhs_wire->name.str() &&
              (module_name.substr(0, 2) == "O_"))
            {
              is_fab_output = true;
            }
          }
        }
      }
    }
    return is_fab_output;
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

    #if GEN_JSON_METHOD
    // Extract the primitive information (before anything is modified)
    PRIMITIVES_EXTRACTOR extractor(tech);
    extractor.extract(_design);
    #endif

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
                }
              } else {
                if (cell->output(portName)) {
                  in_prim_outs.insert(wire->name.str());
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
                  }
                } else {
                  if (cell->output(portName)) {
                    in_prim_outs.insert(wire->name.str());
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
            if(!is_fab_out(original_mod, lhs_chunk.wire, primitives) && 
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
    check_undriven_IO(original_mod);
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
            if(!is_fab_out(interface_mod, lhs_chunk.wire, primitives) &&
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
    original_mod->fixup_ports();
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
        #if GEN_JSON_METHOD
        extractor.assign_location(p.second._associated_pin, p.second._name, p.second._properties);
        #endif
      }
    }

    #if GEN_JSON_METHOD
    extractor.write_json(io_config_json);
    #else
    dump_io_config_json(interface_mod, io_config_json);
    #endif

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
