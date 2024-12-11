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

#include <algorithm>
#include <regex>
#include <set>

#include "backends/rtlil/rtlil_backend.h"
#include "kernel/celltypes.h"
#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/sigtools.h"
#include "yaml_info_auto.h"

USING_YOSYS_NAMESPACE

#define EXT_ASSERT(truth, ...)                                              \
  if (!(__builtin_expect(!!(truth), 0))) {                                  \
    std::string msg = Yosys::stringf(__VA_ARGS__);                          \
    printf("File: %s, Func: %s, Line: %d\n", __FILE__, __func__, __LINE__); \
    printf("  Error Msg: %s\n", msg.c_str());                               \
    fflush(stdout);                                                         \
    log_assert((truth));                                                    \
  }

#define POST_MSG(space, ...) \
  { post_msg(space, false, stringf(__VA_ARGS__)); }

#define POST_ERR_MSG(space, ...) \
  { post_msg(space, true, stringf(__VA_ARGS__)); }

#define ENABLE_DEBUG_MSG (0)

/*
  Forwarded
*/
struct PRIMITIVE;
struct PORT;
struct NET;
struct DRIVE_SINK;
std::string primitive_get_id(const PRIMITIVE* primitive);
std::string port_get_id(const PORT* port);

enum DIRECTION { IN, OUT, INOUT, UNKNOWN };

DIRECTION get_dir_enum(const std::string& dir) {
  if (dir == "IN") {
    return DIRECTION::IN;
  } else if (dir == "OUT") {
    return DIRECTION::OUT;
  } else if (dir == "INOUT") {
    return DIRECTION::INOUT;
  }
  log_assert(dir == "UNKNOWN");
  return DIRECTION::UNKNOWN;
};

std::string get_dir_string(const DIRECTION dir) {
  if (dir == DIRECTION::IN) {
    return "IN";
  } else if (dir == DIRECTION::OUT) {
    return "OUT";
  } else if (dir == DIRECTION::INOUT) {
    return "INOUT";
  }
  log_assert(dir == DIRECTION::UNKNOWN);
  return "UNKNOWN";
};

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
    fflush(stdout);
#endif
  }
  const uint32_t offset = 0;
  const std::string msg = "";
};

/*
  Structure to store pin information
*/
struct PARSED_LOCATION {
  void uninitialized() {
    log_assert(type.size() == 0 && bank.size() == 0 && is_clock == false &&
               index == 0 && status && failure_reason.size() == 0);
  }
  void initialized() {
    log_assert(type.size() != 0 && bank.size() != 0 && status &&
               failure_reason.size() == 0);
  }
  std::string get_p_location() {
    initialized();
    return stringf("H%s_%s%s_%d_%dP", type.c_str(), bank.c_str(),
                   is_clock ? "_CC" : "", (index / 2) * 2, (index / 2));
  }
  std::string get_half_bank_location() {
    initialized();
    return stringf("H%s_%s_%d_%dP", type.c_str(), bank.c_str(),
                   (index / 20) * 20, (index / 20) * 10);
  }
  bool parse(const std::string& loc) {
    std::regex const e{"H([PR])_([1-6])_(CC_|)([0-9]+)_([0-9]+)([PN])"};
    std::smatch m;
    uninitialized();
    location = loc;
    status = false;
    do {
      // Basic check
      if (location.size() == 0) {
        failure_reason = "Empty entry";
        break;
      }
      // Regex
      if (!(std::regex_match(location, m, e) && m.size() == 7)) {
        failure_reason = "Does not meet regex";
        break;
      }

      type = m[1].str();
      // Bank Type
      if (!(type == "P" || type == "R")) {
        failure_reason = "Bank Type is invalid";
        break;
      }
      // As long as they are 1-6
      bank = m[2].str();
      if (std::to_string(std::stoi(bank)) != bank) {
        failure_reason = "Bank Index is invalid";
        break;
      }
      // If it is clock
      is_clock = m[3].str() == "CC_";
      // Index
      index = std::stoi(m[4].str());
      if (std::to_string(index) != m[4].str()) {
        failure_reason = "Pin Index is invalid";
        break;
      }
      if (!(index >= 0 && index < 40)) {
        failure_reason = "Pin Index is out of range";
        break;
      }
      int pair_index = (int)(index / 2);
      if (std::to_string(pair_index) != m[5].str()) {
        failure_reason = "Pair index is invalid";
        break;
      }
      if (((index % 2) == 0 && m[6].str() == "P") ||
          ((index % 2) == 1 && m[6].str() == "N")) {
        status = true;
      } else {
        failure_reason = "P/N is invalid";
      }
    } while (0);
    return status;
  }
  std::string location = "";
  std::string type = "";
  std::string bank = "";
  bool is_clock = false;
  int index = 0;
  bool status = true;
  std::string failure_reason = "";
};

/*
  DRIVE_SINK
*/
struct DRIVE_SINK {
  DRIVE_SINK(const PRIMITIVE* dc, const std::string& t, const std::string& n,
             const std::string& p, const std::string& s, size_t pi, size_t ps)
      : primitive(dc),
        type(t),
        name(n),
        port(p),
        subport(s),
        port_index(pi),
        port_size(ps) {
    log_assert(port_size > 0);
  }
  DRIVE_SINK(const DRIVE_SINK* ds)
      : DRIVE_SINK(ds->primitive, ds->type, ds->name, ds->port, ds->subport,
                   ds->port_index, ds->port_size) {}
  /*
    Get ID
  */
  std::string id() const {
    std::string s = stringf("DriveSink (Primitive=%s", type.c_str());
    if (type != "FABRIC") {
      s = stringf("%s, Name=%s", s.c_str(), name.c_str());
    }
    s = stringf("%s, Port=%s", s.c_str(), port.c_str());
    if (port_size > 1) {
      s = stringf("%s, Index=%ld", s.c_str(), port_index);
    }
    s = stringf("%s)", s.c_str());
    return s;
  }
  const PRIMITIVE* primitive = nullptr;
  const std::string type = "";
  const std::string name = "";
  const std::string port = "";
  const std::string subport = "";
  const size_t port_index = 0;
  const size_t port_size = 0;
};

/*
  Net
*/
struct NET {
  NET(const std::string& n, const PORT* p, const std::string& ps)
      : name(n), port(p), port_subname(ps) {}
  ~NET() {
    while (drive_sinks.size()) {
      delete drive_sinks.back();
      drive_sinks.pop_back();
    }
  }
  /*
    Get ID
  */
  std::string id() const {
    return stringf("Net %s (%s)", name.c_str(), port_get_id(port).c_str());
  }
  const std::string name = "";
  const PORT* port = nullptr;
  const std::string port_subname = "";
  std::vector<DRIVE_SINK*> drive_sinks;
};

/*
  PORT
*/
struct PORT {
  PORT(const std::string& n, DIRECTION d, size_t s, const PRIMITIVE* dc)
      : name(n), dir(d), size(s), primitive(dc) {
    log_assert(dir == DIRECTION::IN || dir == DIRECTION::OUT);
    log_assert(size);
  }
  ~PORT() {
    while (nets.size()) {
      delete nets.back();
      nets.pop_back();
    }
  }
  void add_net(const std::string& name, const std::string& subport) {
    log_assert(nets.size() < size);
    nets.push_back(new NET(name, this, subport));
  }
  NET* get_net(const std::string& name) const {
    NET* net = nullptr;
    for (auto n : nets) {
      if (n->name == name) {
        net = n;
        break;
      }
    }
    return net;
  }
  /*
    Get ID
  */
  std::string id() const {
    return stringf("Port %s (%s)", name.c_str(),
                   primitive_get_id(primitive).c_str());
  }

  const std::string name = "";
  const DIRECTION dir = DIRECTION::UNKNOWN;
  const size_t size = 0;
  const PRIMITIVE* primitive = nullptr;
  std::vector<NET*> nets;
};

/*
  OBJECT
*/
struct OBJECT {
  OBJECT(const std::string& pn) : port_name(pn) {}
  const std::string port_name = "";
  std::string location = "";
  std::map<std::string, std::string> properties;
};

/*
  CLOCK driven to
*/
struct CLOCK_DRIVEN_TO {
  CLOCK_DRIVEN_TO(const std::string& sp, int s = -1, int fbs = -1)
      : src_port(sp), slot(s), fclk_buf_slot(fbs) {}
  const std::string src_port = "";
  const int slot = -1;
  const int fclk_buf_slot = -1;
  std::vector<const PORT*> dest_ports;
  std::vector<std::string> fabric_ports;
};

/*
  PRIMITIVE
*/
struct PRIMITIVE {
 public:
  PRIMITIVE(const std::string& t, const std::string& n, const YAML* y)
      : type(t),
        name(n),
        yaml(y),
        dir(yaml != nullptr ? get_dir_enum(yaml->direction)
                            : DIRECTION::UNKNOWN) {}
  ~PRIMITIVE() {
    while (ports.size()) {
      auto iter = ports.begin();
      delete iter->second;
      ports.erase(iter);
    }
    while (objects.size()) {
      auto iter = objects.begin();
      delete iter->second;
      objects.erase(iter);
    }
    while (fast_clock_drive_to.size()) {
      delete fast_clock_drive_to.back();
      fast_clock_drive_to.pop_back();
    }
    while (core_clock_drive_to.size()) {
      delete core_clock_drive_to.back();
      core_clock_drive_to.pop_back();
    }
  }
  /*
    Create PRIMITIVE from Cell
  */
  static PRIMITIVE* create(
      const std::string& technology, const Yosys::RTLIL::Cell* cell,
      const std::map<std::string, bool>& fabric_ports_dir,
      std::map<std::string, DRIVE_SINK*>& out_nets,
      std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets,
      std::vector<MSG*>& msgs, std::vector<std::string>& errors) {
    const YAML* yaml = nullptr;
    std::string cell_type = remove_first_backslash(cell->type.str());
    std::string cell_name = remove_first_backslash(cell->name.str());
    if (YAML_DB.at(technology).find(cell_type) !=
        YAML_DB.at(technology).end()) {
      yaml = &YAML_DB.at(technology).at(cell_type);
      log_assert(cell_type != "FABRIC");
    } else if (cell_type.find("fabric_") == 0 &&
               cell_name == "fabric_instance") {
      cell_type = "FABRIC";
      cell_name = "FABRIC";
    } else {
      log_assert(cell_type != "FABRIC");
    }
    PRIMITIVE* primitive = new PRIMITIVE(cell_type, cell_name, yaml);
    if (!primitive->extract(cell, fabric_ports_dir, out_nets, in_nets, msgs,
                            errors)) {
      delete primitive;
      primitive = nullptr;
    }
    return primitive;
  }
  /*
    Get rid the first character if it is '\\'
  */
  static std::string remove_first_backslash(const std::string& name) {
    if (name.size() > 0 && name[0] == '\\') {
      return name.substr(1);
    }
    return name;
  }
  /*
    Get the signals bit by bit
  */
  static void get_signals(const Yosys::RTLIL::SigSpec& sig,
                          std::vector<std::string>& signals) {
    if (sig.is_chunk()) {
      get_chunks(sig.as_chunk(), signals);
    } else {
      for (auto iter = sig.chunks().begin(); iter != sig.chunks().end();
           ++iter) {
        get_chunks(*iter, signals);
      }
    }
    for (size_t i = 0; i < signals.size(); i++) {
      signals[i] = remove_first_backslash(signals[i]);
    }
  }
  /*
    Get ID
  */
  std::string id() const {
    return stringf("Primitive %s (Name=%s)", type.c_str(), name.c_str());
  }
  /*
    If there is any port connected to iopad_external_pin
  */
  bool has_iopad_external_pin() {
    bool iopad_external_pin = false;
    for (auto& p : ports) {
      const PORT* port = p.second;
      for (auto& net : port->nets) {
        for (auto& ds : net->drive_sinks) {
          if (ds->primitive == nullptr) {
            log_assert(ds->type == "INPUT" || ds->type == "OUTPUT" ||
                       ds->type == "INOUT");
            iopad_external_pin = true;
            break;
          }
        }
      }
    }
    return iopad_external_pin;
  }
  /*
    Determine all the object. This is called for parent only
  */
  void determine_objects(std::map<std::string, uint32_t>& tracker) {
    log_assert(location_object.empty());
    log_assert(linked_object.empty());
    log_assert(objects.size() == 0);
    log_assert(yaml != nullptr);
    if (yaml->group == "DATA") {
      for (auto& p : ports) {
        const PORT* port = p.second;
        log_assert(yaml->ports.find(port->name) != yaml->ports.end());
        const PORT_BASIC& port_basic = yaml->ports.at(port->name);
        if (port_basic.is_attribute("iopad_external_pin")) {
          log_assert(port->nets.size() == 1);
          log_assert(port->nets[0]->drive_sinks.size() == 1);
          const DRIVE_SINK* ds = port->nets[0]->drive_sinks[0];
          log_assert(ds->primitive == nullptr);
          if (port_basic.is_input) {
            log_assert(ds->type == "INPUT" || ds->type == "INOUT");
          } else {
            log_assert(ds->type == "OUTPUT" || ds->type == "INOUT");
          }
          is_inout = is_inout || ds->type == "INOUT";
          log_assert(objects.find(ds->subport) == objects.end());
          objects[ds->subport] = new OBJECT(port->name);
          if (!((port->name == "I_N" || port->name == "O_N") &&
                (location_object.size() > 0))) {
            location_object = ds->subport;
          }
          log_assert(location_object.size());
          log_assert(objects.find(location_object) != objects.end());
        }
      }
    } else {
      log_assert(yaml->group == "STANDALONE_CLOCK" ||
                 yaml->group == "FABRIC_CLOCK" ||
                 yaml->group == "STANDALONE_FABRIC" || yaml->group == "SOC");
      if (tracker.find(type) == tracker.end()) {
        tracker[type] = 0;
      }
      location_object = stringf("%s#%d", type.c_str(), tracker[type]);
      log_assert(objects.find(location_object) == objects.end());
      objects[location_object] = new OBJECT(location_object);
      tracker[type] = tracker[type] + 1;
    }
    log_assert(location_object.size());
    log_assert(objects.size());
    linked_object = location_object;
    for (auto& obj : objects) {
      if (obj.first != location_object) {
        linked_object =
            stringf("%s+%s", linked_object.c_str(), obj.first.c_str());
      }
    }
  }

  /*
    Get the last data child
  */
  const PRIMITIVE* get_end_data_child() {
    log_assert(parent == nullptr);
    log_assert(grandparent == nullptr);
    auto child = this;
    while (child->data_child != nullptr) {
      log_assert(child == child->data_child->parent);
      log_assert(this == child->data_child->grandparent);
      child = child->data_child;
    }
    return child;
  }
  /*
    Get the last clock child
  */
  const PRIMITIVE* get_end_clock_child() {
    log_assert(parent == nullptr);
    log_assert(grandparent == nullptr);
    auto child = this;
    while (child->clock_child != nullptr) {
      log_assert(child == child->clock_child->parent);
      log_assert(this == child->clock_child->grandparent);
      child = child->clock_child;
    }
    return child;
  }
  /*
    Get data path info
  */
  std::string get_data_chain_info() {
    log_assert(parent == nullptr);
    log_assert(grandparent == nullptr);
    std::string info = this->type;
    auto child = this;
    while (child->data_child != nullptr) {
      log_assert(child == child->data_child->parent);
      log_assert(this == child->data_child->grandparent);
      child = child->data_child;
      if (dir == DIRECTION::OUT) {
        info = stringf("%s |-> %s", child->type.c_str(), info.c_str());
      } else {
        info = stringf("%s |-> %s", info.c_str(), child->type.c_str());
      }
    }
    return info;
  }
  /*
    Get clock path info
  */
  std::string get_clock_chain_info() {
    log_assert(parent == nullptr);
    log_assert(grandparent == nullptr);
    std::string info = this->type;
    auto child = this;
    while (child->clock_child != nullptr) {
      log_assert(child == child->clock_child->parent);
      log_assert(this == child->clock_child->grandparent);
      child = child->clock_child;
      info = stringf("%s |-> %s", info.c_str(), child->type.c_str());
    }
    return info;
  }
  /*
    Set parameter
  */
  void set_parameter(const std::string& parameter, const std::string& value) {
    log_assert(parameters.find(parameter) == parameters.end());
    parameters[parameter] = value;
  }
  /*
    Get flags
      1. Type
      2. PIN_CLOCK_CORE_ONLY
      3. INOUT
  */
  std::vector<std::string> get_flags() const {
    std::vector<std::string> flags;
    flags.push_back(type);
    if (type == "CLK_BUF" && core_clock_drive_to.size() > 0 &&
        fast_clock_drive_to.size() == 0) {
      flags.push_back("PIN_CLOCK_CORE_ONLY");
    }
    if (is_inout) {
      flags.push_back("INOUT");
    }
    return flags;
  }
  /*
    Get parent type
  */
  std::string get_pre_primitive() const {
    if (parent != nullptr) {
      return parent->type;
    } else {
      return "";
    }
  }
  /*
    Get child type(s)
  */
  std::vector<std::string> get_post_primitives() const {
    std::vector<std::string> primitives;
    if (clock_child != nullptr) {
      primitives.push_back(clock_child->type);
    }
    if (data_child != nullptr) {
      primitives.push_back(data_child->type);
    }
    return primitives;
  }

 private:
  void post_msg(uint32_t offset, bool is_error, const std::string& msg) {
    log_assert(m_msgs != nullptr);
    log_assert(m_errors != nullptr);
    if (is_error) {
      m_errors->push_back(msg);
    }
    m_msgs->push_back(new MSG(offset, is_error ? "Error: " + msg : msg));
  }
  /*
    Get the chunk bit by bit
  */
  static void get_chunks(const Yosys::RTLIL::SigChunk& chunk,
                         std::vector<std::string>& signals) {
    if (chunk.wire == NULL) {
      std::ostringstream const_value;
      RTLIL_BACKEND::dump_const(const_value, chunk.data, chunk.width,
                                chunk.offset);
      std::string keyword = stringf("%d'", chunk.width);
      std::string const_str = const_value.str();
      if (const_str.find(keyword) == 0 &&
          (const_str.size() == (keyword.size() + (size_t)(chunk.width)))) {
        for (int i = 0; i < chunk.width; i++) {
          signals.push_back(stringf("__const_bit_%c__", const_str.back()));
          const_str.pop_back();
        }
      } else {
        for (int i = 0; i < chunk.width; i++) {
          signals.push_back("");
        }
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
    Extract information from CELL
  */
  bool extract(const Yosys::RTLIL::Cell* cell,
               const std::map<std::string, bool>& fabric_ports_dir,
               std::map<std::string, DRIVE_SINK*>& out_nets,
               std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets,
               std::vector<MSG*>& msgs, std::vector<std::string>& errors) {
    log_assert(m_msgs == nullptr);
    log_assert(m_errors == nullptr);
    m_msgs = &msgs;
    m_errors = &errors;
    bool status = yaml != nullptr || (type == "FABRIC" && name == "FABRIC");
    if (status) {
      if (yaml != nullptr) {
        log_assert(type != "FABRIC");
      } else {
        log_assert(type == "FABRIC" && name == "FABRIC");
      }
      for (auto& iter : cell->parameters) {
        std::ostringstream parameter;
        RTLIL_BACKEND::dump_const(parameter, iter.second);
        set_parameter(remove_first_backslash(iter.first.str()),
                      get_param_string(parameter.str()));
      }
      for (auto& iter : cell->connections()) {
        bool input = false;
        int size = -1;
        std::string port_name = remove_first_backslash(iter.first.str());
        std::vector<std::string> signals;
        get_signals(iter.second, signals);
        log_assert(signals.size());
        if (yaml != nullptr) {
          EXT_ASSERT(yaml->ports.find(port_name) != yaml->ports.end(),
                     "Unknown port %s for Primitive %s", port_name.c_str(),
                     yaml->name.c_str());
          const PORT_BASIC& basic = yaml->ports.at(port_name);
          input = basic.is_input;
          size = basic.size;
        } else {
          log_assert(signals.size() == 1);
          log_assert(fabric_ports_dir.find(port_name) !=
                     fabric_ports_dir.end());
          input = fabric_ports_dir.at(port_name);
          size = 1;
        }
        if (int(signals.size()) == size || size == -1) {
          PORT* port =
              add_port(port_name, input ? DIRECTION::IN : DIRECTION::OUT,
                       signals.size());
          size_t i = 0;
          for (auto& signal : signals) {
            std::string subport = port_name;
            if (signals.size() > 1) {
              subport = stringf("%s[%ld]", port_name.c_str(), i);
            }
            port->add_net(signal, subport);
            DRIVE_SINK* new_ds = new DRIVE_SINK(this, type, name, port_name,
                                                subport, i, signals.size());
            if (input) {
              if (in_nets.find(signal) == in_nets.end()) {
                in_nets[signal] = {};
              }
              in_nets.at(signal).push_back(new_ds);
            } else {
              if (out_nets.find(signal) == out_nets.end()) {
                out_nets[signal] = new_ds;
              } else {
                status = false;
                DRIVE_SINK* ds = out_nets.at(signal);
                POST_ERR_MSG(3,
                             "OutNet %s is driven by multiple sources. "
                             "Existing %s vs new %s",
                             signal.c_str(), ds->id().c_str(),
                             new_ds->id().c_str());
                delete new_ds;
              }
            }
            i++;
          }
        } else {
          status = false;
          POST_ERR_MSG(3,
                       "Port %s (Primitive=%s, Name=%s) size mismatch. "
                       "Database size is %d, but detected %ld",
                       port_name.c_str(), type.c_str(), name.c_str(), size,
                       signals.size());
        }
      }
    } else {
      POST_ERR_MSG(3, "Unknown cell %s %s", type.c_str(), name.c_str());
    }
    m_msgs = nullptr;
    m_errors = nullptr;
    return status;
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
    Add new port
  */
  PORT* add_port(const std::string& name, DIRECTION dir, size_t size) {
    log_assert(ports.find(name) == ports.end());
    ports[name] = new PORT(name, dir, size, this);
    return ports.at(name);
  }

 public:
  const std::string type = "";
  const std::string name = "";
  const YAML* yaml = nullptr;
  const DIRECTION dir = DIRECTION::UNKNOWN;
  bool is_inout = false;
  std::map<std::string, PORT*> ports;
  std::map<std::string, std::string> parameters;
  std::string location_object = "";
  std::string location = "";
  PARSED_LOCATION parsed_location;
  std::string linked_object = "";
  std::map<std::string, OBJECT*> objects;
  std::string mode = "";
  bool has_clock_child = false;
  PRIMITIVE* data_child = nullptr;
  PRIMITIVE* clock_child = nullptr;
  PRIMITIVE* parent = nullptr;
  PRIMITIVE* grandparent = nullptr;
  // Clock network
  std::vector<const CLOCK_DRIVEN_TO*> fast_clock_drive_to;
  std::vector<const CLOCK_DRIVEN_TO*> core_clock_drive_to;
  std::pair<const PRIMITIVE*, std::string> fast_clock_driven_by = {nullptr, ""};
  std::pair<const PRIMITIVE*, std::string> core_clock_driven_by = {nullptr, ""};

 private:
  // Temp
  std::vector<MSG*>* m_msgs = nullptr;
  std::vector<std::string>* m_errors = nullptr;
};

std::string primitive_get_id(const PRIMITIVE* primitive) {
  return stringf("Primitive=%s, Name=%s", primitive->type.c_str(),
                 primitive->name.c_str());
}

std::string port_get_id(const PORT* port) {
  return stringf("Primitive=%s, Name=%s, Port=%s",
                 port->primitive->type.c_str(), port->primitive->name.c_str(),
                 port->name.c_str());
}
