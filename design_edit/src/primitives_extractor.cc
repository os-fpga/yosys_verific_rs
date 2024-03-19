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
  This piece of code extract important information from RTLIL::Design class
  directly. These important information includes:
    a. I_BUF
    b. CLK_BUF
    c. O_BUF

    and more when other use cases are understood

  Currently supported use cases are:
    a. normal input port:  I_BUF
    b. clock port:         I_BUF -> CLK_BUF
    c. normal output port: O_BUF
*/
/*
  Author: Chai, Chung Shien
*/

#include "primitives_extractor.h"

#include "backends/rtlil/rtlil_backend.h"
#include "kernel/celltypes.h"
#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/sigtools.h"

USING_YOSYS_NAMESPACE

#define POST_MSG(space, ...) \
  { post_msg(space, stringf(__VA_ARGS__)); }

/*
  Get rid the first character if it is '\\'
*/
std::string get_original_name(const std::string& name) {
  if (name.size() > 0 && name[0] == '\\') {
    return name.substr(1);
  }
  return name;
}

/*
  Get rid the first and last character if they are '"'
*/
std::string get_param_string(const std::string& str) {
  if (str.size() >= 2 && str[0] == '"' && str[str.size() - 1] == '"') {
    return str.substr(1, str.size() - 2);
  }
  return str;
}

/*
  Structure that store message
*/
struct MSG {
  MSG(uint32_t o, const std::string& m) : offset(o), msg(m) {
#if 0
    printf("DEBUG: ");
    for (uint32_t i = 0; i < offset; i++) {
      printf("  ");
    }
    printf("%s\n", msg.c_str());
#endif
  }
  const uint32_t offset = 0;
  const std::string msg = "";
};

/*
  Structure that database of supported primitive
*/
struct PRIMITIVE_DB {
  PRIMITIVE_DB(const std::string& n, bool i, IO_DIR d,
               std::vector<std::string> is, std::vector<std::string> os,
               const std::string& it, const std::string& ot)
      : name(n),
        is_port(i),
        dir(d),
        inputs(is),
        outputs(os),
        intrace_connection(it),
        outtrace_connection(ot) {
    log_assert(dir == IO_DIR::IN || dir == IO_DIR::OUT);
  }
  std::vector<std::string> get_checking_ports() const {
    if (dir == IO_DIR::IN) {
      return inputs;
    }
    return outputs;
  }
  const std::string name = "";
  const bool is_port = false;
  const IO_DIR dir = IO_DIR::UNKNOWN;
  const std::vector<std::string> inputs;
  const std::vector<std::string> outputs;
  const std::string intrace_connection = "";
  const std::string outtrace_connection = "";
};

/*
  Supported primitives
*/
const std::map<std::string, std::vector<PRIMITIVE_DB>> SUPPORTED_PRIMITIVES = {
    {"genesis3",
     {{PRIMITIVE_DB("\\I_BUF", true, IO_DIR::IN, {"\\I"}, {"\\O"}, "\\I",
                    "\\O")},
      {PRIMITIVE_DB("\\CLK_BUF", false, IO_DIR::IN, {"\\I"}, {"\\O"}, "\\I",
                    "\\O")},
      {PRIMITIVE_DB("\\O_BUF", true, IO_DIR::OUT, {"\\I"}, {"\\O"}, "\\O",
                    "\\I")}}}};

/*
  Base structure of primitive
*/
struct PRIMITIVE {
  PRIMITIVE(const PRIMITIVE_DB* d, const std::string& n, PRIMITIVE* p,
            std::map<std::string, std::string> c, bool i)
      : db(d), name(n), parent(p), connections(c), is_port(i) {
    log_assert(db != nullptr);
  }
  std::string get_intrace_connection() const {
    log_assert(connections.find(db->intrace_connection) != connections.end());
    return connections.at(db->intrace_connection);
  }
  std::string get_outtrace_connection() const {
    log_assert(connections.find(db->outtrace_connection) != connections.end());
    return connections.at(db->outtrace_connection);
  }
  // Constructor
  const PRIMITIVE_DB* db = nullptr;
  const std::string name = "";
  const PRIMITIVE* parent = nullptr;
  const std::map<std::string, std::string> connections;
  const bool is_port = false;
  std::map<std::string, std::string> parameters;
  std::map<std::string, PRIMITIVE*> child;
  std::map<std::string, std::vector<std::string>> child_connections;
};

/*
  Structure of port primitive (derived from PRIMITIVE)
*/
struct PORT_PRIMITIVE : PRIMITIVE {
  PORT_PRIMITIVE(const PRIMITIVE_DB* db, const std::string& p,
                 std::map<std::string, std::string> c, IO_DIR d,
                 const std::string& pn, const std::string& pf,
                 const std::string& pr, int oidx, uint32_t idx, uint32_t w)
      : PRIMITIVE(db, p, nullptr, c, true),
        dir(d),
        port_name(pn),
        port_fullname(pf),
        port_realname(pr),
        offset_index(oidx),
        index(idx),
        width(w) {}
  // Constructor
  const IO_DIR dir = UNKNOWN;
  const std::string port_name = "";
  const std::string port_fullname = "";
  const std::string port_realname = "";
  const int offset_index = 0;
  const int index = 0;
  const uint32_t width = 0;
};

/*
  Structure of instance that dumped into JSON
*/
struct INSTANCE {
  INSTANCE(const std::string& m, const std::string& n, const std::string& l,
           const PRIMITIVE* p)
      : module(get_original_name(m)),
        name(get_original_name(n)),
        linked_object(get_original_name(l)),
        primitive(p) {}
  void add_connections(const std::map<std::string, std::string>& cs) {
    for (auto& iter : cs) {
      connections[get_original_name(iter.first)] =
          get_original_name(iter.second);
    }
  }
  void add_parameters(const std::map<std::string, std::string>& ps) {
    for (auto& iter : ps) {
      parameters[get_original_name(iter.first)] = get_param_string(iter.second);
    }
  }
  const std::string module = "";
  const std::string name = "";
  const std::string linked_object = "";
  const PRIMITIVE* primitive = nullptr;
  std::map<std::string, std::string> connections;
  std::map<std::string, std::string> parameters;
  std::string location = "";
  std::map<std::string, std::string> properties;
};

/*
  Extractor constructor
*/
PRIMITIVES_EXTRACTOR::PRIMITIVES_EXTRACTOR(const std::string& technology)
    : m_technology(technology) {
  log_assert(SUPPORTED_PRIMITIVES.find(m_technology) !=
             SUPPORTED_PRIMITIVES.end());
}

/*
  Extractor destructor
*/
PRIMITIVES_EXTRACTOR::~PRIMITIVES_EXTRACTOR() {
  while (m_msgs.size()) {
    delete m_msgs.back();
    m_msgs.pop_back();
  }
  while (m_ports.size()) {
    delete m_ports.back();
    m_ports.pop_back();
  }
  while (m_child_primitives.size()) {
    delete m_child_primitives.back();
    m_child_primitives.pop_back();
  }
  while (m_instances.size()) {
    delete m_instances.back();
    m_instances.pop_back();
  }
}

/*
  Entry point of EXTRACTOR to extract
*/
bool PRIMITIVES_EXTRACTOR::extract(RTLIL::Design* design) {
  // Step 1: Get Input and Output ports
  if (!get_ports(design->top_module())) {
    goto EXTRACT_END;
  }

  // Step 2: Trace CLK_BUF connection
  trace_clk_buf(design->top_module());

  // Step 3: Support more primitive once more use cases are understood

  // Lastly generate instance(s)
  if (m_status) {
    gen_instances();
  }

EXTRACT_END:

  return m_status;
}

/*
  Store the message
*/
void PRIMITIVES_EXTRACTOR::post_msg(uint32_t offset, const std::string& msg) {
  m_msgs.push_back(new MSG(offset, msg));
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
  Get the Input and Output ports
*/
bool PRIMITIVES_EXTRACTOR::get_ports(RTLIL::Module* module) {
  log_assert(m_ports.size() == 0);
  log_assert(m_status);
  POST_MSG(1, "Get Ports");
  for (const RTLIL::Wire* wire : module->wires()) {
    IO_DIR dir = IO_DIR::UNKNOWN;
    if (wire->port_input && !wire->port_output) {
      dir = IO_DIR::IN;
    } else if (!wire->port_input && wire->port_output) {
      dir = IO_DIR::OUT;
    } else if (wire->port_input && wire->port_output) {
      dir = IO_DIR::INOUT;
    }
    if (dir == IO_DIR::IN || dir == IO_DIR::OUT) {
      POST_MSG(
          2, "%s: %s (offset: %d, width: %d)",
          dir == IO_DIR::IN ? "IN" : (dir == IO_DIR::OUT ? "OUT" : "InOut"),
          wire->name.c_str(), wire->start_offset, wire->width);
      for (int index = 0; index < wire->width && m_status; index++) {
        std::string port_name = wire->name.str();
        std::string port_fullname = wire->name.str();
        std::string port_realname = wire->name.str();
        if (wire->width > 1) {
          port_fullname = stringf("%s [%d]", wire->name.c_str(), index);
          port_realname = stringf("%s [%d]", wire->name.c_str(),
                                  wire->start_offset + index);
        }
        m_status = trace_and_create_port(
            module, dir, port_name, port_fullname, port_realname,
            wire->start_offset + index, index, (uint32_t)(wire->width));
      }
    } else if (dir == IO_DIR::INOUT) {
      POST_MSG(2, "Warning: Need to understand how to handle INOUT %s",
               wire->name.c_str());
    }
    if (!m_status) {
      break;
    }
  }
  if (m_ports.size() == 0) {
    m_status = false;
    POST_MSG(2, "Error: Fail to detect any port");
  }
  return m_status;
}

/*
  Check if the primitive is supported
*/
const PRIMITIVE_DB* PRIMITIVES_EXTRACTOR::is_supported_primitive(
    const std::string& name) {
  const PRIMITIVE_DB* db = nullptr;
  for (auto& d : SUPPORTED_PRIMITIVES.at(m_technology)) {
    if (d.name == name) {
      db = &d;
      break;
    }
  }
  return db;
}

/*
  Extract the parameter(s)
*/
void PRIMITIVES_EXTRACTOR::get_primitive_parameters(Yosys::RTLIL::Cell* cell,
                                                    PRIMITIVE* primitive) {
  for (auto& it : cell->parameters) {
    std::ostringstream parameter;
    RTLIL_BACKEND::dump_const(parameter, it.second);
    primitive->parameters[it.first.str()] = parameter.str();
  }
  for (auto& it : primitive->parameters) {
    POST_MSG(4, "Parameter %s: %s", it.first.c_str(), it.second.c_str());
  }
}

/*
  Check if the cell is the one connected to the connection we are looking for
*/
std::map<std::string, std::string> PRIMITIVES_EXTRACTOR::is_connected_cell(
    Yosys::RTLIL::Cell* cell, const PRIMITIVE_DB* db,
    const std::string& connection) {
  log_assert(cell != nullptr);
  log_assert(db != nullptr);
  log_assert(cell->type.str() == db->name);
  size_t total_expected_connections = db->inputs.size() + db->outputs.size();
  log_assert(total_expected_connections);
  size_t total_connections = 0;
  std::map<std::string, std::string> connections;
  for (auto& it : cell->connections()) {
    if (std::find(db->inputs.begin(), db->inputs.end(), it.first.str()) !=
            db->inputs.end() ||
        std::find(db->outputs.begin(), db->outputs.end(), it.first.str()) !=
            db->outputs.end()) {
      std::ostringstream wire;
      RTLIL_BACKEND::dump_sigspec(wire, it.second);
      connections[it.first.str()] = wire.str();
      total_connections++;
    }
  }
  if (total_expected_connections == total_connections) {
    bool found = false;
    for (auto& key : db->get_checking_ports()) {
      if (connections.at(key) == connection) {
        found = true;
        break;
      }
    }
    if (!found) {
      connections.clear();
    }
  } else {
    connections.clear();
  }
  return connections;
}

/*
  Trace and Input/Output Port
*/
bool PRIMITIVES_EXTRACTOR::trace_and_create_port(
    RTLIL::Module* module, IO_DIR dir, const std::string& port_name,
    const std::string& port_fullname, const std::string& port_realname,
    int oindex, int index, uint32_t width) {
  bool found = false;
  std::string primitive_name = "";
  for (auto cell : module->cells()) {
    const PRIMITIVE_DB* db = is_supported_primitive(cell->type.str());
    if (db != nullptr && db->dir == dir) {
      std::map<std::string, std::string> connections =
          is_connected_cell(cell, db, port_fullname);
      if (connections.size()) {
        POST_MSG(3, "Connected %s to %s (%s)", port_fullname.c_str(),
                 db->name.c_str(), cell->name.c_str());
        m_ports.push_back(new PORT_PRIMITIVE(
            db, cell->name.str(), connections, dir, port_name, port_fullname,
            port_realname, oindex, index, width));
        get_primitive_parameters(cell, (PRIMITIVE*)(m_ports.back()));
        found = true;
        for (auto& it : cell->parameters) {
          std::ostringstream parameter;
          RTLIL_BACKEND::dump_const(parameter, it.second);
          m_ports.back()->parameters[it.first.str()] = parameter.str();
        }
        break;
      }
    }
  }
  if (!found) {
    POST_MSG(3, "Error: Fail to trace this port");
  }
  return found;
}

/*
  Trace clock buffer
*/
void PRIMITIVES_EXTRACTOR::trace_clk_buf(RTLIL::Module* module) {
  POST_MSG(1, "Trace Clock Buffer");
  for (PORT_PRIMITIVE*& port : m_ports) {
    if (port->dir == IO_DIR::IN) {
      POST_MSG(2, "IN Port: %s", port->port_fullname.c_str());
      PRIMITIVE* primitive = (PRIMITIVE*)(port);
      std::string trace_connection = port->get_outtrace_connection();
      bool found = trace_next_primitive(module, "\\CLK_BUF", primitive,
                                        trace_connection);
      if (found) {
        for (auto& a : port->child_connections["\\CLK_BUF"]) {
          POST_MSG(4, "Additional Connection: %s", a.c_str());
        }
      } else {
        remove_msg();
      }
    }
  }
}

/*
  Helper function to trace generic primitive (normally internal not directly
  connected to port)
*/
bool PRIMITIVES_EXTRACTOR::trace_next_primitive(Yosys::RTLIL::Module* module,
                                                const std::string& module_name,
                                                PRIMITIVE*& parent,
                                                const std::string& connection) {
  log_assert(parent->child.find(module_name) == parent->child.end());
  const PRIMITIVE_DB* db = is_supported_primitive(module_name);
  bool found = false;
  for (auto cell : module->cells()) {
    if (cell->type.str() == db->name) {
      std::map<std::string, std::string> connections =
          is_connected_cell(cell, db, connection);
      if (connections.size()) {
        POST_MSG(3, "Connected %s", cell->name.c_str());
        m_child_primitives.push_back(
            new PRIMITIVE(db, cell->name.str(), parent, connections, false));
        parent->child[module_name] = m_child_primitives.back();
        get_primitive_parameters(cell, m_child_primitives.back());
        found = true;
        break;
      }
    }
  }
  if (!found) {
    for (auto it : module->connections()) {
      std::ostringstream left;
      std::ostringstream right;
      RTLIL_BACKEND::dump_sigspec(left, it.first);
      RTLIL_BACKEND::dump_sigspec(right, it.second);
      if (right.str() == connection) {
        found = trace_next_primitive(module, module_name, parent, left.str());
        if (found) {
          if (parent->child_connections.find(module_name) ==
              parent->child_connections.end()) {
            parent->child_connections[module_name] = {};
          }
          parent->child_connections[module_name].insert(
              parent->child_connections[module_name].begin(), left.str());
          break;
        }
      }
    }
  }
  return found;
}

/*
  Generate instances that being used in JSON
*/
void PRIMITIVES_EXTRACTOR::gen_instances() {
  log_assert(m_status);
  log_assert(m_instances.size() == 0);
  for (PORT_PRIMITIVE*& port : m_ports) {
    PRIMITIVE* primitive = (PRIMITIVE*)(port);
    gen_instance(port->port_realname, primitive);
    for (auto child : port->child) {
      gen_wire(port, child.first);
      gen_instance(port->port_realname, child.second);
    }
  }
}

/*
  Generate instance that being used in JSON
*/
void PRIMITIVES_EXTRACTOR::gen_instance(const std::string& linked_object,
                                        const PRIMITIVE* primitive) {
  m_instances.push_back(new INSTANCE(primitive->db->name, primitive->name,
                                     linked_object, primitive));
  m_instances.back()->add_connections(primitive->connections);
  m_instances.back()->add_parameters(primitive->parameters);
}

/*
  Generate wire that connecting primitives
*/
void PRIMITIVES_EXTRACTOR::gen_wire(const PORT_PRIMITIVE* port,
                                    const std::string& child) {
  log_assert(port->child.find(child) != port->child.end());
  if (port->child_connections.find(child) != port->child_connections.end()) {
    uint32_t index = 0;
    std::string trace_connection = port->get_outtrace_connection();
    for (auto& wire : port->child_connections.at(child)) {
      std::string primitive_name =
          stringf("AUTO_%s_%s_#%d", get_original_name(child).c_str(),
                  get_original_name(port->port_realname).c_str(), index);
      m_instances.push_back(
          new INSTANCE("WIRE", primitive_name, port->port_realname, nullptr));
      m_instances.back()->add_connections(
          {{"I", trace_connection}, {"O", wire}});
      trace_connection = wire;
    }
  }
}

/*
  Assign the location
*/
void PRIMITIVES_EXTRACTOR::assign_location(
    const std::string& port, const std::string& location,
    std::unordered_map<std::string, std::string>& properties) {
  POST_MSG(1, "Assign location %s (and properties) to Port %s",
           location.c_str(), port.c_str());
  for (auto& instance : m_instances) {
    if (instance->linked_object == port) {
      instance->location = location;
      if (instance->primitive != nullptr && instance->primitive->is_port) {
        for (auto& iter : properties) {
          instance->properties[iter.first] = iter.second;
        }
      }
    }
  }
}

/*
  Write out message and instances information into JSON
*/
void PRIMITIVES_EXTRACTOR::write_json(const std::string& file) {
  std::ofstream json(file.c_str());
  json << "{\n  \"messages\" : [\n";
  json << "    \"Start of IO Analysis\",\n";
  for (auto& msg : m_msgs) {
    json << "    \"";
    for (uint32_t i = 0; i < msg->offset; i++) {
      json << "  ";
    }
    write_json_data(msg->msg, json);
    json << "\",\n";
    json.flush();
  }
  json << "    \"End of IO Analysis\"\n  ]";
  if (m_status && m_instances.size() > 0) {
    json << ",\n  \"instances\" : [";
    size_t index = 0;
    for (auto& instance : m_instances) {
      if (index) {
        json << ",";
      }
      write_instance(instance, json);
      json.flush();
      index++;
    }
    json << "\n  ]";
  } else {
    json << ",\n  \"instances\" : [";
    json << "\n  ]";
  }
  json << "\n}\n";
  json.close();
}

/*
  Write out instance information into JSON
*/
void PRIMITIVES_EXTRACTOR::write_instance(const INSTANCE* instance,
                                          std::ofstream& json) {
  json << "\n    {\n";
  write_json_object(3, "module", instance->module, json);
  json << ",\n";
  write_json_object(3, "name", instance->name, json);
  json << ",\n";
  write_json_object(3, "linked_object", instance->linked_object, json);
  json << ",\n";
  write_json_object(3, "location", instance->location, json);
  json << ",\n";
  json << "      \"connectivity\" : {\n";
  write_instance_map(instance->connections, json);
  json << "      },\n";
  json << "      \"parameters\" : {\n";
  write_instance_map(instance->parameters, json);
  json << "      },\n";
  json << "      \"properties\" : {\n";
  write_instance_map(instance->properties, json);
  json << "      }\n";
  json << "    }";
}

/*
  Write out std::map information into JSON
*/
void PRIMITIVES_EXTRACTOR::write_instance_map(
    std::map<std::string, std::string> map, std::ofstream& json) {
  size_t index = 0;
  for (auto& iter : map) {
    if (index) {
      json << ",\n";
    }
    write_json_object(4, iter.first, iter.second, json);
    index++;
  }
  if (index) {
    json << "\n";
  }
}

/*
  Write out JSON dictionary key and value into JSON
*/
void PRIMITIVES_EXTRACTOR::write_json_object(uint32_t space,
                                             const std::string& key,
                                             const std::string& value,
                                             std::ofstream& json) {
  while (space) {
    json << "  ";
    space--;
  }
  json << "\"";
  write_json_data(key, json);
  json << "\"";
  json << " : ";
  json << "\"";
  write_json_data(value, json);
  json << "\"";
}

/*
  Write string into JSON with handling of special characters
*/
void PRIMITIVES_EXTRACTOR::write_json_data(const std::string& str,
                                           std::ofstream& json) {
  for (auto& c : str) {
    if (c == '\\') {
      json << '\\';
    } else if (c == '"') {
      json << '\\';
    }
    json << c;
  }
}
