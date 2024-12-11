/* Rapid Silicon Copyright 2023
 */
/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2012  Claire Xenia Wolf <claire@yosyshq.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/*
  Author: Chai, Chung Shien
*/

#include "primitives_extractor.h"

#include <algorithm>
#include <regex>
#include <set>

#include "backends/rtlil/rtlil_backend.h"
#include "kernel/celltypes.h"
#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/sigtools.h"
#include "primitive.h"
#include "primitive_path_auto.h"

USING_YOSYS_NAMESPACE

#define MAX_CLOCK_SLOT (16)
#define MAX_FABRIC_CLOCK_SLOT (16)

/*
  Fast Clock:
    - Clock Capable Pin
    - PLL's FAST_CLK and CLK_OUT
  Core Clock:
    - Pin
    - PLL's CLK_OUT, CLK_OUT_DIV2, CLK_OUT_DIV3 and CLK_OUT_DIV4
    - BOOT_CLOCK
    - I_SERDES's CLK_OUT
*/

/*
  Tokenizer
*/
std::vector<std::string> tokenizeString(const std::string& input) {
  std::vector<std::string> tokens;
  std::istringstream iss(input);
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

/*
  Extractor constructor
*/
PRIMITIVES_EXTRACTOR::PRIMITIVES_EXTRACTOR(const std::string& technology)
    : m_technology(technology) {
  if (YAML_DB.find(m_technology) == YAML_DB.end() ||
      PATH_DB.find(m_technology) == PATH_DB.end()) {
    m_basic_status = false;
    POST_ERR_MSG(1, "Technology %s is not supported", m_technology.c_str());
  }
}

/*
  Extractor destructor
*/
PRIMITIVES_EXTRACTOR::~PRIMITIVES_EXTRACTOR() {
  while (m_msgs.size()) {
    delete m_msgs.back();
    m_msgs.pop_back();
  }
  if (m_fabric != nullptr) {
    delete m_fabric;
  }
  while (m_primitives.size()) {
    auto iter = m_primitives.begin();
    delete iter->second;
    m_primitives.erase(iter);
  }
}

/*
  Get the signals bit by bit
*/
void PRIMITIVES_EXTRACTOR::get_signals(const Yosys::RTLIL::SigSpec& sig,
                                       std::vector<std::string>& signals) {
  PRIMITIVE::get_signals(sig, signals);
}

/*
  Check if the net is a real one - not constant or unconnected
*/
bool PRIMITIVES_EXTRACTOR::is_real_net(const std::string& net) {
  if (net == "" || ((net.size() > 14) && (net.find("__const_bit_") == 0) &&
                    (net.rfind("__") == (net.size() - 2)))) {
    return false;
  }
  return true;
}

/*
  Entry point of EXTRACTOR to extract
*/
bool PRIMITIVES_EXTRACTOR::extract(
    Yosys::RTLIL::Module* module, const std::string& user_sdc,
    const std::string& config_json, const std::string& sdc,
    const std::string& clk_pin_xml,
    std::map<std::string, bool>& fabric_ports_dir,
    std::vector<std::string>& errors) {
  log_assert(m_netlist_status);
  log_assert(errors.size() == 0);
  // Store all the PRIMITIVE
  std::vector<PRIMITIVE*> parents;
  std::vector<const PRIMITIVE*> core_clocks;

  // Step 1: Make sure the technology is supported (check in constructor)
  if (!m_basic_status) {
    goto EXTRACT_END;
  }

  // Step 2: Find all the connection relationship
  if (!get_primitives(module, fabric_ports_dir)) {
    goto EXTRACT_END;
  }

  // Step 3: Validate netlist
  if (!validate_netlist()) {
    goto EXTRACT_END;
  }

  // Step 4: Build chain
  if (!build_chain(parents, core_clocks, user_sdc)) {
    goto EXTRACT_END;
  }

  // Step 5: Write SDC
  if (!write_sdc(parents, core_clocks, sdc, clk_pin_xml)) {
    goto EXTRACT_END;
  }

EXTRACT_END:

  // Step 6: Write JSON
  write_json(parents, config_json);

  for (auto error : m_errors) {
    errors.push_back(error);
  }

  return m_status;
}

/*
  Store the message
*/
void PRIMITIVES_EXTRACTOR::post_msg(uint32_t offset, bool is_error,
                                    const std::string& msg) {
  if (is_error) {
    m_status = false;
    m_errors.push_back(msg);
  }
  m_msgs.push_back(new MSG(offset, is_error ? "Error: " + msg : msg));
}

/*
  Remove the last message
*/
void PRIMITIVES_EXTRACTOR::remove_msg() {
  if (m_msgs.size()) {
    delete m_msgs.back();
    m_msgs.pop_back();
  }
}

/*
  Get the Modules (Entry)
*/
bool PRIMITIVES_EXTRACTOR::get_primitives(
    Yosys::RTLIL::Module* module,
    const std::map<std::string, bool>& fabric_ports_dir) {
  POST_MSG(1, "Get all primitives");
  log_assert(m_primitives.size() == 0);
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  std::map<std::string, DRIVE_SINK*> out_nets;
  std::map<std::string, std::vector<DRIVE_SINK*>> in_nets;
  if (get_primitives(module, fabric_ports_dir, out_nets, in_nets) &&
      assign_drive_sink(fabric_ports_dir, out_nets, in_nets) &&
      assign_iopad(module, out_nets, in_nets) && validate_primitive()) {
  }
  while (out_nets.size()) {
    auto iter = out_nets.begin();
    delete iter->second;
    out_nets.erase(iter);
  }
  while (in_nets.size()) {
    auto iter = in_nets.begin();
    while (iter->second.size()) {
      delete iter->second.back();
      iter->second.pop_back();
    }
    in_nets.erase(iter);
  }
  return m_basic_status;
}

/*
  Get the Modules
*/
bool PRIMITIVES_EXTRACTOR::get_primitives(
    Yosys::RTLIL::Module* module,
    const std::map<std::string, bool>& fabric_ports_dir,
    std::map<std::string, DRIVE_SINK*>& out_nets,
    std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets) {
  POST_MSG(2, "Create primitives");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  log_assert(m_fabric == nullptr);
  log_assert(m_primitives.size() == 0);
  if (module->connections().size()) {
    m_basic_status = false;
    POST_ERR_MSG(3, "Does not support wire connection but found:");
    for (auto iter : module->connections()) {
      std::vector<std::string> left_signals;
      std::vector<std::string> right_signals;
      get_signals(iter.first, left_signals);
      get_signals(iter.second, right_signals);
      log_assert(left_signals.size() == right_signals.size());
      for (size_t i = 0; i < left_signals.size(); i++) {
        POST_ERR_MSG(3, "%s --> %s\n", left_signals[i].c_str(),
                     right_signals[i].c_str());
      }
    }
  }
  if (m_basic_status) {
    for (auto cell : module->cells()) {
      PRIMITIVE* primitive =
          PRIMITIVE::create(m_technology, cell, fabric_ports_dir, out_nets,
                            in_nets, m_msgs, m_errors);
      if (primitive != nullptr) {
        log_assert(m_primitives.find(primitive->name) == m_primitives.end());
        if (primitive->yaml != nullptr) {
          m_primitives[primitive->name] = primitive;
        } else if (m_fabric == nullptr) {
          m_fabric = primitive;
        } else {
          m_basic_status = false;
          POST_ERR_MSG(3, "Detected more than one FABRIC primitive");
          break;
        }
      } else {
        m_basic_status = false;
        break;
      }
    }
  }
  return m_basic_status;
}

/*
  Get iopad_external_pin attribute
*/
bool PRIMITIVES_EXTRACTOR::get_yaml_port_iopad(
    const YAML* yaml, const std::string& port,
    const std::map<std::string, bool>& fabric_ports_dir, bool& is_input) {
  bool iopad = false;
  if (yaml != nullptr) {
    log_assert(yaml->ports.find(port) != yaml->ports.end());
    const PORT_BASIC& port_basic = yaml->ports.at(port);
    iopad = port_basic.is_attribute("iopad_external_pin");
    is_input = port_basic.is_input;
  } else {
    log_assert(fabric_ports_dir.find(port) != fabric_ports_dir.end());
    is_input = fabric_ports_dir.at(port);
  }
  return iopad;
}

/*
  Assign nets between primitives
*/
bool PRIMITIVES_EXTRACTOR::assign_drive_sink(
    const std::map<std::string, bool>& fabric_ports_dir,
    const std::map<std::string, DRIVE_SINK*>& out_nets,
    const std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets) {
  POST_MSG(2, "Assign connection between primitives");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  for (auto& dc : m_primitives) {
    PRIMITIVE* primitive = dc.second;
    log_assert(primitive->yaml != nullptr);
    for (auto& p : primitive->ports) {
      PORT* port = p.second;
      bool dc_is_input = false;
      bool dc_is_iopad = get_yaml_port_iopad(primitive->yaml, port->name,
                                             fabric_ports_dir, dc_is_input);
      if (dc_is_input) {
        log_assert(port->dir == DIRECTION::IN);
      } else {
        log_assert(port->dir == DIRECTION::OUT);
      }
      for (auto& net : port->nets) {
        if (net->port->dir == DIRECTION::IN) {
          log_assert(dc_is_input);
          if (out_nets.find(net->name) != out_nets.end()) {
            DRIVE_SINK* ds = out_nets.at(net->name);
            bool ds_is_input = false;
            bool ds_is_iopad = get_yaml_port_iopad(
                ds->primitive->yaml, ds->port, fabric_ports_dir, ds_is_input);
            log_assert(!ds_is_input);
            m_basic_status =
                assign_drive_sink(net, dc_is_iopad, ds, ds_is_iopad);
          }
        } else {
          log_assert(!dc_is_input);
          if (in_nets.find(net->name) != in_nets.end()) {
            for (auto ds : in_nets.at(net->name)) {
              bool ds_is_input = false;
              bool ds_is_iopad = get_yaml_port_iopad(
                  ds->primitive->yaml, ds->port, fabric_ports_dir, ds_is_input);
              log_assert(ds_is_input);
              m_basic_status =
                  assign_drive_sink(net, dc_is_iopad, ds, ds_is_iopad);
            }
          }
        }
      }
    }
  }
  return m_basic_status;
}

/*
  Do the final assigment of DriveSink
*/
bool PRIMITIVES_EXTRACTOR::assign_drive_sink(NET*& net, bool dc_is_iopad,
                                             const DRIVE_SINK* ds,
                                             bool ds_is_iopad) {
  bool status = false;
  if (dc_is_iopad && ds_is_iopad) {
    // This is INOUT
    status = true;
  } else if (dc_is_iopad) {
    status = false;
    POST_ERR_MSG(3, "%s is an IOPAD and should not connect to internal %s",
                 net->port->id().c_str(), get_port_id(ds).c_str());
  } else if (ds_is_iopad) {
    status = false;
    POST_ERR_MSG(3, "%s is an IOPAD and should not connect to internal %s",
                 get_port_id(ds).c_str(), net->port->id().c_str());
  } else {
    status = true;
    net->drive_sinks.push_back(new DRIVE_SINK(ds));
  }
  return status;
}

/*
  Assign IOPAD
*/
bool PRIMITIVES_EXTRACTOR::assign_iopad(
    Yosys::RTLIL::Module* module,
    const std::map<std::string, DRIVE_SINK*>& out_nets,
    const std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets) {
  POST_MSG(2, "Assign connection between primitives and IOPAD");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  for (const RTLIL::Wire* wire : module->wires()) {
    DIRECTION dir = DIRECTION::UNKNOWN;
    std::string direction = "";
    if (wire->port_input && !wire->port_output) {
      dir = DIRECTION::IN;
      direction = "INPUT";
    } else if (!wire->port_input && wire->port_output) {
      dir = DIRECTION::OUT;
      direction = "OUTPUT";
    } else if (wire->port_input && wire->port_output) {
      dir = DIRECTION::INOUT;
      direction = "INOUT";
    }
    if (direction.size() == 0) {
      continue;
    }
    for (int index = 0; index < wire->width; index++) {
      std::string name = PRIMITIVE::remove_first_backslash(wire->name.str());
      std::string fullname = name;
      std::string realname = name;
      if (wire->width > 1) {
        fullname = stringf("%s[%d]", name.c_str(), index);
        realname = stringf("%s[%d]", name.c_str(), wire->start_offset + index);
      }
      std::vector<NET*> nets;
      if (!get_iopad_nets(out_nets, in_nets, fullname, nets)) {
        m_basic_status = false;
        continue;
      }
      if (nets.size() == 0) {
        m_basic_status = false;
        POST_ERR_MSG(3,
                     "%s Port %s (index=%d) is not directly connected to "
                     "any known primitive",
                     direction.c_str(), name.c_str(), index);
        continue;
      }
      DIRECTION net_dir = DIRECTION::UNKNOWN;
      for (auto& net : nets) {
        log_assert(net->port->dir == DIRECTION::IN ||
                   net->port->dir == DIRECTION::OUT);
        if ((net->port->dir == DIRECTION::IN && net_dir == DIRECTION::OUT) ||
            (net->port->dir == DIRECTION::OUT && net_dir == DIRECTION::IN)) {
          net_dir = DIRECTION::INOUT;
        } else {
          log_assert(net_dir != DIRECTION::INOUT);
          net_dir = net->port->dir;
        }
      }
      log_assert(net_dir != DIRECTION::UNKNOWN);
      if (dir != net_dir) {
        m_basic_status = false;
        POST_ERR_MSG(3,
                     "%s Port %s (index=%d) conflicts with net(s) effective "
                     "direction %s [%s]",
                     direction.c_str(), name.c_str(), index,
                     get_dir_string(net_dir).c_str(),
                     get_nets_id(nets).c_str());
        continue;
      }
      if ((nets.size() == 1 &&
           (dir == DIRECTION::IN || dir == DIRECTION::OUT)) ||
          (nets.size() == 2 && dir == DIRECTION::INOUT)) {
        for (auto& net : nets) {
          if (net->drive_sinks.size() == 0) {
            net->drive_sinks.push_back(new DRIVE_SINK(nullptr, direction, name,
                                                      fullname, realname, index,
                                                      wire->width));
          } else {
            m_basic_status = false;
            POST_ERR_MSG(
                3,
                "%s Port %s (index=%d) should connect to an unsigned net "
                "%s, "
                "but it is found had been connected to DriveSink [%s]",
                direction.c_str(), name.c_str(), index, net->id().c_str(),
                get_drive_sinks_id(net->drive_sinks).c_str());
          }
        }
      } else if (nets.size()) {
        m_basic_status = false;
        POST_ERR_MSG(3,
                     "%s Port %s (index=%d) should connect to one "
                     "(INPUT/OUTPUT) or two (INOUT) net(s) but found [%s]",
                     direction.c_str(), name.c_str(), index,
                     get_nets_id(nets).c_str());
      }
    }
  }
  return m_basic_status;
}

/*
  Get NET from DRIVE SINK
*/
NET* PRIMITIVES_EXTRACTOR::get_net_from_drive_sink(
    const DRIVE_SINK* drive_sink, const std::string& net_name) {
  log_assert(drive_sink != nullptr);
  log_assert(drive_sink->primitive != nullptr);
  const PRIMITIVE* primitive = drive_sink->primitive;
  log_assert(primitive->ports.find(drive_sink->port) != primitive->ports.end());
  const PORT* port = primitive->ports.at(drive_sink->port);
  const NET* net = port->get_net(net_name);
  log_assert(net != nullptr);
  return const_cast<NET*>(net);
}

/*
  Get IOPAD net
*/
bool PRIMITIVES_EXTRACTOR::get_iopad_nets(
    const std::map<std::string, DRIVE_SINK*>& out_nets,
    const std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets,
    const std::string& name, std::vector<NET*>& nets) {
  log_assert(nets.size() == 0);
  bool status = true;
  if (in_nets.find(name) != in_nets.end()) {
    const std::vector<DRIVE_SINK*>& dss = in_nets.at(name);
    for (auto& ds : dss) {
      NET* net = get_net_from_drive_sink(ds, name);
      log_assert(net->port->dir == DIRECTION::IN);
      nets.push_back(net);
    }
  }
  if (out_nets.find(name) != out_nets.end()) {
    NET* net = get_net_from_drive_sink(out_nets.at(name), name);
    log_assert(net->port->dir == DIRECTION::OUT);
    nets.push_back(net);
  }
  bool dummy_input;
  std::map<std::string, bool> dummy;
  for (auto& net : nets) {
    if (!get_yaml_port_iopad(net->port->primitive->yaml, net->port->name, dummy,
                             dummy_input)) {
      status = false;
      POST_ERR_MSG(3, "IOPAD %s is connected to internal %s", name.c_str(),
                   net->id().c_str());
    }
  }
  return status;
}

/*
  Validate Primitive
*/
bool PRIMITIVES_EXTRACTOR::validate_primitive() {
  POST_MSG(2, "Validate primitives");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  for (auto& dc : m_primitives) {
    PRIMITIVE* primitive = dc.second;
    for (auto& p : primitive->ports) {
      const PORT* port = p.second;
      if (port->nets.size()) {
        for (auto& net : port->nets) {
          if (net->drive_sinks.size() == 0) {
            m_basic_status = false;
            POST_ERR_MSG(3, "%s does not have drive/sink", net->id().c_str());
          }
        }
      } else {
        m_basic_status = false;
        POST_ERR_MSG(3, "%s does not have net", port->id().c_str());
      }
    }
  }
  return m_basic_status;
}

/*
  Validate netlist
*/
bool PRIMITIVES_EXTRACTOR::validate_netlist() {
  POST_MSG(1, "Validate netlist");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  const PATHS& paths = PATH_DB.at(m_technology);
  if (validate_data_port_iopad() && validate_fabric_control_port() &&
      validate_non_fabric_port() &&
      validate_path("data", &paths.data_paths, true) &&
      validate_path("reversed data", &paths.reversed_data_paths, true) &&
      validate_path("clock", &paths.clock_paths, false) &&
      validate_path("reversed clock", &paths.reversed_clock_paths, false)) {
  } else {
    m_netlist_status = false;
  }
  return m_netlist_status;
}

/*
  Make sure port with iopad_external_pin trait is connected to IOPAD
*/
bool PRIMITIVES_EXTRACTOR::validate_data_port_iopad() {
  POST_MSG(2, "Make sure Port Primitive Data Port is connected to IOPAD");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  for (auto& dc : m_primitives) {
    PRIMITIVE* primitive = dc.second;
    // Make sure all the iopad_external_pin must be connected to IOPAD
    for (auto& iter : primitive->yaml->ports) {
      const std::string& port_name = iter.first;
      const PORT_BASIC& basic_port = iter.second;
      const PORT* port = nullptr;
      if (primitive->ports.find(port_name) != primitive->ports.end()) {
        port = primitive->ports.at(port_name);
      }
      if (basic_port.is_attribute("iopad_external_pin")) {
        // This port must exist and must connect to IOPAD
        do {
          m_netlist_status = false;
          // Make sure port is used in the design
          if (port == nullptr) {
            POST_ERR_MSG(3,
                         "%s should have PORT %s that connect to IOPAD, but no "
                         "connection is made",
                         primitive->id().c_str(), port_name.c_str());
            break;
          }
          // Make sure every IOPAD is connected to one net
          if (port->nets.size() != 1) {
            POST_ERR_MSG(
                3, "%s should only connected to one net, but found %ld [%s]",
                port->id().c_str(), port->nets.size(),
                get_nets_id(port->nets).c_str());
            break;
          }
          // Make sure each net connect to one DriveSink
          const NET* net = port->nets[0];
          if (net->drive_sinks.size() != 1) {
            POST_ERR_MSG(
                3, "%s should connect to one DriveSink, but found %ld [%s]",
                net->id().c_str(), net->drive_sinks.size(),
                get_drive_sinks_id(net->drive_sinks).c_str());
            break;
          }
          // Make sure DrinkSink must be INPUT/OUTPUT/INOUT
          DRIVE_SINK* ds = net->drive_sinks[0];
          if (!(ds->type == "INPUT" || ds->type == "OUTPUT" ||
                ds->type == "INOUT")) {
            POST_ERR_MSG(
                3,
                "%s should connect to IOPAD, but found it is connected to %s",
                net->id().c_str(), get_port_id(ds).c_str());
            break;
          }
          // Make the direction match
          if ((port->dir == DIRECTION::IN && ds->type == "OUTPUT") ||
              (port->dir == DIRECTION::OUT && ds->type == "INPUT")) {
            POST_ERR_MSG(3, "%s has direction conflict with its %s",
                         net->id().c_str(), ds->id().c_str());
            break;
          }
          // Everything is good
          m_netlist_status = true;
          POST_MSG(3, "%s -> %s", port->id().c_str(), get_port_id(ds).c_str());
        } while (0);
      } else if (port != nullptr) {
        // This is not iopad_external_pin
        // Might not exist, but if it does, should not connect to IOPAD
        for (auto& net : port->nets) {
          for (auto& ds : net->drive_sinks) {
            if (ds->type == "INPUT" || ds->type == "OUTPUT" ||
                ds->type == "INOUT") {
              m_netlist_status = false;
              POST_ERR_MSG(3,
                           "%s should not connect to IOPAD, but found it is "
                           "connected to %s",
                           net->id().c_str(), get_port_id(ds).c_str());
            }
          }
        }
      }
    }
  }
  return m_netlist_status;
}

/*
  Make sure fabric control port is connected to fabric
*/
bool PRIMITIVES_EXTRACTOR::validate_fabric_control_port() {
  POST_MSG(2, "Make sure Primitive Control Port is connected to FABRIC");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  for (auto& dc : m_primitives) {
    PRIMITIVE* primitive = dc.second;
    const std::map<std::string, std::string>& map =
        primitive->yaml->fabric_control_map;
    for (auto& p : primitive->ports) {
      const PORT* port = p.second;
      if (map.find(port->name) == map.end()) {
        continue;
      }
      for (auto& net : port->nets) {
        if (net->drive_sinks.size() != 1) {
          m_netlist_status = false;
          POST_ERR_MSG(3,
                       "%s should have only one DriveSink that is directly "
                       "connected to FABRIC, but found %ld [%s]",
                       net->id().c_str(), net->drive_sinks.size(),
                       get_drive_sinks_id(net->drive_sinks).c_str());
          continue;
        }
        DRIVE_SINK* ds = net->drive_sinks[0];
        if (ds->type == "FABRIC") {
          POST_MSG(3, "%s -> %s", port->id().c_str(), get_port_id(ds).c_str());
        } else {
          m_netlist_status = false;
          POST_ERR_MSG(
              3, "%s should connect to FABRIC, but found it is connected to %s",
              net->id().c_str(), get_port_id(ds).c_str());
        }
      }
    }
  }
  return m_netlist_status;
}

/*
  Make sure non fabric port is not connected to fabric
*/
bool PRIMITIVES_EXTRACTOR::validate_non_fabric_port() {
  POST_MSG(2, "Make sure nonFABRIC port is not conneted to FABRIC");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  for (auto& dc : m_primitives) {
    PRIMITIVE* primitive = dc.second;
    const std::vector<std::string>& non_fabric_ports =
        primitive->yaml->non_fabric_ports;
    for (auto& p : primitive->ports) {
      const PORT* port = p.second;
      if (std::find(non_fabric_ports.begin(), non_fabric_ports.end(),
                    port->name) == non_fabric_ports.end()) {
        continue;
      }
      for (auto& net : port->nets) {
        for (auto& ds : net->drive_sinks) {
          if (ds->type == "FABRIC") {
            m_netlist_status = false;
            POST_ERR_MSG(3,
                         "%s should not connect to FABRIC, but found it is "
                         "connected to %s",
                         net->id().c_str(), ds->id().c_str());
          } else {
            POST_MSG(3, "%s -> %s", port->id().c_str(), ds->id().c_str());
          }
        }
      }
    }
  }
  return m_netlist_status;
}

/*
  Make sure data path is valid
*/
bool PRIMITIVES_EXTRACTOR::validate_path(const std::string& path_name,
                                         const void* paths_ptr,
                                         bool must_connect) {
  POST_MSG(2, "Validate %s path", path_name.c_str());
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  log_assert(paths_ptr != nullptr);
  const std::vector<PATH>* paths =
      reinterpret_cast<const std::vector<PATH>*>(paths_ptr);
  for (const auto& path : *paths) {
    for (auto& dc : m_primitives) {
      PRIMITIVE* primitive = dc.second;
      if (primitive->type == path.src.primitive) {
        if (primitive->ports.find(path.src.port) != primitive->ports.end()) {
          const PORT* port = primitive->ports.at(path.src.port);
          const std::vector<NODE>& dests = path.dests;
          if (dests.size()) {
            for (auto& net : port->nets) {
              for (auto& ds : net->drive_sinks) {
                bool match = false;
                for (auto& dest : dests) {
                  if (dest.primitive == "FABRIC") {
                    log_assert(dest.port.empty());
                    if (ds->type == "FABRIC") {
                      match = true;
                      break;
                    }
                  } else {
                    log_assert(dest.primitive.size());
                    log_assert(dest.port.size());
                    if (dest.primitive == ds->type && dest.port == ds->port) {
                      match = true;
                      break;
                    }
                  }
                }
                if (match) {
                  POST_MSG(3, "%s -> %s", net->port->id().c_str(),
                           get_port_id(ds).c_str());
                } else {
                  m_netlist_status = false;
                  POST_ERR_MSG(3, "%s has invalid connection to %s",
                               net->port->id().c_str(),
                               get_port_id(ds).c_str());
                }
              }
            }
          }
        } else if (must_connect) {
          m_netlist_status = false;
          POST_ERR_MSG(3, "%s does not have connect at Port %s",
                       primitive->id().c_str(), path.src.port.c_str());
        }
      }
    }
  }
  return m_netlist_status;
}

/*
  Build chain
*/
bool PRIMITIVES_EXTRACTOR::build_chain(
    std::vector<PRIMITIVE*>& parents,
    std::vector<const PRIMITIVE*>& core_clocks, const std::string& sdc) {
  POST_MSG(1, "Build chain");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  log_assert(parents.size() == 0);
  std::map<std::string, uint32_t> tracker;
  std::vector<std::string> clock_childs;
  std::vector<PRIMITIVE*> data_parents;
  std::vector<PRIMITIVE*> clock_parents;
  std::vector<PRIMITIVE*> standalone_parents;
  const std::vector<PATH>& clock_paths =
      PATH_DB.at(m_technology).clock_child_paths;

  // 1. Build DATA
  POST_MSG(2, "Gather DATA parent primitive");
  for (auto& dc : m_primitives) {
    PRIMITIVE* primitive = dc.second;
    if (primitive->has_iopad_external_pin()) {
      data_parents.push_back(primitive);
    }
  }

  // 2. Trace DATA
  POST_MSG(2, "Trace DATA Chain");
  for (auto& p : data_parents) {
    if (!trace_child_data_path(p, p, 3)) {
      m_netlist_status = false;
      break;
    }
  }
  if (!m_netlist_status) {
    goto BUILD_CHAIN_END;
  }

  // 3. Build ClOCK
  POST_MSG(2, "Gather CLOCK parent primitive");
  // 3a. Find out the child
  for (auto& path : clock_paths) {
    for (auto& dest : path.dests) {
      if (std::find(clock_childs.begin(), clock_childs.end(), dest.primitive) ==
          clock_childs.end()) {
        clock_childs.push_back(dest.primitive);
      }
    }
  }
  // 3b. Loop through clock child paths (purposely defined in certain
  // sequence)
  for (auto& path : clock_paths) {
    // Exclude anything which is child
    if (std::find(clock_childs.begin(), clock_childs.end(),
                  path.src.primitive) == clock_childs.end()) {
      for (auto& dc : m_primitives) {
        PRIMITIVE* primitive = dc.second;
        if (primitive->type == path.src.primitive &&
            primitive->ports.find(path.src.port) != primitive->ports.end()) {
          bool is_clock_parent = false;
          if (primitive->yaml->group == "DATA") {
            if (path.dests.size() == 0 || primitive->has_clock_child) {
              is_clock_parent = true;
            }
          } else {
            is_clock_parent = true;
          }
          if (is_clock_parent) {
            log_assert(std::find(clock_parents.begin(), clock_parents.end(),
                                 primitive) == clock_parents.end());
            clock_parents.push_back(primitive);
          }
        }
      }
    }
  }

  // 4. Trace CLOCK
  POST_MSG(2, "Trace CLOCK Chain");
  for (auto& p : clock_parents) {
    if (!trace_child_clock_path(p, p, 3)) {
      m_netlist_status = false;
      break;
    }
  }
  if (!m_netlist_status) {
    goto BUILD_CHAIN_END;
  }

  // 5. Build STANDALONE: pure control which is not data or clock
  POST_MSG(2, "Gather STANDALONE/CONTROL parent primitive");
  for (auto& dc : m_primitives) {
    PRIMITIVE* primitive = dc.second;
    // DLY_SEL_DECODER or SOC
    if (primitive->yaml->group == "STANDALONE_FABRIC" ||
        primitive->yaml->group == "SOC") {
      standalone_parents.push_back(primitive);
    }
  }

  // 6. Trace STANDALONE
  POST_MSG(2, "Trace STANDALONE/CONTROL Chain");
  for (auto& p : standalone_parents) {
    POST_MSG(3, "%s", p->id().c_str());
  }

  // 7. Put everything in one list
  for (auto& ps : std::vector<std::vector<PRIMITIVE*>>(
           {clock_parents, data_parents, standalone_parents})) {
    for (auto& p : ps) {
      if (p->type != "I_SERDES" &&
          std::find(parents.begin(), parents.end(), p) == parents.end()) {
        parents.push_back(p);
      }
    }
  }

  // 8. Determine linked object
  POST_MSG(2, "Determine linked object");
  for (auto& p : parents) {
    p->determine_objects(tracker);
  }

  // 9. Summarize
  summarize(parents);

  // 10. Check if anything is missing
  if (trace_missing_primitive(parents) && assign_location(data_parents, sdc) &&
      determine_mode(data_parents) &&
      determine_standalone_fabric_location(standalone_parents) &&
      determine_core_clock(clock_parents, core_clocks) &&
      determine_fast_clock(clock_parents)) {
  } else {
    m_netlist_status = false;
  }
BUILD_CHAIN_END:
  return m_netlist_status;
}

/*
  Search the valid path
*/
const void* PRIMITIVES_EXTRACTOR::search_valid_paths(
    const void* paths_ptr, const std::string& primitive) {
  const std::vector<PATH>* paths =
      reinterpret_cast<const std::vector<PATH>*>(paths_ptr);
  const PATH* path = nullptr;
  for (const auto& p : *paths) {
    if (p.src.primitive == primitive) {
      path = &p;
      break;
    }
  }
  return path;
}

/*
  Trace data child
*/
bool PRIMITIVES_EXTRACTOR::trace_child_data_path(PRIMITIVE*& grandparent,
                                                 PRIMITIVE*& parent,
                                                 uint32_t space) {
  POST_MSG(space, "%s", parent->id().c_str());
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  log_assert(!parent->has_clock_child);
  log_assert(parent->data_child == nullptr);
  log_assert(parent->clock_child == nullptr);
  // From PRIMITIVE find the corresponding data path
  const void* path_ptr =
      search_valid_paths(&PATH_DB.at(m_technology).data_paths, parent->type);
  if (path_ptr != nullptr) {
    const PATH* path = reinterpret_cast<const PATH*>(path_ptr);
    if (parent->ports.find(path->src.port) != parent->ports.end()) {
      const PORT* port = parent->ports.at(path->src.port);
      log_assert(port->nets.size());
      const std::vector<NODE>& dests = path->dests;
      size_t fabric_count = 0;
      size_t clock_count = 0;
      size_t data_count = 0;
      size_t standalone_count = 0;
      PRIMITIVE* child = nullptr;
      for (auto& net : port->nets) {
        for (auto& ds : net->drive_sinks) {
          for (auto& dest : dests) {
            if (dest.group == "FABRIC") {
              if (ds->type == "FABRIC") {
                fabric_count++;
              }
            } else if (dest.primitive == ds->type && dest.port == ds->port) {
              if (dest.group == "CLOCK") {
                clock_count++;
              } else if (dest.group == "STANDALONE_DATA") {
                standalone_count++;
                log_assert(ds->primitive != nullptr);
                child = const_cast<PRIMITIVE*>(ds->primitive);
              } else {
                log_assert(dest.group == "DATA");
                log_assert(ds->primitive != nullptr);
                child = const_cast<PRIMITIVE*>(ds->primitive);
                data_count++;
              }
            }
          }
        }
      }
      /*
        IF it is connected to CLOCK
          It must be I_BUF/I_BUF_DS, the net size must be 1
          Might have DATA or FABRIC
          Must not have STANDALONE
        IF it is DATA
          Net size must be 1
          Cannot co-exist with FABRIC and STANDALONE
        IF it is FABRIC
          Net size can be >= 1
          Cannot co-exist with DATA and STANDLONE
        IF it is STANDALONE
          Net size must be 1
          Cannot co-exist with DATA and FABRIC
      */
      // Check CLOCK
      m_netlist_status = validate_data_path_clock(space + 1, port, clock_count,
                                                  standalone_count);
      if (m_netlist_status && clock_count != 0) {
        parent->has_clock_child = true;
      }
      // Check DATA
      if (m_netlist_status) {
        if (data_count) {
          m_netlist_status = validate_data_path_data(
              space + 1, port, data_count, fabric_count, standalone_count);
          if (m_netlist_status) {
            log_assert(child != nullptr);
            log_assert(child->parent == nullptr);
            log_assert(child->grandparent == nullptr);
            parent->data_child = child;
            child->parent = parent;
            child->grandparent = grandparent;
            m_netlist_status = trace_child_data_path(
                grandparent, parent->data_child, space + 1);
          }
        } else if (fabric_count) {
          m_netlist_status = validate_data_path_fabric(
              space + 1, port, data_count, fabric_count, standalone_count);
        } else if (standalone_count) {
          m_netlist_status = validate_data_path_standalone(
              space + 1, port, data_count, fabric_count, standalone_count);
          if (m_netlist_status) {
            log_assert(child != nullptr);
            log_assert(child->parent == nullptr);
            log_assert(child->grandparent == nullptr);
            POST_MSG(space + 1, "%s", child->id().c_str());
            parent->data_child = child;
            child->parent = parent;
            child->grandparent = grandparent;
          }
        } else if (clock_count) {
          // Already checked
        } else {
          m_netlist_status = false;
          POST_ERR_MSG(space + 1, "%s does not have valid DriveSink",
                       port->id().c_str());
        }
      }
    } else {
      m_netlist_status = false;
      POST_ERR_MSG(space + 1,
                   "%s data path incomplete - missing %s port connection",
                   parent->id().c_str(), path->src.port.c_str());
    }
  } else {
    m_netlist_status = false;
    POST_ERR_MSG(space + 1, "%s does not have data path database",
                 parent->type.c_str());
  }
  return m_netlist_status;
}

/*
  IF there is connection to CLOCK
    It must be I_BUF/I_BUF_DS, the net size must be 1
    Might have DATA or FABRIC
    Must not have STANDALONE
*/
bool PRIMITIVES_EXTRACTOR::validate_data_path_clock(uint32_t space,
                                                    const PORT* port,
                                                    size_t clock_count,
                                                    size_t standalone_count) {
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  if (clock_count) {
    if (port->nets.size() == 1) {
      std::vector<DRIVE_SINK*> dss = port->nets[0]->drive_sinks;
      if (clock_count == 1) {
        // Connection to DATA and FABRIC does not matter
        if (standalone_count) {
          POST_ERR_MSG(space,
                       "%s that is connected to CLOCK primitive, should not "
                       "connect to STANDALONE primitive but found [%s]",
                       port->id().c_str(), get_drive_sinks_id(dss).c_str());
        }
      } else {
        m_netlist_status = false;
        POST_ERR_MSG(space,
                     "%s that is connected to CLOCK primitive, should one "
                     "CLOCK DriveSink, but found [%s]",
                     port->id().c_str(), get_drive_sinks_id(dss).c_str());
      }
    } else {
      m_netlist_status = false;
      POST_ERR_MSG(space,
                   "%s that is connected to CLOCK primitive, should have port "
                   "size=1, but found size of %ld",
                   port->id().c_str(), port->nets.size());
    }
  }
  return m_netlist_status;
}

/*
  IF there is connection to DATA
    Net size must be 1
    Cannot co-exist with FABRIC and STANDALONE
*/
bool PRIMITIVES_EXTRACTOR::validate_data_path_data(uint32_t space,
                                                   const PORT* port,
                                                   size_t data_count,
                                                   size_t fabric_count,
                                                   size_t standalone_count) {
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  do {
    if (data_count == 0) {
      break;
    }
    m_netlist_status = false;
    if (port->nets.size() != 1) {
      POST_ERR_MSG(space,
                   "%s that connected to DATA primitive, should have port "
                   "size=1, but found size of %ld",
                   port->id().c_str(), port->nets.size());
      break;
    }
    std::vector<DRIVE_SINK*> dss = port->nets[0]->drive_sinks;
    if (data_count != 1) {
      POST_ERR_MSG(space,
                   "%s that connected to DATA primitive, should have one "
                   "DriveSink, but found [%s]",
                   port->id().c_str(), get_drive_sinks_id(dss).c_str());
      break;
    }
    m_netlist_status = true;
    if (fabric_count) {
      m_netlist_status = false;
      POST_ERR_MSG(space,
                   "%s that connected to DATA primitive, should not connect to "
                   "FABRIC, but found [%s]",
                   port->id().c_str(), get_drive_sinks_id(dss).c_str());
    }
    if (standalone_count) {
      m_netlist_status = false;
      POST_ERR_MSG(space,
                   "%s that connected to DATA primitive, should not connect to "
                   "STANDALONE primitive, but found [%s]",
                   port->id().c_str(), get_drive_sinks_id(dss).c_str());
    }
  } while (0);
  return m_netlist_status;
}

/*
  IF there is connection to FABRIC
    Net size can be >= 1 (which match fabric_count)
      One net one DriveSink
    Cannot co-exist with FABRIC and STANDALONE
*/
bool PRIMITIVES_EXTRACTOR::validate_data_path_fabric(uint32_t space,
                                                     const PORT* port,
                                                     size_t data_count,
                                                     size_t fabric_count,
                                                     size_t standalone_count) {
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  do {
    if (fabric_count == 0) {
      break;
    }
    if (fabric_count != port->nets.size()) {
      m_netlist_status = false;
      POST_ERR_MSG(space,
                   "%s connection to FABRIC does not match up (Net size: %ld "
                   "vs FABRIC DriveSink size: %ld)",
                   port->id().c_str(), port->nets.size(), fabric_count);
      break;
    }
    // So far so good
    std::vector<DRIVE_SINK*> dss;
    for (auto& net : port->nets) {
      for (auto& ds : net->drive_sinks) {
        dss.push_back(ds);
      }
    }
    if (data_count) {
      m_netlist_status = false;
      POST_ERR_MSG(space,
                   "%s that connected to FABRIC, should not connect to DATA "
                   "primitive, but found [%s]",
                   port->id().c_str(), get_drive_sinks_id(dss).c_str());
    }
    if (standalone_count) {
      m_netlist_status = false;
      POST_ERR_MSG(space,
                   "%s that connected to FABRIC, should not connect to "
                   "STANDALONE primitive, but found [%s]",
                   port->id().c_str(), get_drive_sinks_id(dss).c_str());
    }
  } while (0);
  return m_netlist_status;
}

/*
  IF there is connection to STANDALONE
    Net size must be 1
    Cannot co-exist with DATA and FABRIC
*/
bool PRIMITIVES_EXTRACTOR::validate_data_path_standalone(
    uint32_t space, const PORT* port, size_t data_count, size_t fabric_count,
    size_t standalone_count) {
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  do {
    if (standalone_count == 0) {
      break;
    }
    m_netlist_status = false;
    if (port->nets.size() != 1) {
      POST_ERR_MSG(space,
                   "%s that connected to STANDALONE primitive, should have "
                   "port size=1, but found size of %ld",
                   port->id().c_str(), port->nets.size());
      break;
    }
    std::vector<DRIVE_SINK*> dss = port->nets[0]->drive_sinks;
    if (standalone_count != 1) {
      POST_ERR_MSG(space,
                   "%s that connected to STANDALONE primitive, should have one "
                   "DriveSink, but found [%s]",
                   port->id().c_str(), get_drive_sinks_id(dss).c_str());
    }
    m_netlist_status = true;
    if (data_count) {
      m_netlist_status = false;
      POST_ERR_MSG(space,
                   "%s that connected to STANDALONE primitive, should not "
                   "connect to DATA primitive, but found [%s]",
                   port->id().c_str(), get_drive_sinks_id(dss).c_str());
    }
    if (fabric_count) {
      m_netlist_status = false;
      POST_ERR_MSG(space,
                   "%s that connected to STANDALONE primitive, should not "
                   "connect to FABRIC, but found [%s]",
                   port->id().c_str(), get_drive_sinks_id(dss).c_str());
    }
  } while (0);
  return m_netlist_status;
}

/*
  Trace clock child
*/
bool PRIMITIVES_EXTRACTOR::trace_child_clock_path(PRIMITIVE*& grandparent,
                                                  PRIMITIVE*& parent,
                                                  uint32_t space) {
  POST_MSG(space, "%s", parent->id().c_str());
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  log_assert(parent->clock_child == nullptr);
  // From PRIMITIVE find the corresponding data path
  const void* path_ptr = search_valid_paths(
      &PATH_DB.at(m_technology).clock_child_paths, parent->type);
  // must exist, caller already make sure of that
  log_assert(path_ptr != nullptr);
  const PATH* path = reinterpret_cast<const PATH*>(path_ptr);
  if (parent->ports.find(path->src.port) != parent->ports.end()) {
    const PORT* port = parent->ports.at(path->src.port);
    log_assert(port->nets.size());
    const std::vector<NODE>& dests = path->dests;
    if (dests.size()) {
      for (auto& net : port->nets) {
        for (auto& ds : net->drive_sinks) {
          for (auto& dest : dests) {
            if (dest.primitive == ds->type && dest.port == ds->port) {
              PRIMITIVE* child = const_cast<PRIMITIVE*>(ds->primitive);
              log_assert(child->parent == nullptr);
              log_assert(child->grandparent == nullptr);
              if (parent->clock_child == nullptr) {
                parent->clock_child = child;
                child->parent = parent;
                child->grandparent = grandparent;
                if (search_valid_paths(
                        &PATH_DB.at(m_technology).clock_child_paths,
                        parent->clock_child->type) != nullptr) {
                  trace_child_clock_path(grandparent, parent->clock_child,
                                         space + 1);
                } else {
                  POST_MSG(space + 1, "%s", parent->clock_child->id().c_str());
                }
              } else {
                m_netlist_status = false;
                POST_ERR_MSG(
                    space + 1,
                    "%s should only have one clock child %s - found another %s",
                    parent->id().c_str(), parent->clock_child->id().c_str(),
                    ds->id().c_str());
              }
            }
          }
          if (!m_netlist_status) {
            break;
          }
        }
        if (!m_netlist_status) {
          break;
        }
      }
    }
  } else {
    m_netlist_status = false;
    POST_ERR_MSG(space + 1,
                 "%s data path incomplete - missing %s port connection",
                 parent->id().c_str(), path->src.port.c_str());
  }
  return m_netlist_status;
}

/*
  Trace if there is any missing primitive
*/
bool PRIMITIVES_EXTRACTOR::trace_missing_primitive(
    std::vector<PRIMITIVE*>& parents) {
  POST_MSG(2, "Make sure all Primitive are in the chain");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  for (auto& dc : m_primitives) {
    PRIMITIVE* primitive = dc.second;
    if (!trace_if_primitive_exist(parents, primitive)) {
      m_netlist_status = false;
      POST_ERR_MSG(3, "%s is not in the chain", primitive->id().c_str());
    }
  }
  return m_netlist_status;
}

/*
  Trace if primitive exist in the vector hierarchy
*/
bool PRIMITIVES_EXTRACTOR::trace_if_primitive_exist(
    std::vector<PRIMITIVE*>& cells, PRIMITIVE* cell) {
  bool found = false;
  for (auto c : cells) {
    if (c == cell) {
      found = true;
      break;
    }
    auto child = c->clock_child;
    while (child != nullptr) {
      if (child == cell) {
        found = true;
        break;
      }
      child = child->clock_child;
    }
    if (found) {
      break;
    }
    child = c->data_child;
    while (child != nullptr) {
      if (child == cell) {
        found = true;
        break;
      }
      child = child->data_child;
    }
    if (found) {
      break;
    }
  }
  return found;
}

/*
  Location assignment
*/
bool PRIMITIVES_EXTRACTOR::assign_location(std::vector<PRIMITIVE*>& datas,
                                           const std::string& sdc) {
  POST_MSG(2, "Pin assignment");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  if (sdc.size()) {
    std::ifstream file(sdc);
    log_assert(file.is_open());
    std::string line = "";
    while (std::getline(file, line)) {
      std::vector<std::string> tokens = tokenizeString(line);
      if (tokens.size()) {
        if (tokens.size() == 3 && tokens[0] == "set_pin_loc") {
          std::string port_name = tokens[1];
          std::string location = tokens[2];
          bool found = false;
          for (auto& d : datas) {
            if (d->objects.find(port_name) != d->objects.end()) {
              OBJECT*& object = d->objects.at(port_name);
              log_assert(d->ports.find(object->port_name) != d->ports.end());
              const PORT* port = d->ports.at(object->port_name);
              POST_MSG(3, "Assign location %s to IOPAD %s (%s)",
                       location.c_str(), port_name.c_str(), port->id().c_str());
              object->location = location;
              PARSED_LOCATION parsed_location;
              if (parsed_location.parse(location)) {
                if (d->location_object == port_name) {
                  found = true;
                  d->location = location;
                  log_assert(d->parsed_location.parse(d->location));
                }
              } else {
                m_netlist_status = false;
                POST_ERR_MSG(4, "%s location (%s) assignment failed: %s",
                             port->id().c_str(), location.c_str(),
                             parsed_location.failure_reason.c_str());
                break;
              }
            }
          }
          if (!found) {
            POST_MSG(3, "Warning: Fail to assign location to PORT %s",
                     tokens[1].c_str());
          }
        } else {
          POST_MSG(3, "Warning: Unknown SDC command - %s", line.c_str());
        }
      }
    }
    file.close();
  } else {
    POST_MSG(3, "Warning: Does not have user pin SDC");
  }
  return m_netlist_status;
}

/*
  Determine mode
*/
bool PRIMITIVES_EXTRACTOR::determine_mode(std::vector<PRIMITIVE*>& datas) {
  POST_MSG(2, "Determine data mode");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  for (auto& data : datas) {
    log_assert(data->mode.empty());
    auto end_data = data->get_end_data_child();
    std::string chain = data->get_data_chain_info();
    chain =
        stringf("IOPAD %s (%s)", data->linked_object.c_str(), chain.c_str());
    bool status = true;
    if (end_data->type == "I_BUF" || end_data->type == "I_BUF_DS" ||
        end_data->type == "I_DELAY" || end_data->type == "O_BUFT" ||
        end_data->type == "O_BUFT_DS" || end_data->type == "O_DELAY") {
      data->mode = "MODE_BP_DIR";
    } else if (end_data->type == "I_DDR" || end_data->type == "O_DDR") {
      data->mode = "MODE_BP_DDR";
    } else if (end_data->type == "I_SERDES" || end_data->type == "O_SERDES") {
      if (end_data->parameters.find("WIDTH") != end_data->parameters.end()) {
        data->mode = stringf("MODE_RATE_%d",
                             std::stoi(end_data->parameters.at("WIDTH")));
      } else {
        status = false;
        POST_ERR_MSG(3, "%s is missing WIDTH parameter to determine mode rate",
                     end_data->id().c_str());
      }
    } else {
      log_assert(end_data->type == "O_SERDES_CLK");
    }
    if (status) {
      POST_MSG(3, "%s - %s", chain.c_str(), data->mode.c_str());
    } else {
      m_netlist_status = false;
    }
  }
  return m_netlist_status;
}

/*
  Determine mode
*/
bool PRIMITIVES_EXTRACTOR::determine_standalone_fabric_location(
    std::vector<PRIMITIVE*>& standalones) {
  POST_MSG(2, "Determine STANDALONE FABRIC location");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  for (auto& standalone : standalones) {
    log_assert(standalone->location.empty());
    if (standalone->yaml->group != "STANDALONE_FABRIC") {
      continue;
    }
    if (standalone->type == "DLY_SEL_DECODER") {
      POST_MSG(3, "%s", standalone->id().c_str());
      std::vector<std::string> control_ports;
      std::string location = "";
      bool status = true;
      for (int i = 0; i < 20; i++) {
        std::string control_port = stringf("DLY%d_CNTRL", i);
        if (standalone->ports.find(control_port) != standalone->ports.end()) {
          const PORT* port = standalone->ports.at(control_port);
          for (auto& net : port->nets) {
            for (auto& ds : net->drive_sinks) {
              if (ds->type == "I_DELAY" || ds->type == "O_DELAY") {
                const PRIMITIVE* delay = ds->primitive;
                log_assert(delay != nullptr);
                if (delay->grandparent->location.size()) {
                  std::string half_bank_location =
                      delay->grandparent->parsed_location
                          .get_half_bank_location();
                  POST_MSG(4, "%s -> %s (Location: %s -> %s)",
                           net->id().c_str(), ds->id().c_str(),
                           delay->grandparent->location.c_str(),
                           half_bank_location.c_str());
                  if (location.empty()) {
                    location = half_bank_location;
                  } else if (location != half_bank_location) {
                    status = false;
                    POST_ERR_MSG(5,
                                 "DELAY primitives are not groups correctly");
                  }
                } else {
                  POST_MSG(4, "%s -> %s (Location: null)", net->id().c_str(),
                           ds->id().c_str());
                }
              } else {
                status = false;
                POST_ERR_MSG(4,
                             "%s should only connect to I_DELAY or O_DELAY, "
                             "but it is connected to %s",
                             net->id().c_str(), ds->id().c_str());
              }
            }
          }
        }
      }
      if (status) {
        POST_MSG(4, "Effective location: %s", location.c_str());
        if (location.size()) {
          standalone->location = location;
          log_assert(standalone->parsed_location.parse(standalone->location));
        }
      } else {
        m_netlist_status = false;
      }
    } else {
      m_netlist_status = false;
      POST_ERR_MSG(3, "Do not know how to handlle STANDALONE FABRIC %s",
                   standalone->id().c_str());
    }
  }
  return m_netlist_status;
}

/*
  Determine CORE clock
*/
bool PRIMITIVES_EXTRACTOR::determine_core_clock(
    std::vector<PRIMITIVE*>& clocks,
    std::vector<const PRIMITIVE*>& core_clocks) {
  POST_MSG(2, "Determine CORE/FABRIC clock");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  log_assert(core_clocks.size() == 0);
  const PATHS& paths = PATH_DB.at(m_technology);
  std::map<std::string, int> src_max_index;
  for (auto& src : paths.core_clock_drives) {
    if (src_max_index.find(src.primitive) == src_max_index.end()) {
      src_max_index[src.primitive] = 0;
    }
    src_max_index[src.primitive] = src_max_index.at(src.primitive) + 1;
  }
  int slot = 0;
  int fclk_buf_slot = 0;
  for (auto clock : clocks) {
    auto child = clock;
    while (child != nullptr) {
      std::map<std::string, int> src_index;
      for (auto& src : paths.core_clock_drives) {
        if (src_index.find(src.primitive) == src_index.end()) {
          src_index[src.primitive] = 0;
        }
        if (src.primitive == child->type &&
            child->ports.find(src.port) != child->ports.end()) {
          const PORT* port = child->ports.at(src.port);
          log_assert(port->nets.size() == 1);
          CLOCK_DRIVEN_TO* cdt =
              new CLOCK_DRIVEN_TO(port->name, slot, fclk_buf_slot);
          for (auto& net : port->nets) {
            for (auto& ds : net->drive_sinks) {
              for (auto& dest : paths.core_clock_sinks) {
                if (ds->type == dest.primitive &&
                    (dest.primitive == "FABRIC" || ds->port == dest.port)) {
                  if (cdt->dest_ports.size() == 0 &&
                      cdt->fabric_ports.size() == 0) {
                    POST_MSG(3, "%s", port->id().c_str());
                  }
                  // Update DRIVE TO info
                  if (dest.primitive == "FABRIC") {
                    POST_MSG(4, "-> FABRIC (Port: %s)", ds->port.c_str());
                    cdt->fabric_ports.push_back(ds->port);
                  } else {
                    log_assert(ds->primitive != nullptr);
                    log_assert(ds->primitive->ports.find(dest.port) !=
                               ds->primitive->ports.end());
                    cdt->dest_ports.push_back(
                        ds->primitive->ports.at(dest.port));
                    // Update DRIVEN BY info
                    log_assert(ds->primitive->core_clock_driven_by.first ==
                               nullptr);
                    log_assert(
                        ds->primitive->core_clock_driven_by.second.empty());
                    (const_cast<PRIMITIVE*>(ds->primitive))
                        ->core_clock_driven_by =
                        std::make_pair(child, port->name);
                    POST_MSG(4, "-> %s (Port: %s)", ds->primitive->id().c_str(),
                             ds->port.c_str());
                  }
                }
              }
            }
          }
          if (cdt->dest_ports.size() != 0 || cdt->fabric_ports.size() != 0) {
            log_assert(cdt->slot == slot);
            std::string parameter = "ROUTE_TO_FABRIC_CLK";
            if (src_max_index.at(src.primitive) > 1) {
              parameter = stringf("OUT%d_ROUTE_TO_FABRIC_CLK",
                                  src_index.at(src.primitive));
            }
            POST_MSG(5, "Define parameter %s=%d", parameter.c_str(), slot);
            child->core_clock_drive_to.push_back(cdt);
            child->set_parameter(parameter, std::to_string(slot));
            slot++;
            if (cdt->fabric_ports.size() > 1) {
              m_netlist_status = false;
              POST_ERR_MSG(
                  4, "Single clock source drives to multiple FABRIC ports");
            }
          } else {
            delete cdt;
          }
        }
        src_index[src.primitive] = src_index.at(src.primitive) + 1;
      }
      if (child->core_clock_drive_to.size()) {
        log_assert(std::find(core_clocks.begin(), core_clocks.end(), child) ==
                   core_clocks.end());
        core_clocks.push_back(child);
        if (child->type == "FCLK_BUF") {
          log_assert(child->core_clock_drive_to.size() == 1);
          POST_MSG(5, "Define parameter ROUTE_FROM_FABRIC_CLK=%d",
                   fclk_buf_slot);
          child->set_parameter("ROUTE_FROM_FABRIC_CLK",
                               std::to_string(fclk_buf_slot));
          fclk_buf_slot++;
        }
      }
      child = child->clock_child;
    }
  }
  if (slot > MAX_CLOCK_SLOT) {
    m_netlist_status = false;
    POST_MSG(3, "There is only maximum of %d clock slots. But used %d slots",
             MAX_CLOCK_SLOT, slot);
  }
  if (fclk_buf_slot > MAX_FABRIC_CLOCK_SLOT) {
    m_netlist_status = false;
    POST_MSG(
        3, "There is only maximum of %d FABRIC clock slots. But used %d slots",
        MAX_FABRIC_CLOCK_SLOT, fclk_buf_slot);
  }
  return m_netlist_status;
}

/*
  Determine FAST clock
*/
bool PRIMITIVES_EXTRACTOR::determine_fast_clock(
    std::vector<PRIMITIVE*>& clocks) {
  POST_MSG(2, "Determine FAST clock");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  const PATHS& paths = PATH_DB.at(m_technology);
  for (auto clock : clocks) {
    auto child = clock;
    while (child != nullptr) {
      for (auto& src : paths.fast_clock_drives) {
        if (src.primitive == child->type &&
            child->ports.find(src.port) != child->ports.end()) {
          const PORT* port = child->ports.at(src.port);
          log_assert(port->nets.size() == 1);
          CLOCK_DRIVEN_TO* cdt = new CLOCK_DRIVEN_TO(port->name);
          for (auto& net : port->nets) {
            for (auto& ds : net->drive_sinks) {
              for (auto& dest : paths.fast_clock_sinks) {
                log_assert(dest.primitive != "FABRIC");
                if (ds->type == dest.primitive && ds->port == dest.port) {
                  log_assert(ds->primitive != nullptr);
                  log_assert(ds->primitive->ports.find(dest.port) !=
                             ds->primitive->ports.end());
                  log_assert(cdt->fabric_ports.size() == 0);
                  if (cdt->dest_ports.size() == 0) {
                    POST_MSG(3, "%s", port->id().c_str());
                  }
                  // Update DRIVE TO info
                  cdt->dest_ports.push_back(ds->primitive->ports.at(dest.port));
                  // Update DRIVEN BY info
                  log_assert(ds->primitive->fast_clock_driven_by.first ==
                             nullptr);
                  log_assert(
                      ds->primitive->fast_clock_driven_by.second.empty());
                  (const_cast<PRIMITIVE*>(ds->primitive))
                      ->fast_clock_driven_by =
                      std::make_pair(child, port->name);
                  POST_MSG(4, "-> %s (Port: %s)", ds->primitive->id().c_str(),
                           ds->port.c_str());
                }
              }
            }
          }
          for (auto& net : port->nets) {
            for (auto& ds : net->drive_sinks) {
              for (auto& dest : paths.low_priority_fast_clock_sinks) {
                log_assert(dest.primitive != "FABRIC");
                if (ds->type == dest.primitive && ds->port == dest.port) {
                  log_assert(ds->primitive != nullptr);
                  log_assert(ds->primitive->ports.find(dest.port) !=
                             ds->primitive->ports.end());
                  if (ds->primitive->data_child == nullptr ||
                      (ds->primitive->data_child->fast_clock_driven_by.first ==
                           nullptr &&
                       ds->primitive->data_child->fast_clock_driven_by.second
                           .empty())) {
                    log_assert(cdt->fabric_ports.size() == 0);
                    if (cdt->dest_ports.size() == 0) {
                      POST_MSG(3, "%s", port->id().c_str());
                    }
                    // Update DRIVE TO info
                    cdt->dest_ports.push_back(
                        ds->primitive->ports.at(dest.port));
                    // Update DRIVEN BY info
                    log_assert(ds->primitive->fast_clock_driven_by.first ==
                               nullptr);
                    log_assert(
                        ds->primitive->fast_clock_driven_by.second.empty());
                    (const_cast<PRIMITIVE*>(ds->primitive))
                        ->fast_clock_driven_by =
                        std::make_pair(child, port->name);
                    POST_MSG(4, "-> %s (Low Priority Port: %s)",
                             ds->primitive->id().c_str(), ds->port.c_str());
                  }
                }
              }
            }
          }
          log_assert(cdt->fabric_ports.size() == 0);
          if (cdt->dest_ports.size()) {
            child->fast_clock_drive_to.push_back(cdt);
          } else {
            delete cdt;
          }
        }
      }
      child = child->clock_child;
    }
  }
  return m_netlist_status;
}

/*
  Function to summarize what primitive connectivity
*/
void PRIMITIVES_EXTRACTOR::summarize(const std::vector<PRIMITIVE*>& parents) {
  POST_MSG(1, "Summary");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);
  int max_in_object = 0;
  int max_out_object = 0;
  int max_in_trace = 0;
  int max_out_trace = 0;
  std::vector<const PRIMITIVE*> summarized;
  for (auto& parent : parents) {
    if (parent->dir == DIRECTION::OUT) {
      if (int(parent->linked_object.size()) > max_out_object) {
        max_out_object = int(parent->linked_object.size());
      }
    } else {
      if (int(parent->linked_object.size()) > max_in_object) {
        max_in_object = int(parent->linked_object.size());
      }
    }
  }
  for (auto& parent : parents) {
    std::string data_chain = parent->get_data_chain_info();
    std::string clock_chain = parent->get_clock_chain_info();
    if (parent->dir == DIRECTION::OUT) {
      if (int(data_chain.size()) > max_out_trace) {
        max_out_trace = int(data_chain.size());
      }
      if (int(clock_chain.size()) > max_out_trace) {
        max_out_trace = int(clock_chain.size());
      }
    } else {
      if (int(data_chain.size()) > max_in_trace) {
        max_in_trace = int(data_chain.size());
      }
      if (int(clock_chain.size()) > max_in_trace) {
        max_in_trace = int(clock_chain.size());
      }
    }
  }
  max_in_trace += 4;
  std::string dashes = "";
  std::string stars = "";
  while (int(dashes.size()) <
         (max_in_object + max_in_trace + max_out_trace + max_out_object + 8)) {
    dashes.push_back('-');
  }
  while (int(stars.size()) < (max_in_trace + max_out_trace + 4)) {
    stars.push_back('*');
  }
  POST_MSG(2, "    |%s|", dashes.c_str());
  POST_MSG(2, "    | %*s %s %*s |", max_in_object, "", stars.c_str(),
           max_out_object, "");
  for (auto& parent : parents) {
    if (std::find(summarized.begin(), summarized.end(), parent) ==
        summarized.end()) {
      std::string data_chain = parent->get_data_chain_info();
      std::string clock_chain = parent->get_clock_chain_info();
      bool input = parent->dir != DIRECTION::OUT;
      if (data_chain == clock_chain || data_chain.find(clock_chain) == 0 ||
          clock_chain.find(data_chain) == 0) {
        if (data_chain.find(clock_chain) == 0) {
          summarize(input, parent->linked_object, data_chain, max_in_object,
                    max_in_trace, max_out_trace, max_out_object);
        } else {
          summarize(input, parent->linked_object, clock_chain, max_in_object,
                    max_in_trace, max_out_trace, max_out_object);
        }
      } else {
        if (parent->data_child != nullptr && parent->clock_child != nullptr) {
          summarize(input, parent->linked_object, clock_chain, max_in_object,
                    max_in_trace, max_out_trace, max_out_object);
          size_t size = parent->type.size();
          data_chain = data_chain.erase(0, size);
          while (size) {
            data_chain.insert(0, " ");
            size--;
          }
          summarize(input, "", data_chain, max_in_object, max_in_trace,
                    max_out_trace, max_out_object, false);
        } else if (parent->data_child != nullptr) {
          summarize(input, parent->linked_object, data_chain, max_in_object,
                    max_in_trace, max_out_trace, max_out_object);
        } else if (parent->clock_child != nullptr) {
          summarize(input, parent->linked_object, clock_chain, max_in_object,
                    max_in_trace, max_out_trace, max_out_object);
        }
      }
      summarized.push_back(parent);
    }
  }
  POST_MSG(2, "    | %*s %s %*s |", max_in_object, "", stars.c_str(),
           max_out_object, "");
  POST_MSG(2, "    |%s|", dashes.c_str());
}

/*
  Function to summarize what primitive connectivity (recursive for children)
  This only calculate the string size
*/
void PRIMITIVES_EXTRACTOR::summarize(bool input, const std::string& object,
                                     const std::string& trace,
                                     int max_in_object, int max_in_trace,
                                     int max_out_trace, int max_out_object,
                                     bool print_dir) {
  std::string line = print_dir ? (input ? "IN " : "OUT") : "   ";
  if (input) {
    line = stringf("%s | %*s * %-*s%*s * %-*s |", line.c_str(), max_in_object,
                   object.c_str(), max_in_trace, trace.c_str(), max_out_trace,
                   "", max_out_object, "");

  } else {
    line = stringf("%s | %*s * %-*s%*s * %-*s |", line.c_str(), max_in_object,
                   "", max_in_trace, "", max_out_trace, trace.c_str(),
                   max_out_object, object.c_str());
  }
  POST_MSG(2, "%s", line.c_str());
}

/*
  Write out SDC and Clock Pin XML
*/
bool PRIMITIVES_EXTRACTOR::write_sdc(std::vector<PRIMITIVE*>& parents,
                                     std::vector<const PRIMITIVE*>& clocks,
                                     const std::string& sdc_file,
                                     const std::string& clk_pin_xml) {
  POST_MSG(2, "Write out SDC and Clock Pin XML");
  log_assert(m_basic_status);
  log_assert(m_netlist_status);

  // Step 1: Open files
  std::ofstream sdc(sdc_file.c_str());
  std::ofstream xml(clk_pin_xml.c_str());

  // Step 2: Fabric Clock
  write_sdc_core_clock(clocks, sdc, xml);

  // Step 3: Data
  write_sdc_data(parents, sdc);

  // Step 4: None SOC Control
  write_sdc_fabric_control_map(parents, sdc);

  //
  xml.close();
  sdc.close();
  return m_netlist_status;
}

/*
  Get mode the write to SDC
*/
std::string PRIMITIVES_EXTRACTOR::get_sdc_mode(const PRIMITIVE* primitive) {
  log_assert(primitive->yaml->group == "DATA");
  log_assert(primitive->parent == nullptr);
  log_assert(primitive->grandparent == nullptr);
  log_assert(primitive->dir == DIRECTION::IN ||
             primitive->dir == DIRECTION::OUT);
  log_assert(primitive->location.size());
  log_assert(primitive->parsed_location.type.size() > 0 &&
             primitive->parsed_location.bank.size() > 0 &&
             primitive->parsed_location.status &&
             primitive->parsed_location.failure_reason.size() == 0);
  std::string mode =
      stringf("%s_%c_%s",
              primitive->mode.size() ? primitive->mode.c_str() : "MODE_BP_DIR",
              (primitive->parsed_location.index & 1) ? 'B' : 'A',
              primitive->dir == DIRECTION::IN ? "RX" : "TX");
  return mode;
}

/*
  Write CORE/FABRIC clock to SDC
*/
void PRIMITIVES_EXTRACTOR::write_sdc_core_clock(
    std::vector<const PRIMITIVE*>& clocks, std::ofstream& sdc,
    std::ofstream& xml) {
  POST_MSG(3, "Write CORE/FABRIC clock");
  sdc << "#############\n";
  sdc << "#\n";
  sdc << "# CORE/FABRIC clock\n";
  sdc << "#\n";
  sdc << "#############\n";
  std::string fabric_clocks[MAX_CLOCK_SLOT];
  const std::map<std::string, std::string> SOC_CLOCK_MAP = {
      {"SOC_FPGA_INTF_DMA", "clk_fpga_fabric_dma"},
      {"SOC_FPGA_INTF_IRQ", "clk_fpga_fabric_irq"},
      {"SOC_FPGA_INTF_AHB_M", "clk_fpga_ahb"},
      {"SOC_FPGA_INTF_AHB_S", "clk_fpga_ahb"},
  };
  std::vector<SDC_ENTRY*> sdc_entries;
  for (auto& clock : clocks) {
    log_assert(clock->core_clock_drive_to.size());
    for (auto& cdt : clock->core_clock_drive_to) {
      log_assert(cdt->dest_ports.size() != 0 || cdt->fabric_ports.size() != 0);
      log_assert(cdt->slot < MAX_CLOCK_SLOT);
      SDC_ENTRY* entry = new SDC_ENTRY;
      int size = int(((std::string)("Primitive")).size());
      bool to_soc = false;
      for (auto& dest : cdt->dest_ports) {
        if (dest->primitive->yaml->group == "SOC") {
          to_soc = true;
        }
      }
      entry->comments.push_back(
          stringf("# %*s: %s", size, "Primitive", clock->type.c_str()));
      entry->comments.push_back(
          stringf("# %*s: %s", size, "Name", clock->name.c_str()));
      entry->comments.push_back(
          stringf("# %*s: %s", size, "Port", cdt->src_port.c_str()));
      entry->comments.push_back(stringf("# %*s: %d", size, "Slot", cdt->slot));
      if (clock->type == "FCLK_BUF") {
        log_assert(clock->core_clock_drive_to.size() == 1);
        entry->comments.push_back(
            stringf("# %*s: %s", size, "Note", "Need to route from FABRIC"));
      }
      if (cdt->fabric_ports.size()) {
        entry->comments.push_back(
            stringf("# %*s: %s", size, "Note", "Need to route to FABRIC"));
      }
      if (to_soc) {
        entry->comments.push_back(
            stringf("# %*s: %s", size, "Note", "Need to route to SOC"));
      }
      for (auto& dest : cdt->dest_ports) {
        entry->comments.push_back(
            stringf("# %*s-> %s", size, "", dest->id().c_str()));
      }
      for (auto& dest : cdt->fabric_ports) {
        entry->comments.push_back(
            stringf("# %*s-> Port %s (FABRIC)", size, "", dest.c_str()));
      }
      if (clock->type == "FCLK_BUF") {
        log_assert(clock->ports.find("I") != clock->ports.end());
        const PORT* port = clock->ports.at("I");
        log_assert(port->nets.size() == 1);
        log_assert(port->nets[0]->drive_sinks.size() == 1);
        const DRIVE_SINK* ds = port->nets[0]->drive_sinks[0];
        entry->assignments.push_back(SDC_ASSIGNMENT(
            "set_clock_out", "-device_clock",
            stringf("clk[%d]", cdt->fclk_buf_slot), "-design_clock", ds->port));
      }
      if (cdt->fabric_ports.size()) {
        log_assert(fabric_clocks[cdt->slot].empty());
        fabric_clocks[cdt->slot] = cdt->fabric_ports[0];
        entry->assignments.push_back(SDC_ASSIGNMENT(
            "set_clock_pin", "-device_clock", stringf("clk[%d]", cdt->slot),
            "-design_clock", cdt->fabric_ports[0]));
      }
      std::vector<std::string> mapped_soc;
      for (auto& dest : cdt->dest_ports) {
        if (dest->primitive->yaml->group == "SOC") {
          log_assert(SOC_CLOCK_MAP.find(dest->primitive->type) !=
                     SOC_CLOCK_MAP.end());
          std::string mapped_port = SOC_CLOCK_MAP.at(dest->primitive->type);
          if (std::find(mapped_soc.begin(), mapped_soc.end(), mapped_port) ==
              mapped_soc.end()) {
            entry->assignments.push_back(SDC_ASSIGNMENT(
                "set_soc_clk", mapped_port, std::to_string(cdt->slot)));
            mapped_soc.push_back(mapped_port);
          }
        }
      }
      sdc_entries.push_back(entry);
    }
  }
  if (sdc_entries.size() == 0) {
    sdc << "\n";
  }
  write_sdc_entries(sdc, sdc_entries);
  log_assert(sdc_entries.size() == 0);
  xml << "<pin_constraints>\n";
  for (int i = 0; i < MAX_CLOCK_SLOT; i++) {
    if (fabric_clocks[i].size()) {
      xml << stringf("  <set_io pin=\"clk[%d]\" net=\"%s\"/>\n", i,
                     fabric_clocks[i].c_str())
                 .c_str();
    } else {
      xml << stringf("  <set_io pin=\"clk[%d]\" net=\"OPEN\"/>\n", i).c_str();
    }
  }
  xml << "</pin_constraints>\n";
}

/*
  Write FABRIC data map to SDC
*/
void PRIMITIVES_EXTRACTOR::write_sdc_data(std::vector<PRIMITIVE*>& parents,
                                          std::ofstream& sdc) {
  POST_MSG(3, "Write FABRIC data");
  sdc << "#############\n";
  sdc << "#\n";
  sdc << "# FABRIC data\n";
  sdc << "#\n";
  sdc << "#############\n";
  std::vector<SDC_ENTRY*> sdc_entries;
  for (const auto& primitive : parents) {
    if (primitive->yaml->group == "DATA") {
      SDC_ENTRY* entry = new SDC_ENTRY;
      auto end_data = primitive->get_end_data_child();
      std::string chain = primitive->get_data_chain_info();
      POST_MSG(4, "%s", primitive->id().c_str());
      POST_MSG(5, "Location: %s", primitive->location.c_str());
      POST_MSG(5, "Mode: %s", primitive->mode.c_str());
      POST_MSG(5, "Chain: %s", chain.c_str());
      int size = int(((std::string)("Primitive")).size());
      entry->comments.push_back(
          stringf("# %*s: %s", size, "Primitive", primitive->type.c_str()));
      entry->comments.push_back(
          stringf("# %*s: %s", size, "Name", primitive->name.c_str()));
      entry->comments.push_back(
          stringf("# %*s: %s", size, "Location", primitive->location.c_str()));
      entry->comments.push_back(
          stringf("# %*s: %s", size, "Mode", primitive->mode.c_str()));
      entry->comments.push_back(
          stringf("# %*s: %s", size, "Chain", chain.c_str()));
      std::vector<const DRIVE_SINK*> fabrics;
      const void* path_ptr = search_valid_paths(
          &PATH_DB.at(m_technology).data_paths, end_data->type);
      if (path_ptr != nullptr) {
        const PATH* path = reinterpret_cast<const PATH*>(path_ptr);
        if (end_data->ports.find(path->src.port) != end_data->ports.end()) {
          const PORT* port = end_data->ports.at(path->src.port);
          for (auto& net : port->nets) {
            for (auto& ds : net->drive_sinks) {
              if (ds->type == "FABRIC") {
                fabrics.push_back(ds);
              }
            }
          }
        }
      }
      if (fabrics.size()) {
        if (primitive->location.size()) {
          log_assert(primitive->mode.size());
          log_assert(primitive->dir == DIRECTION::IN ||
                     primitive->dir == DIRECTION::OUT);
          std::string location = primitive->parsed_location.get_p_location();
          std::string mode = get_sdc_mode(primitive);
          int index = (primitive->parsed_location.index & 1) ? 5 : 0;
          for (auto& fabric : fabrics) {
            entry->assignments.push_back(SDC_ASSIGNMENT(
                "set_io", fabric->port, location, "-mode", mode,
                "-internal_pin",
                stringf("%s[%d]_A",
                        primitive->dir == DIRECTION::IN ? "g2f_rx_in"
                                                        : "f2g_tx_out",
                        index)));
            index++;
          }
        } else {
          POST_MSG(5, "Skip: Location is not available");
          entry->comments.push_back(
              stringf("# %*s: %s", size, "Skip", "Location is not available"));
        }
      } else {
        POST_MSG(5, "Skip: Does not connect to FABRIC");
        entry->comments.push_back(
            stringf("# %*s: %s", size, "Skip", "Does not connect to FABRIC"));
      }
      sdc_entries.push_back(entry);
    }
  }
  if (sdc_entries.size() == 0) {
    sdc << "\n";
  }
  write_sdc_entries(sdc, sdc_entries);
  log_assert(sdc_entries.size() == 0);
}

/*
  Write out Fabric control map (entry)
*/
void PRIMITIVES_EXTRACTOR::write_sdc_fabric_control_map(
    std::vector<PRIMITIVE*>& parents, std::ofstream& sdc) {
  POST_MSG(3, "Write FABRIC Control Map");
  sdc << "#############\n";
  sdc << "#\n";
  sdc << "# FABRIC Control Map\n";
  sdc << "#\n";
  sdc << "#############\n";
  std::vector<SDC_ENTRY*> sdc_entries;
  std::vector<const PRIMITIVE*> written_cells;
  std::map<std::string, const PORT*> used_ports;
  for (auto& parent : parents) {
    auto end_child = parent->get_end_data_child();
    while (end_child->parent != nullptr) {
      write_sdc_fabric_control_map(parent, end_child, written_cells,
                                   sdc_entries, used_ports);
      end_child = end_child->parent;
    }
    log_assert(end_child == parent);
    end_child = parent->get_end_clock_child();
    while (end_child->parent != nullptr) {
      write_sdc_fabric_control_map(parent, end_child, written_cells,
                                   sdc_entries, used_ports);
      end_child = end_child->parent;
    }
    log_assert(end_child == parent);
    write_sdc_fabric_control_map(parent, end_child, written_cells, sdc_entries,
                                 used_ports);
  }
  if (sdc_entries.size() == 0) {
    sdc << "\n";
  }
  write_sdc_entries(sdc, sdc_entries);
  log_assert(sdc_entries.size() == 0);
}

/*
  Write out Fabric control map (entry)
*/
void PRIMITIVES_EXTRACTOR::write_sdc_fabric_control_map(
    const PRIMITIVE* grandparent, const PRIMITIVE* primitive,
    std::vector<const PRIMITIVE*>& written_cells,
    std::vector<SDC_ENTRY*>& sdc_entries,
    std::map<std::string, const PORT*>& used_ports) {
  SDC_ENTRY* entry = new SDC_ENTRY;
  POST_MSG(4, "%s", primitive->id().c_str());
  POST_MSG(5, "Location: %s", grandparent->location.c_str());
  POST_MSG(5, "Mode: %s", grandparent->mode.c_str());
  int size = int(((std::string)("Primitive")).size());
  entry->comments.push_back(
      stringf("# %*s: %s", size, "Primitive", primitive->type.c_str()));
  entry->comments.push_back(
      stringf("# %*s: %s", size, "Name", primitive->name.c_str()));
  entry->comments.push_back(
      stringf("# %*s: %s", size, "Location", grandparent->location.c_str()));
  entry->comments.push_back(
      stringf("# %*s: %s", size, "Mode", grandparent->mode.c_str()));
  log_assert(std::find(written_cells.begin(), written_cells.end(), primitive) ==
             written_cells.end());
  written_cells.push_back(primitive);
  sdc_entries.push_back(entry);
  size -= 2;
  for (auto& iter : primitive->yaml->fabric_control_map) {
    entry = new SDC_ENTRY;
    std::string port_name = iter.first;
    std::string map = iter.second;
    POST_MSG(5, "Port: %s", port_name.c_str());
    POST_MSG(6, "Map: %s", map.c_str());
    entry->comments.push_back(
        stringf("### %*s: %s", size, "Port", port_name.c_str()));
    entry->comments.push_back(stringf("### %*s: %s", size, "Map", map.c_str()));
    if (primitive->ports.find(port_name) != primitive->ports.end()) {
      if (primitive->yaml->group == "SOC" || grandparent->location.size() > 0) {
        const PORT* port = primitive->ports.at(port_name);
        std::string location = "VCC_HP_AUX";
        std::string mode = "Mode_GPIO";
        if (primitive->yaml->group == "DATA" ||
            primitive->yaml->group == "STANDALONE_DATA") {
          mode = get_sdc_mode(grandparent);
          location = grandparent->location;
        } else if (primitive->yaml->group == "STANDALONE_FABRIC") {
          location = grandparent->location;
        } else {
          log_assert(primitive->yaml->group == "SOC");
        }
        POST_MSG(6, "Mode: %s", mode.c_str());
        entry->comments.push_back(
            stringf("### %*s: %s", size, "Mode", mode.c_str()));
        int index = 0;
        for (auto& net : port->nets) {
          log_assert(net->drive_sinks.size() == 1);
          const DRIVE_SINK* ds = net->drive_sinks[0];
          log_assert(ds != nullptr);
          log_assert(ds->type == "FABRIC");
          std::string internal_pin = stringf("%s[%d]", map.c_str(), index);
          if (primitive->yaml->group != "SOC") {
            if (port->nets.size() == 1) {
              internal_pin = map;
            }
            size_t ab = internal_pin.find("{A|B}");
            if (ab != std::string::npos) {
              internal_pin = internal_pin.replace(
                  ab, 5, grandparent->parsed_location.index & 1 ? "B" : "A");
            }
          }
          POST_MSG(7, "iPin: %s", internal_pin.c_str());
          entry->comments.push_back(
              stringf("#--> %*s: %s", size - 1, "iPin", internal_pin.c_str()));
          log_assert(internal_pin.find("{A|B}") == std::string::npos);
          std::string used_port =
              stringf("%s + %s", location.c_str(), internal_pin.c_str());
          if (used_ports.find(used_port) == used_ports.end()) {
            used_ports[used_port] = port;
            entry->assignments.push_back(
                SDC_ASSIGNMENT("set_io", ds->port, location, "-mode", mode,
                               "-internal_pin", internal_pin));
          } else {
            POST_MSG(7, "Skip: Had already been used by %s",
                     port->id().c_str());
            entry->comments.push_back(
                stringf("#---> %*s: Had already been used by %s", size - 2,
                        "Skip", used_ports.at(used_port)->id().c_str()));
          }
          index++;
        }
      } else {
        POST_MSG(6, "Skip: Location is not available");
        entry->comments.push_back(
            stringf("### %*s: %s", size, "Skip", "Location is not available"));
      }
    } else {
      POST_MSG(6, "Skip: Not used in design");
      entry->comments.push_back(
          stringf("### %*s: %s", size, "Skip", "Not used in design"));
    }
    sdc_entries.push_back(entry);
  }
}

/*
  Write out Fabric control map (entry)
*/
void PRIMITIVES_EXTRACTOR::write_json(std::vector<PRIMITIVE*>& parents,
                                      const std::string& config_json) {
  std::ofstream json(config_json.c_str());
  log_assert(m_status == (m_basic_status && m_netlist_status));
  size_t index = 0;
  json << "{\n";
  write_json_object_bool(json, 1, "status", m_status, true, true);
  json << "  \"error_messages\": [\n";
  for (auto& msg : m_errors) {
    index++;
    write_json_string(json, 2, msg, index == m_errors.size(), true);
    json.flush();
  }
  json << "  ],\n";
  json << "  \"messages\": [\n";
  write_json_string(json, 2, "Start of Extractor Analysis", true, true);
  for (auto& msg : m_msgs) {
    write_json_string(json, 2, gen_space_str(msg->offset) + msg->msg, true,
                      true);
    json.flush();
  }
  write_json_string(json, 2, "End of Extractor Analysis", false, true);
  json << "  ],\n";
  json << "  \"instances\": [";
  if (m_status && parents.size()) {
    std::vector<const PRIMITIVE*> written;
    json << "\n";
    for (auto& parent : parents) {
      write_json_primitive(json, parent, written);
      auto child = parent->clock_child;
      while (child != nullptr) {
        write_json_primitive(json, child, written);
        child = child->clock_child;
      }
      child = parent->data_child;
      while (child != nullptr) {
        write_json_primitive(json, child, written);
        child = child->data_child;
      }
    }
    log_assert(written.size() == m_primitives.size());
  }
  json << "\n  ]";
  json << "\n}\n";
  json.close();
}

/*
  Write out JSON boolean object
*/
std::string PRIMITIVES_EXTRACTOR::gen_space_str(uint32_t space) {
  std::string space_string = "";
  for (uint32_t i = 0; i < space; i++) {
    space_string += "  ";
  }
  return space_string;
}

/*
  Write out JSON chars
*/
void PRIMITIVES_EXTRACTOR::write_json_chars(std::ofstream& json,
                                            const std::string& str) {
  for (auto& c : str) {
    if (c == '\\') {
      json << '\\';
    } else if (c == '"') {
      json << '\\';
    }
    json << c;
  }
}

/*
  Write out JSON string
*/
void PRIMITIVES_EXTRACTOR::write_json_string(std::ofstream& json,
                                             uint32_t space,
                                             const std::string& str, bool comma,
                                             bool new_line) {
  json << gen_space_str(space).c_str();
  json << "\"";
  write_json_chars(json, str);
  json << "\"";
  json << (comma ? "," : "");
  json << (new_line ? "\n" : "");
}

/*
  Write out JSON boolean object
*/
void PRIMITIVES_EXTRACTOR::write_json_object_bool(std::ofstream& json,
                                                  uint32_t space,
                                                  const std::string& object,
                                                  bool status, bool comma,
                                                  bool new_line) {
  json << gen_space_str(space).c_str();
  json << "\"";
  write_json_chars(json, object);
  json << "\": ";
  json << (status ? "true" : "false");
  json << (comma ? "," : "");
  json << (new_line ? "\n" : "");
}

/*
  Write out JSON string object
*/
void PRIMITIVES_EXTRACTOR::write_json_object_str(std::ofstream& json,
                                                 uint32_t space,
                                                 const std::string& object,
                                                 const std::string& str,
                                                 bool comma, bool new_line) {
  json << gen_space_str(space).c_str();
  json << "\"";
  write_json_chars(json, object);
  json << "\": ";
  json << "\"";
  write_json_chars(json, str);
  json << "\"";
  json << (comma ? "," : "");
  json << (new_line ? "\n" : "");
}

/*
  Write out JSON dict object
*/
void PRIMITIVES_EXTRACTOR::write_json_object_vector(
    std::ofstream& json, uint32_t space, const std::string& object,
    const std::vector<std::string>& vec, bool comma, bool new_line) {
  json << gen_space_str(space).c_str();
  json << "\"";
  write_json_chars(json, object);
  json << "\": [\n";
  // Dict
  size_t index = 0;
  for (auto& v : vec) {
    index++;
    write_json_string(json, space + 1, v, index < vec.size(), true);
  }
  // End Dict
  json << gen_space_str(space).c_str();
  json << "]";
  json << (comma ? "," : "");
  json << (new_line ? "\n" : "");
}

/*
  Write out JSON dict object
*/
void PRIMITIVES_EXTRACTOR::write_json_object_dict(
    std::ofstream& json, uint32_t space, const std::string& object,
    const std::map<std::string, std::string>& dict1,
    const std::map<std::string, std::string>& dict2, bool comma,
    bool new_line) {
  json << gen_space_str(space).c_str();
  json << "\"";
  write_json_chars(json, object);
  json << "\": {\n";
  // Dict
  size_t index = 0;
  for (auto& dict : dict1) {
    index++;
    write_json_object_str(json, space + 1, dict.first, dict.second,
                          index < (dict1.size() + dict2.size()), true);
  }
  for (auto& dict : dict2) {
    index++;
    write_json_object_str(json, space + 1, dict.first, dict.second,
                          index < (dict1.size() + dict2.size()), true);
  }
  // End Dict
  json << gen_space_str(space).c_str();
  json << "}";
  json << (comma ? "," : "");
  json << (new_line ? "\n" : "");
}

/*
  Write out JSON PRIMITIVE
*/
void PRIMITIVES_EXTRACTOR::write_json_primitive(
    std::ofstream& json, const PRIMITIVE* primitive,
    std::vector<const PRIMITIVE*>& written_primitives) {
  log_assert(primitive != nullptr);
  if (std::find(written_primitives.begin(), written_primitives.end(),
                primitive) == written_primitives.end()) {
    size_t index = 0;
    std::map<std::string, std::string> dummy;
    std::vector<std::string> dummy_errors;
    const PRIMITIVE* parent = primitive->grandparent;
    if (parent == nullptr) {
      parent = primitive;
    }
    json << gen_space_str(2).c_str();
    json << "{\n";
    write_json_object_str(json, 3, "module", primitive->type, true, true);
    write_json_object_str(json, 3, "name", primitive->name, true, true);
    write_json_object_str(json, 3, "location_object", parent->location_object,
                          true, true);
    write_json_object_str(json, 3, "location", parent->location, true, true);
    write_json_object_str(json, 3, "linked_object", parent->linked_object, true,
                          true);
    // Objects
    write_json_primitive_objects(json, parent->location_object, parent->objects,
                                 primitive->parameters);
    json << ",\n";
    // End Objects
    // Params
    write_json_object_dict(json, 3, "parameters", primitive->parameters, dummy,
                           true, true);
    // End Params
    // Connectivity
    index = 0;
    json << gen_space_str(3).c_str();
    json << "\"connectivity\": {\n";
    for (auto& p : primitive->ports) {
      write_json_primitive_port(json, p.second);
      index++;
      if (index < primitive->ports.size()) {
        json << ",";
      }
      json << "\n";
    }
    json << gen_space_str(3).c_str();
    json << "},\n";
    // End Connectivity
    write_json_object_vector(json, 3, "flags", primitive->get_flags(), true,
                             true);
    write_json_object_str(json, 3, "pre_primitive",
                          primitive->get_pre_primitive(), true, true);
    write_json_object_vector(json, 3, "post_primitives",
                             primitive->get_post_primitives(), true, true);
    // Route Fast Clock
    write_json_primitive_clock_drive_to(json, primitive->fast_clock_drive_to);
    json << ",\n";
    // End Route Fast Clock
    write_json_object_vector(json, 3, "errors", dummy_errors, false, true);
    json << gen_space_str(2).c_str();
    json << "}";
    written_primitives.push_back(primitive);
    log_assert(written_primitives.size() <= m_primitives.size());
    if (written_primitives.size() < m_primitives.size()) {
      json << ",\n";
    }
  }
}

void PRIMITIVES_EXTRACTOR::write_json_primitive_objects(
    std::ofstream& json, const std::string& location_object,
    const std::map<std::string, OBJECT*>& objects,
    const std::map<std::string, std::string>& parameters) {
  log_assert(objects.find(location_object) != objects.end());
  std::vector<std::string> objs = {location_object};
  for (auto& o : objects) {
    if (std::find(objs.begin(), objs.end(), o.first) == objs.end()) {
      objs.push_back(o.first);
    }
  }
  size_t index = 0;
  json << gen_space_str(3).c_str();
  json << "\"linked_objects\": {\n";
  for (auto& o : objs) {
    // Object
    const OBJECT* object = objects.at(o);
    log_assert(object != nullptr);
    json << gen_space_str(4).c_str();
    json << "\"";
    json << o.c_str();
    json << "\": {\n";
    write_json_object_str(json, 5, "location", object->location, true, true);
    write_json_object_dict(json, 5, "properties", parameters,
                           object->properties, false, true);
    json << gen_space_str(4).c_str();
    json << "}";
    // End Object
    index++;
    if (index < objects.size()) {
      json << ",";
    }
    json << "\n";
  }
  json << gen_space_str(3).c_str();
  json << "}";
}

void PRIMITIVES_EXTRACTOR::write_json_primitive_port(std::ofstream& json,
                                                     const PORT* port) {
  log_assert(port != nullptr);
  size_t index = 0;
  json << gen_space_str(4).c_str();
  json << "\"";
  json << port->name.c_str();
  json << "\": [\n";
  for (auto& net : port->nets) {
    json << gen_space_str(5).c_str();
    json << "{\n";
    write_json_object_str(json, 6, "net", net->name, true, true);
    std::vector<std::string> vec;
    for (auto& ds : net->drive_sinks) {
      vec.push_back(get_port_id(ds));
    }
    write_json_object_vector(json, 6, "DriveSink", vec, false, true);
    json << gen_space_str(5).c_str();
    json << "}";
    index++;
    if (index < port->nets.size()) {
      json << ",";
    }
    json << "\n";
  }
  json << gen_space_str(4).c_str();
  json << "]";
}

void PRIMITIVES_EXTRACTOR::write_json_primitive_clock_drive_to(
    std::ofstream& json, const std::vector<const CLOCK_DRIVEN_TO*>& clocks) {
  size_t index = 0;
  json << gen_space_str(3).c_str();
  json << "\"route_clock_to\": {\n";
  for (auto& clock : clocks) {
    log_assert(clock->fabric_ports.size() == 0);
    log_assert(clock->dest_ports.size());
    std::vector<std::string> dests;
    for (auto& port : clock->dest_ports) {
      dests.push_back(port->primitive->name);
    }
    index++;
    write_json_object_vector(json, 4, clock->src_port, dests,
                             index < clocks.size(), true);
  }
  json << gen_space_str(3).c_str();
  json << "}";
}

/*
  Write string to the text output
*/
void PRIMITIVES_EXTRACTOR::file_write_string(std::string& line,
                                             const std::string& string,
                                             int size) {
  if (size == -1) {
    line += string;
  } else {
    line += stringf("%-*s", size, string.c_str());
  }
}

/*
  Write out SDC entries
*/
void PRIMITIVES_EXTRACTOR::write_sdc_entries(
    std::ofstream& sdc, std::vector<SDC_ENTRY*>& sdc_entries) {
  size_t col1 = 0;
  size_t col2 = 0;
  size_t col3 = 0;
  size_t col4 = 0;
  size_t col5 = 0;
  size_t col6 = 0;
  for (auto& entry : sdc_entries) {
    for (auto& assignment : entry->assignments) {
      if (assignment.str1.size() > col1) {
        col1 = assignment.str1.size();
      }
      if (assignment.str2.size() > col2) {
        col2 = assignment.str2.size();
      }
      if (assignment.str3.size() > col3) {
        col3 = assignment.str3.size();
      }
      if (assignment.str4.size() > col4) {
        col4 = assignment.str4.size();
      }
      if (assignment.str5.size() > col5) {
        col5 = assignment.str5.size();
      }
      if (assignment.str6.size() > col6) {
        col6 = assignment.str6.size();
      }
    }
  }
  for (auto& entry : sdc_entries) {
    for (auto& comment : entry->comments) {
      sdc << comment.c_str() << "\n";
    }
    for (auto& assignment : entry->assignments) {
      std::string line = "";
      file_write_string(line, assignment.str1, (int)(col1 + 1));
      file_write_string(line, assignment.str2, (int)(col2 + 1));
      file_write_string(line, assignment.str3, (int)(col3 + 1));
      file_write_string(line, assignment.str4, (int)(col4 + 1));
      file_write_string(line, assignment.str5, (int)(col5 + 1));
      file_write_string(line, assignment.str6, (int)(col6 + 1));
      file_write_string(line, assignment.str7);
      while (line.size() > 0 && line.back() == ' ') {
        line.pop_back();
      }
      sdc << line.c_str() << "\n";
    }
    sdc << "\n";
  }
  while (sdc_entries.size()) {
    delete sdc_entries.back();
    sdc_entries.pop_back();
  }
}

/*
  Get Drive Sink(s) ID
*/
std::string PRIMITIVES_EXTRACTOR::get_drive_sinks_id(
    const std::vector<DRIVE_SINK*> dss) {
  std::string id = "";
  for (auto& ds : dss) {
    if (id.size()) {
      id = stringf("%s, %s", id.c_str(), ds->id().c_str());
    } else {
      id = ds->id();
    }
  }
  return id;
}

/*
  Get Net(s) ID
*/
std::string PRIMITIVES_EXTRACTOR::get_nets_id(const std::vector<NET*> nets) {
  std::string id = "";
  for (auto& net : nets) {
    if (id.size()) {
      id = stringf("%s, %s", id.c_str(), net->id().c_str());
    } else {
      id = net->id();
    }
  }
  return id;
}

/*
  Get Port ID
*/
std::string PRIMITIVES_EXTRACTOR::get_port_id(const DRIVE_SINK* ds) {
  if (ds->primitive != nullptr) {
    return stringf("Port %s (Primitive=%s, Name=%s)", ds->port.c_str(),
                   ds->type.c_str(), ds->name.c_str());
  }
  return stringf("Port %s (Primitive=%s)", ds->port.c_str(), ds->type.c_str());
}
