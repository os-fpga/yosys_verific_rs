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
    a. I_BUF    [connected to PORT]
    b. O_BUF    [connected to PORT]
    c. O_BUFT   [connected to PORT]
    c. CLK_BUF  [connected internally]
    d. I_DDR    [connected internally]
    e. O_DDR    [connected internally]
    f. I_DELAY  [connected internally]
    g. O_DELAY  [connected internally]

    and more when other use cases are understood

  Currently supported use cases are:
    a. normal input port:   I_BUF
    b. clock port:          I_BUF -> CLK_BUF
    c. normal output port:  O_BUF/O_BUFT
    d. DDR input:           I_BUF -> I_DDR (become two bits)
                            I_DELAY -> I_DDR (become two bits)
    e. DDR output:          (from two bits) O_DDR -> O_BUF
                            (become two bits) O_DDR --> O_DELAY
    f. I_DELAY:             I_BUF -> I_DELAY
    g. O_DELAY:             O_DELAY -> O_BUF
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

#define ENABLE_DEBUG_MSG (0)

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
  Get IO_DIR string
*/
std::string get_dir_name(IO_DIR dir, uint8_t cap = 0) {
  if (dir == IO_DIR::IN) {
    return cap == 0 ? "input" : (cap == 1 ? "Input" : "INPUT");
  } else if (dir == IO_DIR::OUT) {
    return cap == 0 ? "output" : (cap == 1 ? "Output" : "OUTPUT");
  } else if (dir == IO_DIR::INOUT) {
    return cap == 0 ? "inout" : (cap == 1 ? "Inout" : "INOUT");
  } else {
    return cap == 0 ? "unknown" : (cap == 1 ? "Unknown" : "UNKNOWN");
  }
}

/*
  Structure that store message
*/
struct MSG {
  MSG(uint32_t o, const std::string& m) : offset(o), msg(m) {
#if ENABLE_DEBUG_MSG
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
  Structure that store database of supported primitive
*/
struct PRIMITIVE_DB {
  PRIMITIVE_DB(const std::string& n, bool r, bool i, IO_DIR d,
               std::vector<std::string> is, std::vector<std::string> os,
               const std::string& it, const std::string& ot)
      : name(n),
        ready(r),
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
  const bool ready = false;
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
     // These are Port Primitive, they are directly connected to the
     // PIN/PORT/PAD
     // Inputs
     {{PRIMITIVE_DB("\\I_BUF", true, true, IO_DIR::IN, {"\\I"}, {"\\O"}, "\\I",
                    "\\O")},
      {PRIMITIVE_DB("\\I_BUF_DS", false, true, IO_DIR::IN, {"\\I_P", "\\I_N"},
                    {"\\O"}, "", "\\O")},
      // Output
      {PRIMITIVE_DB("\\O_BUF", true, true, IO_DIR::OUT, {"\\I"}, {"\\O"}, "\\O",
                    "\\I")},
      {PRIMITIVE_DB("\\O_BUFT", true, true, IO_DIR::OUT, {"\\I"}, {"\\O"},
                    "\\O", "\\I")},
      {PRIMITIVE_DB("\\O_BUF_DS", false, true, IO_DIR::OUT, {"\\I"},
                    {"\\O_P", "\\O_N"}, "", "\\I")},
      {PRIMITIVE_DB("\\O_BUFT_DS", false, true, IO_DIR::OUT, {"\\I"},
                    {"\\O_P", "\\O_N"}, "", "\\I")},
      // These are none-Port Primitive
      // In direction
      {PRIMITIVE_DB("\\CLK_BUF", true, false, IO_DIR::IN, {"\\I"}, {"\\O"},
                    "\\I", "\\O")},
      {PRIMITIVE_DB("\\I_DELAY", true, false, IO_DIR::IN, {"\\I"}, {"\\O"},
                    "\\I", "\\O")},
      {PRIMITIVE_DB("\\I_DDR", true, false, IO_DIR::IN, {"\\D"}, {}, "\\D",
                    "")},
      // Out direction
      {PRIMITIVE_DB("\\O_DELAY", true, false, IO_DIR::OUT, {"\\I"}, {"\\O"},
                    "\\O", "\\I")},
      {PRIMITIVE_DB("\\O_DDR", true, false, IO_DIR::OUT, {}, {"\\Q"}, "\\Q",
                    "")}}}};

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
                 std::map<std::string, std::string> c,
                 std::vector<PORT_INFO> ps)
      : PRIMITIVE(db, p, nullptr, c, true),
        port_infos(ps),
        dir(ps.size() ? ps[0].dir : IO_DIR::UNKNOWN) {
    log_assert(port_infos.size());
    log_assert(dir == IO_DIR::IN || dir == IO_DIR::OUT);
    for (auto port : port_infos) {
      log_assert(dir == port.dir);
    }
  }
  std::string linked_object() const {
    std::string name = "";
    for (auto port : port_infos) {
      name = stringf("%s+%s", name.c_str(),
                     get_original_name(port.realname).c_str());
    }
    name.erase(0, 1);
    return name;
  }
  std::vector<std::string> linked_objects() const {
    std::vector<std::string> names;
    for (auto port : port_infos) {
      names.push_back(get_original_name(port.realname));
    }
    return names;
  }
  // Constructor
  const std::vector<PORT_INFO> port_infos;
  const IO_DIR dir = IO_DIR::UNKNOWN;
};

/*
  Structure of instance that dumped into JSON
*/
struct INSTANCE {
  INSTANCE(const std::string& m, const std::string& n,
           std::vector<std::string> ls, const PRIMITIVE* p)
      : module(get_original_name(m)),
        name(get_original_name(n)),
        linked_objects(ls),
        primitive(p) {
    log_assert(linked_objects.size());
    for (auto o : linked_objects) {
      properties[o] = {};
      locations[o] = "";
    }
  }
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
  std::string linked_object() const {
    std::string name = "";
    for (auto o : linked_objects) {
      name = stringf("%s+%s", name.c_str(), o.c_str());
    }
    name.erase(0, 1);
    return name;
  }
  const std::string module = "";
  const std::string name = "";
  const std::vector<std::string> linked_objects;
  const PRIMITIVE* primitive = nullptr;
  std::map<std::string, std::string> connections;
  std::map<std::string, std::string> parameters;
  std::map<std::string, std::string> locations;
  std::map<std::string, std::map<std::string, std::string>> properties;
};

/*
  Extractor constructor
*/
PRIMITIVES_EXTRACTOR::PRIMITIVES_EXTRACTOR(const std::string& technology)
    : m_technology(technology) {
  if (SUPPORTED_PRIMITIVES.find(m_technology) == SUPPORTED_PRIMITIVES.end()) {
    m_status = false;
    POST_MSG(1, "Error: Technology %s is not supported", m_technology.c_str());
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
  // Step 1: Make sure the technology is supported (check in constructor)
  if (!m_status) {
    goto EXTRACT_END;
  }

  // Step 2: Get Input and Output ports
  if (!get_ports(design->top_module())) {
    goto EXTRACT_END;
  }

  // Step 3: Trace CLK_BUF connection
  trace_next_primitive(design->top_module(), "\\I_BUF", "\\CLK_BUF");

  // Step 5: Trace I_DELAY connection
  trace_next_primitive(design->top_module(), "\\I_BUF", "\\I_DELAY");

  // Step 6: Trace I_DDR connection
  trace_next_primitive(design->top_module(), "\\I_DELAY", "\\I_DDR");
  trace_next_primitive(design->top_module(), "\\I_BUF", "\\I_DDR");

  // Step 7: Trace O_DELAY connection
  trace_next_primitive(design->top_module(), "\\O_BUF", "\\O_DELAY");

  // Step 8: Trace O_DDR connection
  trace_next_primitive(design->top_module(), "\\O_DELAY", "\\O_DDR");
  trace_next_primitive(design->top_module(), "\\O_BUF", "\\O_DDR");

  // Step 9: Support more primitive once more use cases are understood

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
bool PRIMITIVES_EXTRACTOR::get_ports(Yosys::RTLIL::Module* module) {
  log_assert(m_ports.size() == 0);
  log_assert(m_status);
  POST_MSG(1, "Get Ports");
  std::vector<PORT_INFO> port_infos;
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
      for (int index = 0; index < wire->width; index++) {
        std::string port_name = wire->name.str();
        std::string port_fullname = wire->name.str();
        std::string port_realname = wire->name.str();
        if (wire->width > 1) {
          port_fullname = stringf("%s[%d]", wire->name.c_str(), index);
          port_realname =
              stringf("%s[%d]", wire->name.c_str(), wire->start_offset + index);
        }
        POST_MSG(2, "Detect %s port %s (index=%d, width=%d, offset=%d)",
                 get_dir_name(dir).c_str(), port_name.c_str(), index,
                 wire->width, wire->start_offset);
        port_infos.push_back(PORT_INFO(
            dir, port_name, port_fullname, port_realname,
            wire->start_offset + index, index, (uint32_t)(wire->width)));
      }
    } else if (dir == IO_DIR::INOUT) {
      POST_MSG(2, "Warning: Need to understand how to handle INOUT %s",
               wire->name.c_str());
    }
  }
  if (port_infos.size()) {
    trace_and_create_port(module, port_infos);
  } else {
    m_status = false;
    POST_MSG(2, "Error: Fail to detect any port");
  }
  return m_status;
}

/*
  Check if the primitive is supported
*/
const PRIMITIVE_DB* PRIMITIVES_EXTRACTOR::is_supported_primitive(
    const std::string& name, PORT_REQ req) {
  const PRIMITIVE_DB* db = nullptr;
  for (auto& d : SUPPORTED_PRIMITIVES.at(m_technology)) {
    if (d.ready && d.name == name) {
      if (req == PORT_REQ::DONT_CARE ||
          (req == PORT_REQ::IS_PORT && d.is_port) ||
          (req == PORT_REQ::NOT_PORT && !d.is_port)) {
        db = &d;
      }
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
bool PRIMITIVES_EXTRACTOR::get_port_cell_connections(
    Yosys::RTLIL::Cell* cell, const PRIMITIVE_DB* db,
    std::map<std::string, std::string>& primary_connections,
    std::map<std::string, std::string>& secondary_connections) {
  log_assert(cell != nullptr);
  log_assert(db != nullptr);
  log_assert(db->is_port);
  log_assert(cell->type.str() == db->name);
  std::vector<std::string> checking_ports = db->get_checking_ports();
  log_assert(checking_ports.size());
  bool status = false;
  primary_connections.clear();
  secondary_connections.clear();
  POST_MSG(2, "Get important connection of cell %s %s", cell->type.c_str(),
           cell->name.c_str());
  for (auto& it : cell->connections()) {
    bool is_input = std::find(db->inputs.begin(), db->inputs.end(),
                              it.first.str()) != db->inputs.end();
    bool is_output = is_input
                         ? false
                         : std::find(db->outputs.begin(), db->outputs.end(),
                                     it.first.str()) != db->outputs.end();

    if (is_input || is_output) {
      // These are signal we care about
      std::map<std::string, std::string>* connections = &secondary_connections;
      if ((db->dir == IO_DIR::IN && is_input) ||
          (db->dir == IO_DIR::OUT && is_output)) {
        connections = &primary_connections;
      }
      std::ostringstream wire;
      RTLIL_BACKEND::dump_sigspec(wire, it.second, true, true);
      log_assert(connections->find(it.first.str()) == connections->end());
      (*connections)[it.first.str()] = wire.str();
    }
  }
  if (checking_ports.size() == primary_connections.size()) {
    // Good, everything that important is connected
    status = true;
  } else {
    for (auto port : checking_ports) {
      if (primary_connections.find(port) == primary_connections.end()) {
        POST_MSG(3,
                 "Warning: Cell %s does not have all checking port connected "
                 "(Missing %s)",
                 cell->name.c_str(), port.c_str());
      }
    }
  }
  return status;
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
      RTLIL_BACKEND::dump_sigspec(wire, it.second, true, true);
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
void PRIMITIVES_EXTRACTOR::trace_and_create_port(
    Yosys::RTLIL::Module* module, std::vector<PORT_INFO>& port_infos) {
  std::string primitive_name = "";
  std::vector<size_t> port_trackers;
  POST_MSG(1, "Get Port Primitives");
  for (auto cell : module->cells()) {
    const PRIMITIVE_DB* db =
        is_supported_primitive(cell->type.str(), PORT_REQ::IS_PORT);
    if (db != nullptr) {
      bool status = true;
      std::map<std::string, std::string> primary_connections;
      std::map<std::string, std::string> secondary_connections;
      if (get_port_cell_connections(cell, db, primary_connections,
                                    secondary_connections)) {
        // Expect PORT primitive should direct connect to input/output port
        std::vector<PORT_INFO> connected_ports;
        for (auto iter : primary_connections) {
          if (!get_connected_port(module, iter.first, iter.second, db->dir,
                                  port_infos, port_trackers, connected_ports)) {
            status = false;
            break;
          }
        }
        if (status) {
          std::map<std::string, std::string> connections;
          for (auto iter : primary_connections) {
            connections[iter.first] = iter.second;
          }
          for (auto iter : secondary_connections) {
            connections[iter.first] = iter.second;
          }
          m_ports.push_back(new PORT_PRIMITIVE(db, cell->name.str(),
                                               connections, connected_ports));
          get_primitive_parameters(cell, (PRIMITIVE*)(m_ports.back()));
          for (auto& it : cell->parameters) {
            std::ostringstream parameter;
            RTLIL_BACKEND::dump_const(parameter, it.second);
            m_ports.back()->parameters[it.first.str()] = parameter.str();
          }
        } else {
          POST_MSG(4, "Error: Ignore cell %s", cell->name.c_str());
        }
      } else {
        POST_MSG(3, "Error: Ignore cell %s", cell->name.c_str());
        status = false;
      }
    }
  }
}

bool PRIMITIVES_EXTRACTOR::get_connected_port(
    Yosys::RTLIL::Module* module, const std::string& cell_port_name,
    const std::string& connection, IO_DIR dir,
    std::vector<PORT_INFO>& port_infos, std::vector<size_t>& port_trackers,
    std::vector<PORT_INFO>& connected_ports, int loop) {
  bool status = true;
  log_assert(port_trackers.size() <= port_infos.size());
  size_t index = 0;
  while (index < port_infos.size()) {
    if (connection == port_infos[index].fullname) {
      POST_MSG(3, "Cell port %s is connected to %s port %s",
               cell_port_name.c_str(),
               get_dir_name(port_infos[index].dir).c_str(),
               port_infos[index].fullname.c_str());
      if (dir == port_infos[index].dir) {
        connected_ports.push_back(port_infos[index]);
      } else {
        POST_MSG(4,
                 "Error: But there is direction conflict. Port Primitive "
                 "direction is %s, but port direction is %s\n",
                 get_dir_name(dir).c_str(),
                 get_dir_name(port_infos[index].dir).c_str());
        status = false;
        break;
      }
      if (std::find(port_trackers.begin(), port_trackers.end(), index) ==
          port_trackers.end()) {
        port_trackers.push_back(index);
      } else {
        POST_MSG(4, "Warning: %s port %s had been connected more than one",
                 get_dir_name(port_infos[index].dir, 1).c_str(),
                 port_infos[index].fullname.c_str());
      }
      break;
    }
    index++;
  }
  if (index == port_infos.size()) {
    status = false;
    for (auto it : module->connections()) {
      std::vector<std::string> left_signals;
      std::vector<std::string> right_signals;
      get_signals(it.first, left_signals);
      get_signals(it.second, right_signals);
      log_assert(left_signals.size() == right_signals.size());
      for (size_t i = 0; i < right_signals.size(); i++) {
        std::string src =
            dir == IO_DIR::IN ? left_signals[i] : right_signals[i];
        std::string dest =
            dir == IO_DIR::IN ? right_signals[i] : left_signals[i];
        if (src == connection) {
          status =
              get_connected_port(module, cell_port_name, dest, dir, port_infos,
                                 port_trackers, connected_ports, loop + 1);
          break;
        }
      }
      if (status) {
        break;
      }
    }
    if (!status && loop == 0) {
      // Not connected
      POST_MSG(3, "Error: There is no port connection to cell port %s",
               cell_port_name.c_str());
    }
  }
  return status;
}

/*
  Trace clock buffer
*/
void PRIMITIVES_EXTRACTOR::trace_next_primitive(
    Yosys::RTLIL::Module* module, const std::string& src_primitive_name,
    const std::string& dest_primitive_name) {
  POST_MSG(1, "Trace %s --> %s", src_primitive_name.c_str(),
           dest_primitive_name.c_str());
  std::vector<PRIMITIVE*> all_primitives;
  const PRIMITIVE_DB* src_primitive =
      PRIMITIVES_EXTRACTOR::is_supported_primitive(src_primitive_name,
                                                   PORT_REQ::DONT_CARE);
  log_assert(src_primitive != nullptr);
  if (src_primitive->is_port) {
    for (auto& p : m_ports) {
      all_primitives.push_back((PRIMITIVE*)(p));
    }
  } else {
    for (auto& c : m_child_primitives) {
      all_primitives.push_back(c);
    }
  }
  for (PRIMITIVE*& primitive : all_primitives) {
    if (primitive->db->name == src_primitive_name) {
      std::string trace_connection = primitive->get_outtrace_connection();
#if ENABLE_DEBUG_MSG == 0
      size_t original_msg_size = m_msgs.size();
#endif

      POST_MSG(2, "Try %s %s out connection: %s", primitive->db->name.c_str(),
               primitive->name.c_str(), trace_connection.c_str());
      bool found = trace_next_primitive(module, dest_primitive_name, primitive,
                                        trace_connection);
      if (found) {
        for (auto& a : primitive->child_connections[dest_primitive_name]) {
          POST_MSG(4, "Additional Connection: %s", a.c_str());
        }
      } else {
#if ENABLE_DEBUG_MSG == 0
        while (m_msgs.size() > original_msg_size) {
          remove_msg();
        }
#endif
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
  const PRIMITIVE_DB* db =
      is_supported_primitive(module_name, PORT_REQ::NOT_PORT);
  log_assert(db != nullptr);
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
      std::vector<std::string> left_signals;
      std::vector<std::string> right_signals;
      get_signals(it.first, left_signals);
      get_signals(it.second, right_signals);
      log_assert(left_signals.size() == right_signals.size());
      for (size_t i = 0; i < right_signals.size(); i++) {
        std::string src =
            db->dir == IO_DIR::IN ? right_signals[i] : left_signals[i];
        std::string dest =
            db->dir == IO_DIR::IN ? left_signals[i] : right_signals[i];
        if (src == connection) {
          found = trace_next_primitive(module, module_name, parent, dest);
          if (found) {
            if (parent->child_connections.find(module_name) ==
                parent->child_connections.end()) {
              parent->child_connections[module_name] = {};
            }
            parent->child_connections[module_name].insert(
                parent->child_connections[module_name].begin(), dest);
          }
          break;
        }
      }
      if (found) {
        break;
      }
    }
  }
  return found;
}

/*
  Get the chunk bit by bit
*/
void PRIMITIVES_EXTRACTOR::get_chunks(const Yosys::RTLIL::SigChunk& chunk,
                                      std::vector<std::string>& signals) {
  if (chunk.wire == NULL) {
    for (int i = 0; i < chunk.width; i++) {
      signals.push_back("");
    }
  } else {
    // Should use chunk.width? or chunk.wire->width?
    if (chunk.wire->width == 1 && chunk.width == 1 && chunk.offset == 0) {
      signals.push_back(chunk.wire->name.str());
    } else {
      for (int i = 0; i < chunk.width; i++) {
        signals.push_back(
            stringf("%s[%d]", chunk.wire->name.c_str(), chunk.offset + i));
      }
    }
  }
}

/*
  Get the signals bit by bit
*/
void PRIMITIVES_EXTRACTOR::get_signals(const Yosys::RTLIL::SigSpec& sig,
                                       std::vector<std::string>& signals) {
  if (sig.is_chunk()) {
    get_chunks(sig.as_chunk(), signals);
  } else {
    for (auto iter = sig.chunks().begin(); iter != sig.chunks().end(); ++iter) {
      get_chunks(*iter, signals);
    }
  }
}

/*
  Generate instances that being used in JSON
*/
void PRIMITIVES_EXTRACTOR::gen_instances() {
  log_assert(m_status);
  log_assert(m_instances.size() == 0);
  for (PORT_PRIMITIVE*& port : m_ports) {
    PRIMITIVE* primitive = (PRIMITIVE*)(port);
    gen_instances(port->linked_object(), port->linked_objects(), primitive);
  }
}

/*
  Generate instances (recursive for children) that being used in JSON
*/
void PRIMITIVES_EXTRACTOR::gen_instances(
    const std::string& linked_object, std::vector<std::string> linked_objects,
    const PRIMITIVE* primitive) {
  log_assert(m_status);
  if (primitive->db->dir == IO_DIR::IN) {
    // Generate instance: parent first then child
    if (primitive->is_port) {
      gen_instance(linked_objects, primitive);
    }
    for (auto child : primitive->child) {
      gen_wire(linked_object, linked_objects, primitive, child.first);
      gen_instance(linked_objects, child.second);
      gen_instances(linked_object, linked_objects, child.second);
    }

  } else {
    // Reverse the sequence to generate instance, child first, then parent
    for (auto child : primitive->child) {
      gen_instances(linked_object, linked_objects, child.second);
      gen_instance(linked_objects, child.second);
      gen_wire(linked_object, linked_objects, primitive, child.first);
    }
    if (primitive->is_port) {
      gen_instance(linked_objects, primitive);
    }
  }
}

/*
  Generate instance that being used in JSON
*/
void PRIMITIVES_EXTRACTOR::gen_instance(std::vector<std::string> linked_objects,
                                        const PRIMITIVE* primitive) {
  m_instances.push_back(new INSTANCE(primitive->db->name, primitive->name,
                                     linked_objects, primitive));
  m_instances.back()->add_connections(primitive->connections);
  m_instances.back()->add_parameters(primitive->parameters);
}

/*
  Generate wire that connecting primitives
*/
void PRIMITIVES_EXTRACTOR::gen_wire(const std::string& linked_object,
                                    std::vector<std::string> linked_objects,
                                    const PRIMITIVE* primitive,
                                    const std::string& child) {
  log_assert(primitive->child.find(child) != primitive->child.end());
  if (primitive->child_connections.find(child) !=
      primitive->child_connections.end()) {
    uint32_t index = 0;
    std::string trace_connection = primitive->get_outtrace_connection();
    for (auto& wire : primitive->child_connections.at(child)) {
      std::string primitive_name =
          stringf("AUTO_%s_%s_#%d", get_original_name(child).c_str(),
                  linked_object.c_str(), index);
      m_instances.push_back(
          new INSTANCE("WIRE", primitive_name, linked_objects, nullptr));
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
    if (std::find(instance->linked_objects.begin(),
                  instance->linked_objects.end(),
                  port) != instance->linked_objects.end()) {
      instance->locations[port] = location;
      if (instance->primitive != nullptr && instance->primitive->is_port) {
        if (instance->properties.find(port) == instance->properties.end()) {
          instance->properties[port] = {};
        }
        log_assert(instance->properties.find(port) !=
                   instance->properties.end());
        for (auto& iter : properties) {
          instance->properties[port][iter.first] = iter.second;
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
  write_json_object(3, "linked_object", instance->linked_object(), json);
  json << ",\n";
  json << "      \"linked_objects\" : {\n";
  log_assert(instance->linked_objects.size());
  size_t index = 0;
  for (auto& object : instance->linked_objects) {
    if (index) {
      json << ",\n";
    }
    json << "        \"" << object.c_str() << "\" : {\n";
    write_json_object(5, "location", instance->locations.at(object), json);
    json << ",\n";
    json << "          \"properties\" : {\n";
    write_instance_map(instance->properties.at(object), json);
    json << "          }\n";
    json << "        }";
    index++;
  }
  json << "\n";
  json << "      },\n";
  json << "      \"connectivity\" : {\n";
  write_instance_map(instance->connections, json);
  json << "      },\n";
  json << "      \"parameters\" : {\n";
  write_instance_map(instance->parameters, json);
  json << "      }\n";
  json << "    }";
}

/*
  Write out std::map information into JSON
*/
void PRIMITIVES_EXTRACTOR::write_instance_map(
    std::map<std::string, std::string> map, std::ofstream& json,
    uint32_t space) {
  size_t index = 0;
  for (auto& iter : map) {
    if (index) {
      json << ",\n";
    }
    write_json_object(space, iter.first, iter.second, json);
    index++;
  }
  if (index) {
    json << "\n";
  }
}

/*
  Write out std::vector information into JSON
*/
void PRIMITIVES_EXTRACTOR::write_instance_array(std::vector<std::string> array,
                                                std::ofstream& json,
                                                uint32_t space) {
  size_t index = 0;
  for (auto& iter : array) {
    if (index) {
      json << ",\n";
    }
    for (uint8_t i = 0; i < space; i++) {
      json << "  ";
    }
    json << "\"" << iter.c_str() << "\"";
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
