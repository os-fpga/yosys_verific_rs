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
    a. I_BUF        [connected to PORT]
    b. I_BUF_DS     [connected to PORT]
    c. O_BUF        [connected to PORT]
    d. O_BUFT       [connected to PORT]
    e. O_BUF_DS     [connected to PORT]
    f. O_BUFT_DS    [connected to PORT]
    g. CLK_BUF      [connected internally]
    h. I_DELAY      [connected internally]
    i. O_DELAY      [connected internally]
    j. I_DDR        [connected internally]
    k. O_DDR        [connected internally]
    j. PLL          [connected internally]
    k. BOOT_CLOCK   [connected internally]
    l. O_SERDES_CLK [connected internally]
    m. FCLK_BUF     [connected internally]

    and more when other use cases are understood

  Currently supported use cases are:
    a. I_PORTS:             I_BUF
                            I_BUF_DS
    b. Clock port:          I_PORTS -> CLK_BUF
    c. O_PORTS:             O_BUF
                            O_BUFT
                            O_BUF_DS
                            O_BUFT_DS
    d. I_DELAY:             I_PORTS -> I_DELAY
    e. O_DELAY:             O_DELAY -> O_PORTS
    f. I_DDR:               I_PORTS -> I_DDR (become two bits)
                            I_DELAY -> I_DDR (become two bits)
    g. O_DDR:               (from two bits) O_DDR -> O_PORTS
                            (from two bits) O_DDR -> O_DELAY
    h. I_SERDES:            I_PORTS -> I_SERDES
                            I_DELAY -> I_SERDES
    i. O_SERDES:            O_SERDES -> O_PORTS
                            O_SERDES -> O_DELAY
    j. PLL:                 I_PORTS -> CLK_BUF -> PLL
                            BOOT_CLOCK -> PLL
    k. O_SERDES_CLK:        O_SERDES_CLK -> O_BUF/O_BUFT
    l. FCLK_BUF

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

USING_YOSYS_NAMESPACE

#define POST_MSG(space, ...) \
  { post_msg(space, stringf(__VA_ARGS__)); }

#define ENABLE_DEBUG_MSG (0)
#define GENERATION_ALWAYS_INWARD_DIRECTION (1)
#define ENABLE_INSTANCE_CROSS_CHECK (1)

#define P_IS_NULL (0)
#define P_IS_NOT_READY (1 << 0)
#define P_IS_PORT (1 << 1)
#define P_IS_STANDALONE (1 << 2)
#define P_IS_CLOCK (1 << 3)
#define P_IS_GEARBOX_CLOCK (1 << 4)
#define P_IS_ANY_INPUTS (1 << 5)
#define P_IS_ANY_OUTPUTS (1 << 6)
#define P_IS_OPTIONAL_INPUT (1 << 7)
#define P_IS_OPTIONAL_OUTPUT (1 << 8)
#define P_IS_IN_DIR (1 << 9)
#define P_IS_FABRIC_CLKBUF (1 << 10)

std::map<std::string, uint32_t> g_standalone_tracker;
bool g_enable_debug = false;

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
  Split the string using the delimiter
*/
std::vector<std::string> split_string(std::string str,
                                      const std::string& delimiter) {
  log_assert(str.size());
  log_assert(delimiter.size());
  std::vector<std::string> strs;
  size_t index = str.find(delimiter);
  while (index != std::string::npos) {
    strs.push_back(str.substr(0, index));
    str = str.substr(index + delimiter.size());
    index = str.find(delimiter);
  }
  strs.push_back(str);
  return strs;
}

/*
  Precaution: sort the string alphabetically
*/
std::string sort_name(std::string names) {
  std::set<std::string> sorted_names;
  size_t index = names.find("+");
  while (index != std::string::npos) {
    log_assert(index != 0);
    sorted_names.insert(names.substr(0, index));
    names = names.substr(index + 1);
    index = names.find("+");
  }
  log_assert(names.size());
  sorted_names.insert(names);
  names = "";
  for (auto iter = sorted_names.begin(); iter != sorted_names.end(); iter++) {
    if (names.size()) {
      names = stringf("%s+%s", names.c_str(), (*iter).c_str());
    } else {
      names = (*iter);
    }
  }
  return names;
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
  PRIMITIVE_DB(const std::string& n, uint32_t f, std::vector<std::string> is,
               std::vector<std::string> os, const std::string& it,
               const std::string& ot, const std::string& fc, const std::string& cc,
               const std::string& d, std::map<std::string, std::string> csm,
               std::map<std::string, std::string> p)
      : name(n),
        feature(f),
        inputs(is),
        outputs(os),
        intrace_connection(it),
        outtrace_connection(ot),
        fast_clock(fc),
        core_clock(cc),
        data_signal(d),
        control_signal_map(csm),
        properties(p) {}
  std::vector<std::string> get_checking_ports() const {
    if (is_in_dir()) {
      return inputs;
    }
    return outputs;
  }
  bool is_ready() const { return (feature & P_IS_NOT_READY) == P_IS_NULL; }
  bool is_port() const { return (feature & P_IS_PORT) != P_IS_NULL; }
  bool is_standalone() const {
    return (feature & P_IS_STANDALONE) != P_IS_NULL;
  }
  bool is_clock() const { return (feature & P_IS_CLOCK) != P_IS_NULL; }
  bool is_fabric_clkbuf() const {
    return (feature & P_IS_FABRIC_CLKBUF) != P_IS_NULL;
  }
  bool is_gearbox_clock() const {
    return (feature & P_IS_GEARBOX_CLOCK) != P_IS_NULL;
  }
  bool is_any_inputs() const {
    return (feature & P_IS_ANY_INPUTS) != P_IS_NULL;
  }
  bool is_any_outputs() const {
    return (feature & P_IS_ANY_OUTPUTS) != P_IS_NULL;
  }
  bool is_optional_input() const {
    return (feature & P_IS_OPTIONAL_INPUT) != P_IS_NULL;
  }
  bool is_optional_output() const {
    return (feature & P_IS_OPTIONAL_OUTPUT) != P_IS_NULL;
  }
  bool is_in_dir() const { return (feature & P_IS_IN_DIR) != P_IS_NULL; }
  bool is_out_dir() const { return (feature & P_IS_IN_DIR) == P_IS_NULL; }
  const std::string name = "";
  const uint32_t feature = 0;
  const std::vector<std::string> inputs;
  const std::vector<std::string> outputs;
  const std::string intrace_connection = "";
  const std::string outtrace_connection = "";
  const std::string fast_clock = "";
  const std::string core_clock = "";
  const std::string data_signal = "";
  const std::map<std::string, std::string> control_signal_map;
  const std::map<std::string, std::string> properties;
};

/*
  Supported primitives
*/
// clang-format off
const std::map<std::string, std::vector<PRIMITIVE_DB>> SUPPORTED_PRIMITIVES = {
  {"genesis3",
    // These are Port Primitive, they are directly connected to the
    // PIN/PORT/PAD
    // Inputs
    {
      {
        PRIMITIVE_DB(
          "\\I_BUF",
          P_IS_PORT | P_IS_IN_DIR,
          {"\\I"},                              // inputs
          {"\\O"},                              // outputs
          "",                                   // intrace_connection
          "\\O",                                // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "\\O",                                // data_signal
          {                                     // control_signal_map
            {"EN", "in:f2g_in_en_{A|B}"}
          },
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\I_BUF_DS",
          P_IS_PORT | P_IS_IN_DIR,
          {"\\I_P", "\\I_N"},                   // inputs
          {"\\O"},                              // outputs
          "",                                   // intrace_connection
          "\\O",                                // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "\\O",                                // data_signal
          {                                     // control_signal_map
            {"EN", "in:f2g_in_en_{A|B}"}
          },
          {}                                    // properties
      )},
      // Output
      {
        PRIMITIVE_DB(
          "\\O_BUF",
          P_IS_PORT,
          {"\\I"},                              // inputs
          {"\\O"},                              // outputs
          "",                                   // intrace_connection
          "\\I",                                // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "\\I",                                // data_signal
          {},                                   // control_signal_map
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\O_BUFT",
          P_IS_PORT,
          {"\\I"},                              // inputs
          {"\\O"},                              // outputs
          "",                                   // intrace_connection
          "\\I",                                // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "\\I",                                // data_signal
          {                                     // control_signal_map
            {"T", "in:f2g_tx_oe_{A|B}"}
          },
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\O_BUF_DS",
          P_IS_PORT,
          {"\\I"},                              // inputs
          {"\\O_P", "\\O_N"},                   // outputs
          "",                                   // intrace_connection
          "\\I",                                // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "\\I",                                // data_signal
          {},                                   // control_signal_map
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\O_BUFT_DS",
          P_IS_PORT,
          {"\\I"},                              // inputs
          {"\\O_P", "\\O_N"},                   // outputs
          "",                                   // intrace_connection
          "\\I",                                // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "\\I",                                // data_signal
          {                                     // control_signal_map
            {"T", "in:f2g_tx_oe_{A|B}"}
          },
          {}                                    // properties
      )},
      // These are none-Port Primitive
      // In direction
      {
        PRIMITIVE_DB(
          "\\CLK_BUF",
          P_IS_CLOCK | P_IS_GEARBOX_CLOCK | P_IS_IN_DIR,
          {"\\I"},                              // inputs
          {"\\O"},                              // outputs
          "\\I",                                // intrace_connection
          "\\O",                                // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "",                                   // data_signal
          {},                                   // control_signal_map
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\I_DELAY",
          P_IS_IN_DIR,
          {"\\I", "\\CLK_IN"},                  // inputs
          {"\\O"},                              // outputs
          "\\I",                                // intrace_connection
          "\\O",                                // outtrace_connection
          "\\CLK_IN",                           // fast_clock
          "\\CLK_IN",                           // core_clock
          "\\O",                                // data_signal
          {                                     // control_signal_map
            {"DLY_LOAD", "in:rule=half-first:f2g_trx_dly_ld"},
            {"DLY_ADJ", "in:rule=half-first:f2g_trx_dly_adj"},
            {"DLY_INCDEC", "in:rule=half-first:f2g_trx_dly_inc"},
            {"DLY_TAP_VALUE", "out:rule=half-first:g2f_trx_dly_tap"}
          },
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\I_DDR",
          P_IS_IN_DIR,
          {"\\D", "\\C"},                       // inputs
          {},                                   // outputs
          "\\D",                                // intrace_connection
          "",                                   // outtrace_connection
          "\\C",                                // fast_clock
          "",                                   // core_clock
          "\\Q",                                // data_signal
          {                                     // control_signal_map
            {"R", "in:TO_BE_DETERMINED"},
            {"E", "in:TO_BE_DETERMINED"}
          },
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\I_SERDES",
          P_IS_IN_DIR | P_IS_OPTIONAL_OUTPUT,
          {"\\D", "\\CLK_IN", "\\PLL_CLK"},     // inputs
          {"\\CLK_OUT"},                        // outputs
          "\\D",                                // intrace_connection
          "",                                   // outtrace_connection
          "\\PLL_CLK",                          // fast_clock
          "\\CLK_IN",                           // core_clock
          "\\Q",                                // data_signal
          {                                     // control_signal_map
            {"RST", "in:f2g_trx_reset_n_{A|B}"},
            {"BITSLIP_ADJ", "in:rule=half-first:f2g_rx_bitslip_adj"},
            {"EN", "in:TO_BE_DETERMINED"},
            {"DATA_VALID", "out:g2f_rx_dvalid_{A|B}"},
            {"DPA_LOCK", "out:rule=half-first:g2f_rx_dpa_lock"},
            {"DPA_ERROR", "out:rule=half-first:g2f_rx_dpa_error"}
          },
          {                                     // properties
            {"MUST_HAVE_PARAMS", "\\WIDTH" },
            {"DEFINE_DATA_WIDTH_FROM_PARAMS", "\\WIDTH" },
            {"CLK_OUT_PORT", "\\CLK_OUT" }
          }
      )},
      {
        PRIMITIVE_DB(
          "\\BOOT_CLOCK",
          P_IS_CLOCK | P_IS_STANDALONE | P_IS_IN_DIR,
          {},                                   // inputs
          {"\\O"},                              // outputs
          "",                                   // intrace_connection
          "\\O",                                // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "",                                   // data_signal
          {},                                   // control_signal_map
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\PLL",
          P_IS_CLOCK | P_IS_GEARBOX_CLOCK | P_IS_ANY_OUTPUTS | P_IS_IN_DIR,
          {"\\CLK_IN"},                         // inputs
          {"\\CLK_OUT", "\\CLK_OUT_DIV2",       // outputs
           "\\CLK_OUT_DIV3", "\\CLK_OUT_DIV4"},
          "\\CLK_IN",                           // intrace_connection
          "",                                   // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "",                                   // data_signal
          {                                     // control_signal_map
            {"PLL_EN", "in:TO_BE_DETERMINED"},
            {"LOCK", "out:TO_BE_DETERMINED"}
          },
          {}                                    // properties
      )},
      // Out direction
      {
        PRIMITIVE_DB(
          "\\O_DELAY",
          P_IS_NULL,
          {"\\I", "\\CLK_IN"},                  // inputs
          {"\\O"},                              // outputs
          "\\O",                                // intrace_connection
          "\\I",                                // outtrace_connection
          "\\CLK_IN",                           // fast_clock
          "\\CLK_IN",                           // core_clock
          "\\I",                                // data_signal
          {                                     // control_signal_map
            {"DLY_LOAD", "in:rule=half-first:f2g_trx_dly_ld"},
            {"DLY_ADJ", "in:rule=half-first:f2g_trx_dly_adj"},
            {"DLY_INCDEC", "in:rule=half-first:f2g_trx_dly_inc"},
            {"DLY_TAP_VALUE", "out:rule=half-first:g2f_trx_dly_tap"}
          },
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\O_DDR",
          P_IS_NULL,
          {"\\C"},                              // inputs
          {"\\Q"},                              // outputs
          "\\Q",                                // intrace_connection
          "",                                   // outtrace_connection
          "",                                   // fast_clock
          "\\C",                                // core_clock
          "\\D",                                // data_signal
          {                                     // control_signal_map
            {"R", "in:TO_BE_DETERMINED"},
            {"E", "in:TO_BE_DETERMINED"}
          },
          {}                                    // properties
      )},
      {
        PRIMITIVE_DB(
          "\\O_SERDES",
          P_IS_NULL,
          {"\\CLK_IN", "\\PLL_CLK"},            // inputs
          {"\\Q"},                              // outputs
          "\\Q",                                // intrace_connection
          "",                                   // outtrace_connection
          "\\PLL_CLK",                          // fast_clock
          "\\CLK_IN",                           // core_clock
          "\\D",                                // data_signal
          {                                     // control_signal_map
            {"RST", "in:f2g_trx_reset_n_{A|B}"},
            {"DATA_VALID", "in:f2g_tx_dvalid_{A|B}"},
            {"OE_IN", "in:TO_BE_DETERMINED"},
            {"OE_OUT", "out:TO_BE_DETERMINED"},
            {"CHANNEL_BOND_SYNC_IN", "in:TO_BE_DETERMINED"},
            {"CHANNEL_BOND_SYNC_OUT", "out:TO_BE_DETERMINED"},
            {"PLL_LOCK", "in:TO_BE_DETERMINED"}
          },
          {                                     // properties
            {"MUST_HAVE_PARAMS", "\\WIDTH" },
            {"DEFINE_DATA_WIDTH_FROM_PARAMS", "\\WIDTH" }
          }                                    
      )},
      {
        PRIMITIVE_DB(
          "\\O_SERDES_CLK",
          P_IS_NULL,
          {"\\PLL_CLK"},                        // inputs
          {"\\OUTPUT_CLK"},                     // outputs
          "\\OUTPUT_CLK",                       // intrace_connection
          "",                                   // outtrace_connection
          "\\PLL_CLK",                          // fast_clock
          "",                                   // core_clock
          "",                                   // data_signal
          {                                     // control_signal_map
            {"CLK_EN", "in:f2g_tx_clk_en_{A|B} "}
          },
          {}                                    // properties
      )},
      // Special: Fabric Clock Buffer
      {
        PRIMITIVE_DB(
          "\\FCLK_BUF",
          P_IS_FABRIC_CLKBUF | P_IS_IN_DIR,
          {"\\I"},                              // inputs
          {"\\O"},                              // outputs
          "\\I",                                // intrace_connection
          "\\O",                                // outtrace_connection
          "",                                   // fast_clock
          "",                                   // core_clock
          "",                                   // data_signal
          {},                                   // control_signal_map
          {}                                    // properties
      )}
    }
  }
};
// clang-format on

/*
  Base structure of primitive
*/
struct PRIMITIVE {
  PRIMITIVE(const PRIMITIVE_DB* d, const std::string& n, PRIMITIVE* p,
            std::map<std::string, std::string> c, bool i)
      : db(d),
        name(n),
        parent(p),
        grandparent(p == nullptr
                        ? nullptr
                        : (p->grandparent == nullptr ? p : p->grandparent)),
        connections(c),
        is_port_primitive(i) {
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
  void set_instance(INSTANCE* inst) const {
    log_assert(inst != nullptr);
    log_assert(instance == nullptr);
    instance = inst;
  }
  // Constructor
  const PRIMITIVE_DB* db = nullptr;
  const std::string name = "";
  const PRIMITIVE* parent = nullptr;
  const PRIMITIVE* grandparent = nullptr;
  const std::map<std::string, std::string> connections;
  const bool is_port_primitive = false;
  int data_width = -2;
  std::map<std::string, std::string> parameters;
  std::map<std::string, PRIMITIVE*> child;
  std::map<std::string, std::vector<std::string>> child_connections;
  std::map<std::string, std::vector<const PRIMITIVE*>> gearbox_clocks;
  std::vector<std::string> errors;
  mutable INSTANCE* instance = nullptr;
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
        dir((db->is_standalone() || db->is_fabric_clkbuf())
                ? IO_DIR::IN
                : (ps.size() ? ps[0].dir : IO_DIR::UNKNOWN)) {
    log_assert(port_infos.size() || db->is_standalone() ||
               db->is_fabric_clkbuf());
    log_assert(dir == IO_DIR::IN || dir == IO_DIR::OUT);
    for (auto port : port_infos) {
      log_assert(dir == port.dir);
    }
    if (db->is_standalone() || db->is_fabric_clkbuf()) {
      standalone_name =
          db->is_standalone() ? get_original_name(db->name) : "FABRIC_CLKBUF";
      if (g_standalone_tracker.find(standalone_name) ==
          g_standalone_tracker.end()) {
        g_standalone_tracker[standalone_name] = 0;
      }
      uint32_t index = g_standalone_tracker[standalone_name];
      g_standalone_tracker[standalone_name] =
          g_standalone_tracker[standalone_name] + 1;
      standalone_name = stringf("%s#%d", standalone_name.c_str(), index);
    }
  }
  std::string linked_object() const {
    std::string name = "";
    if (db->is_standalone() || db->is_fabric_clkbuf()) {
      name = standalone_name;
    } else {
      for (auto port : port_infos) {
        name = stringf("%s+%s", name.c_str(),
                       get_original_name(port.realname).c_str());
      }
      name.erase(0, 1);
    }
    return sort_name(name);
  }
  std::vector<std::string> linked_objects() const {
    std::vector<std::string> names;
    if (db->is_standalone() || db->is_fabric_clkbuf()) {
      names.push_back(standalone_name);
    } else {
      for (auto port : port_infos) {
        names.push_back(get_original_name(port.realname));
      }
    }
    return names;
  }
  // Constructor
  const std::vector<PORT_INFO> port_infos;
  const IO_DIR dir = IO_DIR::UNKNOWN;
  std::string standalone_name = "";
  int chain_data_width = -2;
};

/*
  Structure of instance that dumped into JSON
*/
struct INSTANCE {
  INSTANCE(const std::string& m, const std::string& n,
           std::vector<std::string> ls, const PRIMITIVE* p,
           const std::string& pre, std::vector<std::string> post,
           std::map<std::string, std::vector<const PRIMITIVE*>> gcs)
      : module(get_original_name(m)),
        name(get_original_name(n)),
        linked_objects(ls),
        primitive(p),
        pre_primitive(pre),
        post_primitives(post) {
    log_assert(linked_objects.size());
    for (auto o : linked_objects) {
      properties[o] = {};
      locations[o] = "";
    }
    for (auto& gc : gcs) {
      gearbox_clocks[gc.first] = std::vector<std::string>({});
      for (auto& p : gc.second) {
        gearbox_clocks[gc.first].push_back(get_original_name(p->name));
      }
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
    return sort_name(name);
  }
  std::string get_location_object_with_priority(
      std::vector<std::string> ports = {"I_P", "O_P"}) const {
    // Check the design port name from the connectivity
    std::string location_object = "";
    if (primitive != nullptr) {
      // none-WIRE
      if (primitive->grandparent == nullptr) {
        std::string design_object = "";
        for (auto port : ports) {
          if (connections.find(port) != connections.end()) {
            design_object = connections.at(port);
            break;
          }
        }
        if (design_object.size() > 0 &&
            locations.find(design_object) != locations.end() &&
            locations.at(design_object).size() > 0) {
          // good
          location_object = design_object;
        } else {
          // Just loop through locations, first non-empty will be used
          for (auto iter : locations) {
            if (iter.second.size()) {
              location_object = iter.first;
              break;
            }
          }
        }
        if (location_object.size() == 0 && design_object.size() > 0) {
          location_object = design_object;
        }
        if (location_object.size() == 0) {
          // Just loop through locations, first will be used
          for (auto iter : locations) {
            location_object = iter.first;
            break;
          }
        }
      } else {
        // This checking prevent unlimited recursive loop
        log_assert(primitive->grandparent->instance != nullptr);
        log_assert(primitive->grandparent->instance->primitive != nullptr);
        log_assert(primitive->grandparent->instance->primitive->grandparent ==
                   nullptr);
        location_object =
            primitive->grandparent->instance->get_location_object_with_priority(
                ports);
      }
    }
    return location_object;
  }
  std::string get_location_with_priority(std::vector<std::string> ports = {
                                             "I_P", "O_P"}) const {
    // Check the design port name from the connectivity
    std::string location = "";
    if (primitive != nullptr) {
      // none-WIRE
      if (primitive->grandparent == nullptr) {
        std::string location_object = get_location_object_with_priority(ports);
        if (location_object.size()) {
          log_assert(locations.find(location_object) != locations.end());
          location = locations.at(location_object);
        }
      } else {
        // This checking prevent unlimited recursive loop
        log_assert(primitive->grandparent->instance != nullptr);
        log_assert(primitive->grandparent->instance->primitive != nullptr);
        log_assert(primitive->grandparent->instance->primitive->grandparent ==
                   nullptr);
        location =
            primitive->grandparent->instance->get_location_with_priority(ports);
      }
    }
    return location;
  }
  const std::string module = "";
  const std::string name = "";
  const std::vector<std::string> linked_objects;
  const PRIMITIVE* primitive = nullptr;
  const std::string pre_primitive = "";
  const std::vector<std::string> post_primitives;
  std::map<std::string, std::vector<std::string>> gearbox_clocks;
  std::map<std::string, std::string> connections;
  std::map<std::string, std::string> parameters;
  std::map<std::string, std::string> locations;
  std::map<std::string, std::map<std::string, std::string>> properties;
};

/*
  Structure that store pin information
*/
struct PIN_PORT {
  PIN_PORT(std::string n, bool i, bool s, bool f)
      : name(n), is_input(i), is_standalone(s), is_fabric_clkbuf(f) {}
  const std::string name = "";
  const bool is_input = false;
  const bool is_standalone = false;
  const bool is_fabric_clkbuf = false;
  std::string location = "";
  std::string mode = "";
  std::string internal_pin = "";
  std::vector<std::string> traces;
  std::vector<std::string> full_traces;
  std::string skip_reason = "";
};

/*
  Structure Fabric Clock
  Mainly used to track how the original JSON (io_config.json) mapped to wrapped
  JSON (config.json)
*/
struct FABRIC_CLOCK {
  FABRIC_CLOCK(const std::string& l, const std::string& m, const std::string& i,
               const std::string& ip, const std::string& op,
               const std::string& in, const std::string& on, bool fc)
      : linked_object(l),
        module(m),
        name(i),
        iport(ip),
        oport(op),
        inet(in),
        onet(on),
        is_fabric_clkbuf(fc) {}
  const std::string linked_object = "";
  const std::string module = "";
  const std::string name = "";
  const std::string iport = "";
  const std::string oport = "";
  const std::string inet = "";
  const std::string onet = "";
  const bool is_fabric_clkbuf = false;
};

/*
  Structure to store core clock information
*/
struct CORE_CLOCK_INFO {
  CORE_CLOCK_INFO(const std::string& l, uint32_t i) : location(l), index(i) {}
  const std::string location = "";
  const uint32_t index = 0;
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
  while (m_fabric_clocks.size()) {
    delete m_fabric_clocks.back();
    m_fabric_clocks.pop_back();
  }
  while (m_pin_infos.size()) {
    delete m_pin_infos.back();
    m_pin_infos.pop_back();
  }
}

/*
  Entry point of EXTRACTOR to extract
*/
bool PRIMITIVES_EXTRACTOR::extract(RTLIL::Design* design) {
  // Step 1: Misc - dump rtlil for easier debug
  run_pass("write_rtlil design.rtlil", design);
  g_standalone_tracker.clear();

  // Step 2: Make sure the technology is supported (check in constructor)
  if (!m_status) {
    goto EXTRACT_END;
  }

  // Step 3: Get Input and Output ports
  if (!get_ports(design->top_module())) {
    goto EXTRACT_END;
  }

  // Step 4: Trace CLK_BUF connection
  trace_next_primitive(design->top_module(), "\\I_BUF", "\\CLK_BUF");
  trace_next_primitive(design->top_module(), "\\I_BUF_DS", "\\CLK_BUF");

  // Step 5: Trace PLL connection
  trace_next_primitive(design->top_module(), "\\CLK_BUF", "\\PLL");
  trace_next_primitive(design->top_module(), "\\BOOT_CLOCK", "\\PLL");

  // Step 6: Trace primitives that might go to I_DELAY and I_DDR
  for (auto input :
       std::vector<std::string>({"\\I_BUF", "\\I_BUF_DS", "\\I_DELAY"})) {
    for (auto output :
         std::vector<std::string>({"\\I_DELAY", "\\I_DDR", "\\I_SERDES"})) {
      if (input != output) {
        trace_next_primitive(design->top_module(), input, output);
      }
    }
  }

  // Step 7: Trace primitives that might go to O_DELAY and O_DDR
  for (auto input : std::vector<std::string>(
           {"\\O_BUF", "\\O_BUFT", "\\O_BUF_DS", "\\O_BUFT_DS", "\\O_DELAY"})) {
    for (auto output :
         std::vector<std::string>({"\\O_DELAY", "\\O_DDR", "\\O_SERDES"})) {
      if (input != output) {
        trace_next_primitive(design->top_module(), input, output);
      }
    }
  }

  // Step 8: Support of O_SERDES_CLK
  for (auto input : std::vector<std::string>(
           {"\\O_BUF", "\\O_BUFT", "\\O_BUF_DS", "\\O_BUFT_DS"})) {
    trace_next_primitive(design->top_module(), input, "\\O_SERDES_CLK");
  }

  // Step 9: Support more primitive once more use cases are understood

  // Step 10: Let's make fabric clock buffer as the last item to trace (need to
  // add to the chain)
  trace_fabric_clkbuf(design->top_module());

  // Step 11: Trace primitive that the clock need to routed to gearbox (does not
  // need to add to the chain)
  trace_gearbox_clock();

  // Lastly generate instance(s)
  if (m_status) {
    gen_instances();
    determine_fabric_clock(design->top_module());
    summarize();
    finalize(design->top_module());
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
  Store the message (used for SDC)
*/
void PRIMITIVES_EXTRACTOR::post_sdc_comment(SDC_ENTRY*& entry, uint32_t offset,
                                            const std::string& type,
                                            const std::string& comment) {
  log_assert(type == "Skip" || type == "Fail");
  POST_MSG(offset, "%s reason: %s", type.c_str(), comment.c_str());
  entry->comments.push_back(
      stringf("# %s reason: %s", type.c_str(), comment.c_str()));
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
    if (dir == IO_DIR::IN || dir == IO_DIR::OUT || dir == IO_DIR::INOUT) {
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
        port_infos.push_back(PORT_INFO(dir == IO_DIR::INOUT ? IO_DIR::IN : dir,
                                       port_name, port_fullname, port_realname,
                                       wire->start_offset + index, index,
                                       (uint32_t)(wire->width)));
        if (dir == IO_DIR::INOUT) {
          port_infos.push_back(PORT_INFO(
              IO_DIR::OUT, port_name, port_fullname, port_realname,
              wire->start_offset + index, index, (uint32_t)(wire->width)));
        }
      }
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
    const std::string& name, PRIMITIVE_REQ req) {
  const PRIMITIVE_DB* db = nullptr;
  for (auto& d : SUPPORTED_PRIMITIVES.at(m_technology)) {
    if (d.is_ready() && d.name == name) {
      if (req == PRIMITIVE_REQ::DONT_CARE ||
          (req == PRIMITIVE_REQ::IS_PORT && d.is_port()) ||
          (req == PRIMITIVE_REQ::NOT_PORT && !d.is_port()) ||
          (req == PRIMITIVE_REQ::IS_STANDALONE && d.is_standalone()) ||
          (req == PRIMITIVE_REQ::IS_FABRIC_CLKBUF && d.is_fabric_clkbuf())) {
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
  if (primitive->db->properties.find("MUST_HAVE_PARAMS") !=
      primitive->db->properties.end()) {
    std::string param = primitive->db->properties.at("MUST_HAVE_PARAMS");
    log_assert(param.size());
    std::vector<std::string> params = split_string(param, ";");
    for (auto& p : params) {
      log_assert(primitive->parameters.find(p) != primitive->parameters.end());
    }
  }
  if (primitive->db->properties.find("DEFINE_DATA_WIDTH_FROM_PARAMS") !=
      primitive->db->properties.end()) {
    std::string param =
        primitive->db->properties.at("DEFINE_DATA_WIDTH_FROM_PARAMS");
    log_assert(param.size());
    log_assert(primitive->parameters.find(param) !=
               primitive->parameters.end());
    param = primitive->parameters.at(param);
    primitive->data_width = std::stoi(param);
    log_assert(primitive->data_width > 0);
    log_assert(!primitive->is_port_primitive);
    log_assert(primitive->grandparent != nullptr);
    PRIMITIVE* ptr = const_cast<PRIMITIVE*>(primitive->grandparent);
    log_assert(ptr->is_port_primitive);
    PORT_PRIMITIVE* port = reinterpret_cast<PORT_PRIMITIVE*>(ptr);
    port->chain_data_width = primitive->data_width;
  }
  for (auto& it : primitive->parameters) {
    POST_MSG(4, "Parameter %s: %s", it.first.c_str(), it.second.c_str());
  }
  POST_MSG(4, "Data Width: %d", primitive->data_width);
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
  log_assert(db->is_port() || db->is_standalone());
  log_assert(cell->type.str() == db->name);
  std::vector<std::string> checking_ports = db->get_checking_ports();
  log_assert(checking_ports.size() != 0 || db->is_standalone());
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
      if ((db->is_in_dir() && is_input) || (db->is_out_dir() && is_output)) {
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
  size_t input_connections = 0;
  size_t output_connections = 0;
  std::map<std::string, std::string> connections;
  for (auto& it : cell->connections()) {
    bool is_input = false;
    bool is_output = false;
    if ((is_input = (std::find(db->inputs.begin(), db->inputs.end(),
                               it.first.str()) != db->inputs.end())) ||
        (is_output = (std::find(db->outputs.begin(), db->outputs.end(),
                                it.first.str()) != db->outputs.end()))) {
      log_assert(is_input ^ is_output);
      std::ostringstream wire;
      RTLIL_BACKEND::dump_sigspec(wire, it.second, true, true);
      connections[it.first.str()] = wire.str();
      if (is_input) {
        input_connections++;
      }
      if (is_output) {
        output_connections++;
      }
    }
  }
  if ((db->inputs.size() == input_connections ||
       (db->is_any_inputs() && input_connections > 0) ||
       db->is_optional_input()) &&
      (db->outputs.size() == output_connections ||
       (db->is_any_outputs() && output_connections > 0) ||
       db->is_optional_output())) {
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
  POST_MSG(1, "Get Port/Standalone Primitives");
  for (auto cell : module->cells()) {
    const PRIMITIVE_DB* db =
        is_supported_primitive(cell->type.str(), PRIMITIVE_REQ::IS_PORT);
    if (db == nullptr) {
      db = is_supported_primitive(cell->type.str(),
                                  PRIMITIVE_REQ::IS_STANDALONE);
    }
    if (db != nullptr) {
      bool status = true;
      std::map<std::string, std::string> primary_connections;
      std::map<std::string, std::string> secondary_connections;
      if (get_port_cell_connections(cell, db, primary_connections,
                                    secondary_connections)) {
        // Expect PORT primitive should direct connect to input/output port
        std::vector<PORT_INFO> connected_ports;
        for (auto iter : primary_connections) {
          if (!get_connected_port(module, iter.first, iter.second,
                                  db->is_in_dir() ? IO_DIR::IN : IO_DIR::OUT,
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

/*
  Check if the input/output port is connected to primitive port
*/
bool PRIMITIVES_EXTRACTOR::get_connected_port(
    Yosys::RTLIL::Module* module, const std::string& cell_port_name,
    const std::string& connection, IO_DIR dir,
    std::vector<PORT_INFO>& port_infos, std::vector<size_t>& port_trackers,
    std::vector<PORT_INFO>& connected_ports, int loop) {
  bool status = true;
  log_assert(port_trackers.size() <= port_infos.size());
  size_t index = 0;
  while (index < port_infos.size()) {
    if (connection == port_infos[index].fullname &&
        dir == port_infos[index].dir) {
      POST_MSG(3, "Cell port %s is connected to %s port %s",
               cell_port_name.c_str(),
               get_dir_name(port_infos[index].dir).c_str(),
               port_infos[index].fullname.c_str());
      connected_ports.push_back(port_infos[index]);
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
  Entry function to trace next generic primitive
*/
void PRIMITIVES_EXTRACTOR::trace_next_primitive(
    Yosys::RTLIL::Module* module, const std::string& src_primitive_name,
    const std::string& dest_primitive_name) {
  POST_MSG(1, "Trace %s --> %s", src_primitive_name.c_str(),
           dest_primitive_name.c_str());
  std::vector<PRIMITIVE*> src_primitives;
  const PRIMITIVE_DB* src_primitive =
      PRIMITIVES_EXTRACTOR::is_supported_primitive(src_primitive_name,
                                                   PRIMITIVE_REQ::DONT_CARE);
  log_assert(src_primitive != nullptr);
  if (src_primitive->is_port() || src_primitive->is_standalone()) {
    for (auto& p : m_ports) {
      src_primitives.push_back((PRIMITIVE*)(p));
    }
  } else {
    for (auto& c : m_child_primitives) {
      src_primitives.push_back(c);
    }
  }
  for (PRIMITIVE*& primitive : src_primitives) {
    for (auto cell : module->cells()) {
      if (primitive->db->name == src_primitive_name &&
          cell->type.str() == dest_primitive_name) {
        std::string trace_connection = primitive->get_outtrace_connection();
#if ENABLE_DEBUG_MSG == 0
        size_t original_msg_size = m_msgs.size();
#endif
        POST_MSG(2, "Try %s %s out connection: %s -> %s",
                 primitive->db->name.c_str(), primitive->name.c_str(),
                 trace_connection.c_str(), cell->name.c_str());
        bool found =
            trace_next_primitive(module, primitive, cell, trace_connection);
        if (found) {
          for (auto& a : primitive->child_connections[cell->name.str()]) {
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
}

/*
  Helper function to trace generic primitive (normally internal not directly
  connected to port)
*/
bool PRIMITIVES_EXTRACTOR::trace_next_primitive(Yosys::RTLIL::Module* module,
                                                PRIMITIVE*& parent,
                                                Yosys::RTLIL::Cell* cell,
                                                const std::string& connection) {
  log_assert(parent->child.find(cell->name.str()) == parent->child.end());
  const PRIMITIVE_DB* db =
      is_supported_primitive(cell->type.str(), PRIMITIVE_REQ::NOT_PORT);
  log_assert(db != nullptr);
  bool found = false;
  std::map<std::string, std::string> connections =
      is_connected_cell(cell, db, connection);
  if (connections.size()) {
    POST_MSG(3, "Connected %s", cell->name.c_str());
    m_child_primitives.push_back(
        new PRIMITIVE(db, cell->name.str(), parent, connections, false));
    parent->child[cell->name.str()] = m_child_primitives.back();
    get_primitive_parameters(cell, m_child_primitives.back());
    found = true;
  }
  if (!found) {
    for (auto it : module->connections()) {
      std::vector<std::string> left_signals;
      std::vector<std::string> right_signals;
      get_signals(it.first, left_signals);
      get_signals(it.second, right_signals);
      log_assert(left_signals.size() == right_signals.size());
      for (size_t i = 0; i < right_signals.size(); i++) {
        std::string src = db->is_in_dir() ? right_signals[i] : left_signals[i];
        std::string dest = db->is_in_dir() ? left_signals[i] : right_signals[i];
        if (src == connection) {
          found = trace_next_primitive(module, parent, cell, dest);
          if (found) {
            if (parent->child_connections.find(cell->name.str()) ==
                parent->child_connections.end()) {
              parent->child_connections[cell->name.str()] = {};
            }
            parent->child_connections[cell->name.str()].insert(
                parent->child_connections[cell->name.str()].begin(), dest);
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
  Function to trace primitive which is fabric clock buffer (fabric -> GB ->
  fabric)
*/
void PRIMITIVES_EXTRACTOR::trace_fabric_clkbuf(Yosys::RTLIL::Module* module) {
  POST_MSG(1, "Trace fabric clock buffer");
  for (auto cell : module->cells()) {
    const PRIMITIVE_DB* db = is_supported_primitive(
        cell->type.str(), PRIMITIVE_REQ::IS_FABRIC_CLKBUF);
    if (db != nullptr) {
      // Currently only support one input and one output
      log_assert(db->inputs.size() == 1);  // must be coming from fabric
      log_assert(db->outputs.size() ==
                 1);  // must be going to fabric, cannot be routed to anywhere
      std::string input_net = "";
      std::string output_net = "";
      std::map<std::string, std::string> connections;
      for (auto& it : cell->connections()) {
        if (it.first.str() == db->inputs[0]) {
          std::vector<std::string> signals;
          get_signals(it.second, signals);
          if (signals.size() == 1 && is_real_net(signals[0])) {
            input_net = signals[0];
            connections[it.first.str()] = input_net;
          }
        } else if (it.first.str() == db->outputs[0]) {
          std::vector<std::string> signals;
          get_signals(it.second, signals);
          if (signals.size() == 1 && is_real_net(signals[0])) {
            output_net = signals[0];
            connections[it.first.str()] = output_net;
          }
        }
      }
      if (input_net.size() > 0 && output_net.size() > 0) {
        bool input_connected_to_fabric = false;
        bool output_connected_to_fabric = false;
        for (auto secondary_cell : module->cells()) {
          const PRIMITIVE_DB* fdb = is_supported_primitive(
              secondary_cell->type.str(), PRIMITIVE_REQ::DONT_CARE);
          if (fdb == nullptr) {
            // This is not primitive, must be fabric then
            for (auto& it : secondary_cell->connections()) {
              std::vector<std::string> signals;
              get_signals(it.second, signals);
              if (signals.size() == 1) {
                if (signals[0] == input_net) {
                  input_connected_to_fabric = true;
                } else if (signals[0] == output_net) {
                  output_connected_to_fabric = true;
                }
              }
            }
            if (input_connected_to_fabric && output_connected_to_fabric) {
              break;
            }
          }
        }
        if (input_connected_to_fabric && output_connected_to_fabric) {
          // This is fabric clock buffer
          POST_MSG(2, "Detect fabric clock buffer");
          for (auto& iter : connections) {
            POST_MSG(3, "%s : %s", iter.first.c_str(), iter.second.c_str());
          }
          m_ports.push_back(
              new PORT_PRIMITIVE(db, cell->name.str(), connections, {}));
        }
      }
    }
  }
}

/*
  Function to trace primitive which clock need to route to gearbox
*/
void PRIMITIVES_EXTRACTOR::trace_gearbox_clock() {
  POST_MSG(1, "Trace gearbox fast clock source");
  for (auto& primitive : m_child_primitives) {
    if (primitive->db->fast_clock.size()) {
      log_assert(!primitive->db->is_clock());
      log_assert(primitive->connections.find(primitive->db->fast_clock) !=
                 primitive->connections.end());
      std::string clock = primitive->connections.at(primitive->db->fast_clock);
      POST_MSG(2, "%s %s port %s: %s", primitive->db->name.c_str(),
               primitive->name.c_str(), primitive->db->fast_clock.c_str(),
               clock.c_str());
      bool found = false;
      for (auto& clock_primitive : m_child_primitives) {
        if (clock_primitive->db->is_gearbox_clock()) {
          for (auto& clock_o : clock_primitive->db->outputs) {
            if (clock_primitive->connections.find(clock_o) !=
                    clock_primitive->connections.end() &&
                clock == clock_primitive->connections.at(clock_o)) {
              POST_MSG(3, "Connected to %s %s port %s",
                       clock_primitive->db->name.c_str(),
                       clock_primitive->name.c_str(), clock_o.c_str());
              std::string port_name = get_original_name(clock_o);
              if (clock_primitive->gearbox_clocks.find(port_name) ==
                  clock_primitive->gearbox_clocks.end()) {
                clock_primitive->gearbox_clocks[port_name] =
                    std::vector<const PRIMITIVE*>({});
              }
              clock_primitive->gearbox_clocks[port_name].push_back(primitive);
              found = true;
              break;
            }
          }
        }
        if (found) {
          break;
        }
      }
      if (!found) {
        std::string msg =
            stringf("Not able to route signal %s to port %s", clock.c_str(),
                    primitive->db->fast_clock.c_str());
        POST_MSG(3, "Warning: %s", msg.c_str());
        primitive->errors.push_back(msg);
      }
    }
  }
}

/*
  Get the chunk bit by bit
*/
void PRIMITIVES_EXTRACTOR::get_chunks(const Yosys::RTLIL::SigChunk& chunk,
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
  Generate instances that being used in JSON
*/
void PRIMITIVES_EXTRACTOR::gen_instances() {
  log_assert(m_status);
  log_assert(m_instances.size() == 0);
  for (PORT_PRIMITIVE*& port : m_ports) {
    PRIMITIVE* primitive = (PRIMITIVE*)(port);
    gen_instances(port->linked_object(), port->linked_objects(), primitive, "");
  }
}

/*
  Generate instances (recursive for children) that being used in JSON
*/
void PRIMITIVES_EXTRACTOR::gen_instances(
    const std::string& linked_object, std::vector<std::string> linked_objects,
    const PRIMITIVE* primitive, const std::string& pre_primitive) {
  log_assert(m_status);
  std::string primitive_type = get_original_name(primitive->db->name);
#if GENERATION_ALWAYS_INWARD_DIRECTION == 0
  if (primitive->db->dir == IO_DIR::IN) {
#endif
    // Generate instance: parent first then child
    if (primitive->is_port_primitive) {
      gen_instance(linked_objects, primitive, pre_primitive);
    }
    for (auto child : primitive->child) {
      gen_wire(linked_object, linked_objects, primitive, child.first);
      gen_instance(linked_objects, child.second, primitive_type);
      gen_instances(linked_object, linked_objects, child.second,
                    primitive_type);
    }
#if GENERATION_ALWAYS_INWARD_DIRECTION == 0
  } else {
    // Reverse the sequence to generate instance, child first, then parent
    for (auto child : primitive->child) {
      gen_instances(linked_object, linked_objects, child.second,
                    primitive_type);
      gen_instance(linked_objects, child.second, primitive_type);
      gen_wire(linked_object, linked_objects, primitive, child.first);
    }
    if (primitive->is_port_primitive) {
      gen_instance(linked_objects, primitive, pre_primitive);
    }
  }
#endif
}

/*
  Generate instance that being used in JSON
*/
void PRIMITIVES_EXTRACTOR::gen_instance(std::vector<std::string> linked_objects,
                                        const PRIMITIVE* primitive,
                                        const std::string& pre_primitive) {
  std::vector<std::string> child_primitive_type;
  for (auto child : primitive->child) {
    child_primitive_type.push_back(get_original_name(child.second->db->name));
  }
  m_instances.push_back(new INSTANCE(
      primitive->db->name, primitive->name, linked_objects, primitive,
      pre_primitive, child_primitive_type, primitive->gearbox_clocks));
  m_instances.back()->add_connections(primitive->connections);
  m_instances.back()->add_parameters(primitive->parameters);
  primitive->set_instance(m_instances.back());
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
      m_instances.push_back(new INSTANCE("WIRE", primitive_name, linked_objects,
                                         nullptr, "", {}, {}));
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
    std::unordered_map<std::string, std::string>& properties,
    const std::string& internal_pin) {
  POST_MSG(1, "Assign location %s (and properties) to Port %s",
           location.c_str(), port.c_str());
  for (auto& instance : m_instances) {
    if (std::find(instance->linked_objects.begin(),
                  instance->linked_objects.end(),
                  port) != instance->linked_objects.end()) {
      PIN_PORT* p = get_pin_info(port, instance->primitive->db->is_in_dir()
                                           ? IO_DIR::IN
                                           : IO_DIR::OUT);
      log_assert(p != nullptr);
      p->location = location;
      p->internal_pin = internal_pin;
      instance->locations[port] = location;
      if (instance->primitive != nullptr &&
          instance->primitive->is_port_primitive) {
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
  Query the location of a primitive by the instance/module/cell name
*/
std::vector<std::string> PRIMITIVES_EXTRACTOR::get_primitive_locations_by_name(
    const std::string& name, bool unique_location) {
  std::vector<std::string> locations;
  for (auto& instance : m_instances) {
    if (instance->name == name) {
      if (unique_location) {
        std::string location = instance->get_location_with_priority();
        locations.push_back(location);
      } else {
        for (auto& location : instance->locations) {
          if (!location.second.empty()) {
            locations.push_back(location.second);
          }
        }
      }
      break;
    }
  }
  return locations;
}

/*
  Auto determine the clock
*/
void PRIMITIVES_EXTRACTOR::determine_fabric_clock(
    Yosys::RTLIL::Module* module) {
  log_assert(m_status);
  // log_assert(m_instances.size());
  POST_MSG(1, "Trace Fabric Clock");
  for (auto& instance : m_instances) {
    if (instance->primitive->db->is_clock() ||
        instance->primitive->db->is_fabric_clkbuf()) {
      // If it is clock, the direction should be in
      log_assert(instance->primitive->db->is_in_dir());
      log_assert(instance->primitive->db->outputs.size());
      size_t i = 0;
      for (auto& out : instance->primitive->db->outputs) {
        if (instance->primitive->connections.find(out) !=
            instance->primitive->connections.end()) {
          // Input
          log_assert(instance->primitive->db->inputs.size() == 0 ||
                     instance->primitive->db->inputs.size() == 1);
          std::string iport = "";
          std::string inet = "";
          if (instance->primitive->db->inputs.size()) {
            iport = get_original_name(instance->primitive->db->inputs[0]);
            log_assert(instance->connections.find(iport) !=
                       instance->connections.end());
            inet = instance->connections[iport];
          }
          // Output
          std::string oport = get_original_name(out);
          log_assert(instance->connections.find(oport) !=
                     instance->connections.end());
          std::string onet = instance->connections[oport];
          if (need_to_route_to_fabric(
                  module, instance->primitive->db->name,
                  instance->primitive->name, out,
                  instance->primitive->connections.at(out))) {
            std::string clock =
                stringf("%d", (uint32_t)(m_fabric_clocks.size()));
            std::string name = "ROUTE_TO_FABRIC_CLK";
            if (instance->primitive->db->outputs.size() > 1) {
              name = stringf("OUT%d_ROUTE_TO_FABRIC_CLK", (uint32_t)(i));
            }
            instance->parameters[name] = clock;
            for (auto object : instance->linked_objects) {
              log_assert(instance->properties.find(object) !=
                         instance->properties.end());
              instance->properties[object][name] = clock;
            }
            if (instance->primitive->db->is_fabric_clkbuf()) {
              log_assert(instance->primitive->is_port_primitive);
              const PORT_PRIMITIVE* port_primitive =
                  (const PORT_PRIMITIVE*)(instance->primitive);
              std::string from_clock = port_primitive->standalone_name;
              log_assert(from_clock.find("FABRIC_CLKBUF#") == 0);
              from_clock = from_clock.substr(14);
              log_assert(from_clock.size());
              instance->parameters["ROUTE_FROM_FABRIC_CLK"] = from_clock;
              for (auto object : instance->linked_objects) {
                log_assert(instance->properties.find(object) !=
                           instance->properties.end());
                instance->properties[object]["ROUTE_FROM_FABRIC_CLK"] =
                    from_clock;
              }
            }
            std::string linked_object = instance->linked_object();
            m_fabric_clocks.push_back(new FABRIC_CLOCK(
                linked_object, instance->module, instance->name, iport, oport,
                inet, onet, instance->primitive->db->is_fabric_clkbuf()));
          }
        }
        i++;
      }
    }
  }
}

/*
  Determine if the clock need to route to fabric
*/
bool PRIMITIVES_EXTRACTOR::need_to_route_to_fabric(
    Yosys::RTLIL::Module* module, const std::string& module_type,
    const std::string& module_name, const std::string& port_name,
    const std::string& net_name) {
  bool fabric = false;
  POST_MSG(2, "Module %s %s: clock port %s, net %s", module_type.c_str(),
           module_name.c_str(), port_name.c_str(), net_name.c_str());
  for (auto cell : module->cells()) {
    if (cell->name.str() != module_name) {
      for (auto& it : cell->connections()) {
        std::ostringstream wire;
        RTLIL_BACKEND::dump_sigspec(wire, it.second, true, true);
        if (wire.str() == net_name) {
          POST_MSG(3, "Connected to cell %s %s", cell->type.c_str(),
                   cell->name.c_str());
          const PRIMITIVE_DB* db = is_supported_primitive(
              cell->type.str(), PRIMITIVE_REQ::DONT_CARE);
          if (db != nullptr) {
            POST_MSG(4, "Which is a primitive");
            std::vector<std::string> source_modules;
            std::string core_clk = db->core_clock;
            size_t index = core_clk.find(":");
            if (index != std::string::npos) {
              std::string temp = db->core_clock.substr(0, index);
              core_clk = db->core_clock.substr(index + 1);
              index = temp.find(",");
              while (index != std::string::npos) {
                source_modules.push_back(temp.substr(0, index));
                temp = temp.substr(index + 1);
                index = temp.find(",");
              }
              source_modules.push_back(temp);
            }
            if (it.first.str() == core_clk &&
                (source_modules.size() == 0 ||
                 std::find(source_modules.begin(), source_modules.end(),
                           module_type) != source_modules.end())) {
              // For second check: even though it is not used by core_clk
              //    But we need to route it to fabric, in case only fabric can
              //    do something on it in IO Tile
              POST_MSG(4, "This is gearbox core_clk. Send to fabric");
              fabric = true;
            } else {
              POST_MSG(4,
                       "Does not meet core_clk checking criteria. Not sending "
                       "to fabric");
            }
          } else {
            // If it is not connected to primitive, then it must be fabric
            POST_MSG(4, "Which is not a IO primitive. Send to fabric");
            fabric = true;
          }
          if (fabric) {
            break;
          }
        }
      }
      if (fabric) {
        break;
      }
    }
  }
  return fabric;
}

/*
  Function to get pin info
*/
PIN_PORT* PRIMITIVES_EXTRACTOR::get_pin_info(const std::string& name,
                                             IO_DIR dir) {
  log_assert(dir == IO_DIR::IN || dir == IO_DIR::OUT);
  PIN_PORT* port = nullptr;
  for (auto& p : m_pin_infos) {
    if (p->name == name && ((p->is_input && dir == IO_DIR::IN) ||
                            (!p->is_input && dir == IO_DIR::OUT))) {
      port = p;
      break;
    }
  }
  return port;
}

/*
  Function to summarize what primitive connectivity
*/
void PRIMITIVES_EXTRACTOR::summarize() {
  POST_MSG(1, "Summary");
  log_assert(m_status);
  // log_assert(m_instances.size());
  m_max_in_object_name = 0;
  m_max_out_object_name = 0;
  m_max_object_name = 0;
  m_max_trace = 0;
  for (PORT_PRIMITIVE*& port : m_ports) {
    std::string object_name = port->linked_object();
    if (port->db->is_in_dir()) {
      if (int(object_name.size()) > m_max_in_object_name) {
        m_max_in_object_name = int(object_name.size());
      }
    } else {
      if (int(object_name.size()) > m_max_out_object_name) {
        m_max_out_object_name = int(object_name.size());
      }
    }
  }
  for (PORT_PRIMITIVE*& port : m_ports) {
    for (auto& object : port->linked_objects()) {
      if (int(object.size()) > m_max_object_name) {
        m_max_object_name = int(object.size());
      }
    }
  }

  for (PORT_PRIMITIVE*& port : m_ports) {
    PRIMITIVE* primitive = (PRIMITIVE*)(port);
    summarize(primitive, {get_original_name(port->db->name)},
              port->db->is_in_dir());
  }
  m_max_trace += 32;
  std::string dashes = "";
  std::string stars = "";
  while (dashes.size() < (size_t)(m_max_in_object_name + m_max_trace +
                                  m_max_out_object_name + 8)) {
    dashes.push_back('-');
  }
  while (stars.size() < (size_t)(m_max_trace + 4)) {
    stars.push_back('*');
  }
  POST_MSG(2, "    |%s|", dashes.c_str());
  POST_MSG(2, "    | %*s%s%*s |", m_max_in_object_name + 1, "", stars.c_str(),
           m_max_out_object_name + 1, "");
  for (PORT_PRIMITIVE*& port : m_ports) {
    for (auto& object : port->linked_objects()) {
      log_assert(get_pin_info(object, port->dir) == nullptr);
      m_pin_infos.push_back(new PIN_PORT(object, port->db->is_in_dir(),
                                         port->db->is_standalone(),
                                         port->db->is_fabric_clkbuf()));
    }
    PRIMITIVE* primitive = (PRIMITIVE*)(port);
    summarize(primitive, port->linked_object(), port->linked_objects(),
              {get_original_name(port->db->name)},
              {get_original_name(port->db->name)}, port->db->is_in_dir());
  }
  POST_MSG(2, "    | %*s%s%*s |", m_max_in_object_name + 1, "", stars.c_str(),
           m_max_out_object_name + 1, "");
  POST_MSG(2, "    |%s|", dashes.c_str());
}

/*
  Function to summarize what primitive connectivity (recursive for children)
  This only calculate the string size
*/
void PRIMITIVES_EXTRACTOR::summarize(const PRIMITIVE* primitive,
                                     const std::vector<std::string> traces,
                                     bool is_in_dir) {
  log_assert(traces.size());
  if (primitive->child.size()) {
    for (auto child : primitive->child) {
      log_assert(is_in_dir == child.second->db->is_in_dir());
      std::vector<std::string> temp = traces;
      temp.push_back(get_original_name(child.second->db->name));
      summarize(child.second, temp, is_in_dir);
    }
  } else {
    std::string trace = "";
    for (auto t : traces) {
      log_assert(t.size());
      if (trace.size()) {
        trace = stringf("%s -> %s", trace.c_str(), t.c_str());
      } else {
        trace = t;
      }
    }
    if ((int)(trace.size()) > m_max_trace) {
      m_max_trace = (int)(trace.size());
    }
  }
}

/*
  Function to summarize what primitive connectivity (recursive for children)
*/
void PRIMITIVES_EXTRACTOR::summarize(const PRIMITIVE* primitive,
                                     const std::string& object_name,
                                     const std::vector<std::string> objects,
                                     const std::vector<std::string> traces,
                                     const std::vector<std::string> full_traces,
                                     bool is_in_dir) {
  log_assert(traces.size());
  for (auto& object : objects) {
    update_pin_info(object, primitive);
  }
  if (primitive->child.size()) {
    uint32_t i = 0;
    for (auto child : primitive->child) {
      log_assert(is_in_dir == child.second->db->is_in_dir());
      std::vector<std::string> temp;
      std::vector<std::string> fulltemp = full_traces;
      if (i == 0) {
        temp = traces;
      } else {
        int s = 0;
        for (auto t : traces) {
          log_assert(t.size());
          s += (int)(t.size());
        }
        s += int((traces.size() - 1) * 5);
        temp = {stringf("%*s", s, " ")};
      }
      temp.push_back(get_original_name(child.second->db->name));
      fulltemp.push_back(get_original_name(child.second->db->name));
      summarize(child.second, object_name, objects, temp, fulltemp, is_in_dir);
      i++;
    }
  } else {
    for (auto& object : objects) {
      PIN_PORT* p = get_pin_info(object, is_in_dir ? IO_DIR::IN : IO_DIR::OUT);
      log_assert(p != nullptr);
      update_pin_traces(p->traces, traces, is_in_dir);
      update_pin_traces(p->full_traces, full_traces, is_in_dir);
    }
    std::string trace = "";
    if (is_in_dir) {
      for (auto t = traces.begin(); t != traces.end(); t++) {
        log_assert(t->size());
        if (trace.size()) {
          trace = stringf("%s |-> %s", trace.c_str(), t->c_str());
        } else {
          trace = *t;
        }
      }
      bool is_child = true;
      for (auto c : traces.front()) {
        if (c != ' ') {
          is_child = false;
          break;
        }
      }
      if (is_child) {
        POST_MSG(2, "IN  | %*s * %-*s * %*s |", m_max_in_object_name, "",
                 m_max_trace, trace.c_str(), m_max_out_object_name, "");
      } else {
        POST_MSG(2, "IN  | %*s * %-*s * %*s |", m_max_in_object_name,
                 object_name.c_str(), m_max_trace, trace.c_str(),
                 m_max_out_object_name, "");
      }
    } else {
      for (auto t = traces.rbegin(); t != traces.rend(); t++) {
        log_assert(t->size());
        if (trace.size()) {
          trace = stringf("%s |-> %s", trace.c_str(), t->c_str());
        } else {
          trace = *t;
        }
      }
      POST_MSG(2, "OUT | %*s * %*s * %-*s |", m_max_in_object_name, "",
               m_max_trace, trace.c_str(), m_max_out_object_name,
               object_name.c_str());
    }
  }
}

/*
  Update pin mode
  Except the table and valid connection matrix, this is the only function that
  have hardcoded primitive. When I have time, will see how to make this
  data-driven
*/
void PRIMITIVES_EXTRACTOR::update_pin_info(const std::string& pin_name,
                                           const PRIMITIVE* primitive) {
  PIN_PORT* pin = get_pin_info(
      pin_name, primitive->db->is_in_dir() ? IO_DIR::IN : IO_DIR::OUT);
  log_assert(pin != nullptr);
  if (primitive->db->name == "\\I_DDR" || primitive->db->name == "\\O_DDR") {
    log_assert(pin->mode.size() == 0);
    pin->mode = "DDR";
  } else if (primitive->db->name == "\\I_SERDES" ||
             primitive->db->name == "\\O_SERDES" ||
             primitive->db->name == "\\O_SERDES_CLK") {
    log_assert(pin->mode.size() == 0);
    if (primitive->parameters.find("\\WIDTH") != primitive->parameters.end()) {
      int width = std::stoi(primitive->parameters.at("\\WIDTH"));
      pin->mode = stringf("RATE_%d", width);
    } else if (primitive->parameters.find("\\DATA_RATE") !=
               primitive->parameters.end()) {
      pin->mode = get_param_string(primitive->parameters.at("\\DATA_RATE"));
    }
    if (pin->mode.size() == 0) {
      // If not set, by default is SDR
      pin->mode = "SDR";
    }
    log_assert(pin->mode == "SDR" || pin->mode == "DDR" ||
               pin->mode.find("RATE_") == 0);
  }
  if (primitive->db->name == "\\I_BUF_DS" ||
      primitive->db->name == "\\O_BUF_DS" ||
      primitive->db->name == "\\O_BUFT_DS") {
    std::string secondary_port =
        primitive->db->name == "\\I_BUF_DS" ? "\\I_N" : "\\O_N";
    log_assert(primitive->connections.find(secondary_port) !=
               primitive->connections.end());
    std::string name =
        get_original_name(primitive->connections.at(secondary_port));
    if (name == pin_name) {
      pin->skip_reason =
          "This is secondary pin. But IO bitstream generation will still make "
          "sure it is used in pair. Otherwise the IO bitstream will be "
          "invalid";
    }
  }
}

/*
  Update pin traces
*/
void PRIMITIVES_EXTRACTOR::update_pin_traces(
    std::vector<std::string>& pin_traces, const std::vector<std::string> traces,
    bool is_in_dir) {
  std::string trace = "";
  if (is_in_dir) {
    for (auto t = traces.begin(); t != traces.end(); t++) {
      log_assert(t->size());
      if (trace.size()) {
        trace = stringf("%s |-> %s", trace.c_str(), t->c_str());
      } else {
        trace = *t;
      }
    }
  } else {
    for (auto t = traces.rbegin(); t != traces.rend(); t++) {
      log_assert(t->size());
      if (trace.size()) {
        trace = stringf("%s |-> %s", trace.c_str(), t->c_str());
      } else {
        trace = *t;
      }
    }
  }
  pin_traces.push_back(trace);
}

/*
  Function in final stage to check if there is mistake in the design (or code)
*/
void PRIMITIVES_EXTRACTOR::finalize(Yosys::RTLIL::Module* module) {
  size_t design_count = 0;
  size_t primitive_count = m_ports.size() + m_child_primitives.size();
  size_t instance_count = 0;
  for (auto cell : module->cells()) {
    if (is_supported_primitive(cell->type.str(), PRIMITIVE_REQ::DONT_CARE) !=
        nullptr) {
      design_count++;
    }
  }
  for (auto& inst : m_instances) {
    if (inst->module != "WIRE") {
      instance_count++;
    }
  }
  if (design_count == primitive_count && design_count == instance_count) {
    POST_MSG(1, "Final checking is good");
  } else {
    POST_MSG(1,
             "Error: Final checking failed. Design count: %ld, Primitive "
             "count: %ld, Instance count: %ld",
             design_count, primitive_count, instance_count);
    if (design_count != primitive_count) {
      for (auto cell : module->cells()) {
        if (is_supported_primitive(cell->type.str(),
                                   PRIMITIVE_REQ::DONT_CARE) != nullptr) {
          bool found = false;
          for (auto& p : m_ports) {
            if (p->name == cell->name.str()) {
              found = true;
              break;
            }
          }
          if (found) {
            continue;
          }
          for (auto& p : m_child_primitives) {
            if (p->name == cell->name.str()) {
              found = true;
              break;
            }
          }
          if (found) {
            continue;
          }
          POST_MSG(2, "Error: Missing %s (%s) in primitive list",
                   cell->type.c_str(), cell->name.c_str());
        }
      }
    }
    if (design_count != instance_count) {
      for (auto cell : module->cells()) {
        if (is_supported_primitive(cell->type.str(),
                                   PRIMITIVE_REQ::DONT_CARE) != nullptr) {
          bool found = false;
          for (auto& inst : m_instances) {
            if (inst->name == get_original_name(cell->name.str())) {
              found = true;
              break;
            }
          }
          if (found) {
            continue;
          }
          POST_MSG(2, "Error: Missing %s (%s) in instance list",
                   cell->type.c_str(), cell->name.c_str());
        }
      }
    }
  }
}

/*
  Write out message and instances information into JSON
*/
void PRIMITIVES_EXTRACTOR::write_json(const std::string& file, bool simple) {
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
      write_instance(instance, json, simple);
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
                                          std::ofstream& json, bool simple) {
  json << "\n    {\n";
  write_json_object(3, "module", instance->module, json);
  json << ",\n";
  write_json_object(3, "name", instance->name, json);
  json << ",\n";
  write_json_object(3, "location_object",
                    instance->get_location_object_with_priority(), json);
  json << ",\n";
  write_json_object(3, "location", instance->get_location_with_priority(),
                    json);
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
    write_instance_map(instance->properties.at(object), json, 6);
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
  if (simple) {
    json << "      }\n";
  } else {
    json << "      },\n";
    write_json_object(3, "pre_primitive", instance->pre_primitive, json);
    json << ",\n";
    json << "      \"post_primitives\" : [\n",
        write_instance_array(instance->post_primitives, json, 4);
    json << "      ],\n";
    index = 0;
    json << "      \"route_clock_to\" : {\n";
    for (auto c : instance->gearbox_clocks) {
      if (index) {
        json << ",\n";
      }
      json << "        \"" << c.first.c_str() << "\" : [\n";
      write_instance_array(c.second, json, 5);
      json << "        ]";
      index++;
    }
    if (index) {
      json << "\n";
    }
  }
  json << "      },\n";
  json << "      \"errors\" : [\n";
  write_instance_array(instance->primitive->errors, json, 4);
  json << "      ]\n";
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
    json << "\"";
    write_json_data(iter, json);
    json << "\"";
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

/*
  Write out fabric clock or mode SDC
*/
void PRIMITIVES_EXTRACTOR::write_sdc(const std::string& file,
                                     const nlohmann::json& wrapped_instances) {
#if ENABLE_INSTANCE_CROSS_CHECK
  POST_MSG(1, "Cross-check instances vs wrapped-instances");
  for (auto& inst : m_instances) {
    if (inst->module != "WIRE") {
      get_wrapped_instance(wrapped_instances, inst->name);
    }
  }
#endif
  POST_MSG(1, "Generate SDC");
  std::ofstream sdc(file.c_str());
  // Clock
  sdc << "#############\n";
  sdc << "#\n";
  sdc << "# Fabric clock assignment\n";
  sdc << "#\n";
  sdc << "#############\n";
  uint32_t i = 0;
  uint32_t j = 0;
  for (auto clk : m_fabric_clocks) {
    if (clk->is_fabric_clkbuf) {
      sdc << "# This is fabric clock buffer\n";
    }
    std::string wrapped_net = get_output_wrapped_net(
        wrapped_instances, get_wrapped_instance(wrapped_instances, clk->name),
        clk);
    if (!wrapped_net.size()) {
      sdc << "# Fail reason: Failed to find the mapped name\n";
    }
    // Always print the port name
    sdc << stringf(
               "# set_clock_pin -device_clock clk[%d] -design_clock "
               "%s (Physical port name)\n",
               i, clk->linked_object.c_str())
               .c_str();
    if (wrapped_net.size()) {
      sdc << stringf(
                 "# set_clock_pin -device_clock clk[%d] -design_clock "
                 "%s (Original clock primitive out-net to fabric)\n",
                 i, clk->onet.c_str())
                 .c_str();
      sdc << stringf(
                 "set_clock_pin   -device_clock clk[%d] -design_clock "
                 "%s\n",
                 i, wrapped_net.c_str())
                 .c_str();
    } else {
      sdc << stringf(
                 "set_clock_pin   -device_clock clk[%d] -design_clock "
                 "%s\n",
                 i, clk->onet.c_str())
                 .c_str();
    }
    if (clk->is_fabric_clkbuf) {
      sdc << "\n# For fabric clock buffer output\n";
      std::string wrapped_net = get_input_wrapped_net(
          wrapped_instances, get_wrapped_instance(wrapped_instances, clk->name),
          clk);
      if (wrapped_net.size()) {
        sdc << stringf(
                   "# set_clock_out -device_clock clk[%d] -design_clock "
                   "%s\n",
                   j, clk->inet.c_str())
                   .c_str();
        sdc << stringf(
                   "set_clock_out   -device_clock clk[%d] -design_clock "
                   "%s\n",
                   j, wrapped_net.c_str())
                   .c_str();
      } else {
        sdc << "# Fail reason: Failed to find the mapped name\n";
        sdc << stringf(
                   "set_clock_out   -device_clock clk[%d] -design_clock "
                   "%s\n",
                   j, clk->inet.c_str())
                   .c_str();
      }
      j++;
    }
    sdc << "\n";
    i++;
  }
  if (i == 0) {
    sdc << "\n";
  }
  // Mode
  sdc << "#############\n";
  sdc << "#\n";
  sdc << "# Each pin mode and location assignment\n";
  sdc << "#\n";
  sdc << "#############\n";
  // Consider {object_name}
  m_max_object_name += 2;
  // Consider maximum mode 11 + 5
  if (m_max_object_name < 16) {
    m_max_object_name = 16;
  }
  m_max_object_name += 1;  // For space
  // First column is max is "# set_mode" = 10 + 1
  POST_MSG(2, "Determine data signals");
  std::vector<SDC_ENTRY*> sdc_entries;
  for (auto& pin : m_pin_infos) {
    if (pin->is_standalone || pin->is_fabric_clkbuf) {
      continue;
    }
    SDC_ENTRY* entry = new SDC_ENTRY;
    size_t i = 0;
    for (auto& trace : pin->traces) {
      if (i == 0) {
        entry->assignments.push_back(
            SDC_ASSIGNMENT("# Pin", pin->name, ":: " + trace, ""));
      } else {
        entry->assignments.push_back(
            SDC_ASSIGNMENT("#", "", ":: " + trace, ""));
      }
      i++;
    }
    PIN_PARSER parsed_pin;
    POST_MSG(3, "Pin object=%s, location: %s", pin->name.c_str(),
             pin->location.c_str());
    if (validate_location(pin->location, parsed_pin)) {
      char ab = pin->location.back() == 'P' ? 'A' : 'B';
      std::string mode = "MODE_BP_DIR";
      if (pin->mode == "SDR") {
        mode = "MODE_BP_SDR";
      } else if (pin->mode == "DDR") {
        mode = "MODE_BP_DDR";
      } else if (pin->mode.find("RATE_") == 0) {
        mode = stringf("MODE_%s", pin->mode.c_str());
      }
      mode = stringf("%s_%c_%s", mode.c_str(), ab, pin->is_input ? "RX" : "TX");
      if (m_location_mode.find(pin->location) == m_location_mode.end()) {
        m_location_mode[pin->location] = mode;
      }
      if (pin->skip_reason.size()) {
        POST_MSG(4, "Skip this because \'%s\'", pin->skip_reason.c_str());
        entry->comments.push_back(
            stringf("# Skip this because \'%s\'", pin->skip_reason.c_str()));
      } else {
        std::vector<std::string> data_nets;
        std::vector<bool> found_data_nets;
        bool data_input = false;
        std::string data_reason =
            get_fabric_data(wrapped_instances, pin->name, data_nets,
                            found_data_nets, data_input);
        if (data_reason.size()) {
          if (data_reason.find("but data signal is not defined") ==
                  std::string::npos &&
              data_reason.find("Clock data from object") == std::string::npos) {
            entry->comments.push_back(
                stringf("# Fail reason: %s", data_reason.c_str()));
          } else {
            entry->comments.push_back(stringf("# %s", data_reason.c_str()));
          }
        } else {
          entry->assignments.push_back(
              SDC_ASSIGNMENT("# set_mode", mode, pin->location, ""));
          entry->assignments.push_back(SDC_ASSIGNMENT(
              "# set_io", pin->name, pin->location, "--> (original)"));
          std::string location =
              ab == 'A' ? pin->location
                        : stringf("H%s_%s%s_%d_%dP", parsed_pin.type.c_str(),
                                  parsed_pin.bank.c_str(),
                                  parsed_pin.is_clock ? "_CC" : "",
                                  parsed_pin.index - 1, parsed_pin.index / 2);
          for (size_t data_i = 0, data_j = (ab == 'A' ? 0 : 5);
               data_i < data_nets.size(); data_i++, data_j++) {
            entry->assignments.push_back(SDC_ASSIGNMENT(
                (std::string)(found_data_nets[data_i] ? "" : "# ") + "set_io",
                data_nets[data_i], location, "-mode", mode, "-internal_pin",
                stringf("%s[%ld]_A", data_input ? "g2f_rx_in" : "f2g_tx_out",
                        data_j)));
          }
        }
      }
    } else if (pin->location.size()) {
      POST_MSG(4, "Pin location is invalid");
      entry->comments.push_back("# Pin location is invalid");
    } else {
      POST_MSG(4, "Pin location is not assigned");
      entry->comments.push_back("# Pin location is not assigned");
    }
    sdc_entries.push_back(entry);
  }
  write_sdc_entries(sdc, sdc_entries);
#if 1
  // Internal control signals
  sdc << "#############\n";
  sdc << "#\n";
  sdc << "# Internal Control Signals\n";
  sdc << "#\n";
  sdc << "#############\n";
  POST_MSG(2, "Determine internal control signals");
  for (auto& inst : m_instances) {
    if (inst->module != "WIRE") {
      const PRIMITIVE_DB* db = inst->primitive->db;
      log_assert(db != nullptr);
      std::string linked_object = inst->linked_object();
      std::string location = inst->get_location_with_priority();
      for (auto iter : db->control_signal_map) {
        write_sdc_internal_control_signal(sdc_entries, wrapped_instances,
                                          inst->module, linked_object, location,
                                          iter.first, iter.second);
      }
    }
  }
  write_sdc_entries(sdc, sdc_entries);
#endif
  // Core Clocks
  sdc << "#############\n";
  sdc << "#\n";
  sdc << "# Each gearbox core clock\n";
  sdc << "#\n";
  sdc << "#############\n";
  std::map<std::string, CORE_CLOCK_INFO*> core_clocks;
  for (auto& inst : m_instances) {
    if (inst->module != "WIRE") {
      const PRIMITIVE_DB* db = inst->primitive->db;
      log_assert(db != nullptr);
      std::string core_clk = get_original_name(db->core_clock);
      size_t index = core_clk.find(":");
      if (index != std::string::npos) {
        core_clk = db->core_clock.substr(index + 1);
      }
      if (core_clk.size()) {
        SDC_ENTRY* entry = new SDC_ENTRY;
        std::string location = inst->get_location_with_priority({"I_P", "O_P"});
        entry->comments.push_back(
            stringf("# Module: %s", inst->module.c_str()));
        entry->comments.push_back(stringf("# Location: %s", location.c_str()));
        entry->comments.push_back(stringf("# Port: %s", core_clk.c_str()));
        if (inst->connections.find(core_clk) != inst->connections.end()) {
          std::string clk_net = inst->connections.at(core_clk);
          entry->comments.push_back(stringf("# Net: %s", clk_net.c_str()));
          if (clk_net.size() > 0) {
            if (location.size() > 0 &&
                m_location_mode.find(location) != m_location_mode.end()) {
              uint32_t index = 0;
              for (auto& fabric_clk : m_fabric_clocks) {
                if (!fabric_clk->is_fabric_clkbuf &&
                    fabric_clk->onet == clk_net) {
                  break;
                }
                index++;
              }
              if (index < (uint32_t)(m_fabric_clocks.size())) {
                PIN_PARSER parsed_pin;
                log_assert(validate_location(location, parsed_pin));
                std::string key =
                    stringf("%s_%s_%d", parsed_pin.type.c_str(),
                            parsed_pin.bank.c_str(), parsed_pin.index / 2);
                if (core_clocks.find(key) == core_clocks.end()) {
                  core_clocks[key] = new CORE_CLOCK_INFO(location, index);
                  entry->assignments.push_back(SDC_ASSIGNMENT(
                      "set_core_clk", location, stringf("%d", index), ""));
                } else if (core_clocks.at(key)->index == index) {
                  entry->comments.push_back(
                      stringf("# Skip reason: Had been defined by %s",
                              core_clocks.at(key)->location.c_str()));
                } else {
                  entry->comments.push_back(
                      stringf("# Fail reason: Conflict with %s (index=%d)",
                              core_clocks.at(key)->location.c_str(),
                              core_clocks.at(key)->index));
                }
              } else {
                entry->comments.push_back(
                    "# Fail reason: Cannot locate the fabric clock");
              }
            } else {
              entry->comments.push_back("# Fail reason: Location is invalid");
            }
          } else {
            entry->comments.push_back(
                "# Fail reason: Port does not connect to valid net");
          }
        } else {
          entry->comments.push_back(
              "# Fail reason: Port does not connect to valid net");
        }
        sdc_entries.push_back(entry);
      }
    }
  }
  write_sdc_entries(sdc, sdc_entries);
  for (auto& iter : core_clocks) {
    delete iter.second;
  }
  sdc.close();
}

void PRIMITIVES_EXTRACTOR::write_sdc_internal_control_signal(
    std::vector<SDC_ENTRY*>& sdc_entries,
    const nlohmann::json& wrapped_instances, const std::string& module,
    const std::string& linked_object, const std::string& location,
    const std::string& port, const std::string& internal_signal) {
  bool input = true;
  std::string internal_signal_name = "";
  if (internal_signal.find("in:") == 0) {
    internal_signal_name = internal_signal.substr(3);
  } else {
    log_assert(internal_signal.find("out:") == 0);
    internal_signal_name = internal_signal.substr(4);
    input = false;
  }
  log_assert(internal_signal_name.size());
  std::string info =
      stringf("Module=%s LinkedObject=%s Location=%s Port=%s Signal=%s",
              module.c_str(), linked_object.c_str(), location.c_str(),
              port.c_str(), internal_signal.c_str());
  POST_MSG(3, "%s", info.c_str());
  SDC_ENTRY* entry = new SDC_ENTRY;
#if 0
  entry->comments.push_back((std::string)("# ") + info);
#else
  entry->comments.push_back(stringf("# Module: %s", module.c_str()));
  entry->comments.push_back(
      stringf("# LinkedObject: %s", linked_object.c_str()));
  entry->comments.push_back(stringf("# Location: %s", location.c_str()));
  entry->comments.push_back(stringf("# Port: %s", port.c_str()));
  entry->comments.push_back(stringf("# Signal: %s", internal_signal.c_str()));
#endif
  // If the mode is available, then the location had been parsed correctly in
  // the first place
  if (m_location_mode.find(location) != m_location_mode.end()) {
    PIN_PARSER pin;
    log_assert(validate_location(location, pin));
    std::vector<std::string> wrapped_nets;
    std::pair<std::string, std::string> reason =
        get_wrapped_instance_net_by_port(wrapped_instances, module,
                                         linked_object, port, wrapped_nets);
    log_assert(reason.first.empty() == reason.second.empty());
    if (reason.first.empty()) {
      // Any subsequence wire
      get_wrapped_instance_potential_next_wire(wrapped_instances,
                                               input ? "O" : "I",
                                               input ? "I" : "O", wrapped_nets);
      std::vector<bool> founds =
          check_fabric_port(wrapped_instances, wrapped_nets);
      if (wrapped_nets.size() == founds.size()) {
        size_t wrapped_net_i = 0;
        std::string rule = "";
        if (internal_signal_name.find("rule=") == 0) {
          size_t index = internal_signal_name.find(":");
          log_assert(index != std::string::npos);
          rule = internal_signal_name.substr(5, index - 5);
          internal_signal_name = internal_signal_name.substr(index + 1);
          log_assert(rule.size());
          log_assert(internal_signal_name.size());
        }
        log_assert(rule == "" || rule == "half-first");
        std::string assigned_location =
            get_assigned_location(entry, rule, location, pin);
        log_assert(assigned_location.size());
        for (auto wrapped_net : wrapped_nets) {
          if (founds[wrapped_net_i]) {
            // Currently only support half-first
            std::string AB =
                assigned_location[assigned_location.size() - 1] == 'N' ? "B"
                                                                       : "A";
            std::string final_internal_signal = internal_signal_name;
            size_t index = final_internal_signal.find("{A|B}");
            if (index != std::string::npos) {
              final_internal_signal =
                  final_internal_signal.replace(index, 5, AB);
            }
            if (wrapped_nets.size() > 1) {
              index = final_internal_signal.find("{[");
              size_t eindex = final_internal_signal.find("]}");
              if (index != std::string::npos) {
                log_assert(eindex != std::string::npos);
                log_assert((index + 2) <= eindex);
                size_t sindex = 0;
                size_t size = 4;
                if ((index + 2) < eindex) {
                  size = eindex - index + 2;
                  std::string number = final_internal_signal.substr(
                      index + 2, eindex - index - 2);
                  log_assert(number.size());
                  sindex = (size_t)(std::stoi(number));
                }
                final_internal_signal = final_internal_signal.replace(
                    index, size, stringf("[%ld]", sindex + wrapped_net_i));
              } else {
                log_assert(eindex == std::string::npos);
                // Append at the end
                final_internal_signal = stringf(
                    "%s[%ld]", final_internal_signal.c_str(), wrapped_net_i);
              }
            }
            std::string skip = "";
            if (internal_signal_name == "TO_BE_DETERMINED") {
              post_sdc_comment(entry, 4, "Skip", "TO_BE_DETERMINED");
              skip = "# ";
            } else if (rule == "half-first") {
              log_assert(final_internal_signal.size());
              std::string unique =
                  stringf("%s+%s", final_internal_signal.c_str(),
                          assigned_location.c_str());
              bool found = std::find(m_auto_assigned_location.begin(),
                                     m_auto_assigned_location.end(),
                                     unique) != m_auto_assigned_location.end();
              if (found) {
                post_sdc_comment(entry, 4, "Fail",
                                 stringf("Assigned location %s and internal "
                                         "signal %s had already been used",
                                         assigned_location.c_str(),
                                         final_internal_signal.c_str()));
                skip = "# ";
              } else {
                m_auto_assigned_location.push_back(unique);
              }
            }
            if (skip == "") {
              if (std::find(m_internal_signal_net.begin(),
                            m_internal_signal_net.end(),
                            wrapped_net) == m_internal_signal_net.end()) {
                m_internal_signal_net.push_back(wrapped_net);
              } else {
                post_sdc_comment(entry, 4, "Fail",
                                 stringf("Found conflict internal signal: %s",
                                         wrapped_net.c_str()));
                skip = "# ";
              }
            }
            std::string set_io = stringf("%sset_io", skip.c_str());
            entry->assignments.push_back(
                SDC_ASSIGNMENT(set_io, wrapped_net, assigned_location, "-mode",
                               m_location_mode.at(location), "-internal_pin",
                               final_internal_signal));
          } else {
            post_sdc_comment(
                entry, 4, "Fail",
                stringf("Unable to trace fabric module connection: %s",
                        wrapped_net.c_str()));
          }
          wrapped_net_i++;
        }
      } else {
        log_assert(founds.size() == 0);
        post_sdc_comment(entry, 4, "Fail", "Unable find fabric instance");
      }
    } else {
      post_sdc_comment(entry, 4, reason.first, reason.second);
    }
  } else {
    std::string reason = stringf(
        "Location %s does not have any mode to begin with", location.c_str());
    post_sdc_comment(entry, 4, "Skip", reason);
  }
  sdc_entries.push_back(entry);
}

/*
  Validate the user assigned location
*/
bool PRIMITIVES_EXTRACTOR::validate_location(const std::string& location,
                                             PIN_PARSER& pin) {
  bool status = false;
  if (location.size()) {
    std::regex const e{"H([PR])_([1-6])_(CC_|)([0-9]+)_([0-9]+)([PN])"};
    std::smatch m;
    if (std::regex_match(location, m, e) && m.size() == 7) {
      // double check
      pin.type = m[1].str();
      log_assert(pin.type == "P" || pin.type == "R");
      // As long as they are 1-6
      pin.bank = m[2].str();
      // If it is clock
      pin.is_clock = m[3].str() == "CC_";
      // Index
      pin.index = std::stoi(m[4].str());
      if (pin.index >= 0 && pin.index < 40) {
        int pair_index = (int)(pin.index / 2);
        if (pair_index == std::stoi(m[5].str())) {
          if (((pin.index % 2) == 0 && m[6].str() == "P") ||
              ((pin.index % 2) == 1 && m[6].str() == "N")) {
            status = true;
          } else {
            pin.failure_reason =
                stringf("Location %s P/N is invalid", location.c_str());
          }
        } else {
          pin.failure_reason =
              stringf("Location %s pair index is invalid", location.c_str());
        }
      } else {
        pin.failure_reason =
            stringf("Location %s is range of index range", location.c_str());
      }
    } else {
      pin.failure_reason =
          stringf("Location %s does not meet regex", location.c_str());
    }
  }
  return status;
}

/*
  Function to get auto-determined assigned location
*/
std::string PRIMITIVES_EXTRACTOR::get_assigned_location(
    SDC_ENTRY*& entry, const std::string& rule, const std::string& location,
    PIN_PARSER& pin) {
  std::string assigned_location = location;
  log_assert(rule == "" || rule == "half-first");
  log_assert(pin.failure_reason.empty());
  if (rule == "half-first") {
    if (pin.index < 20) {
      assigned_location =
          stringf("H%s_%s_0_0P", pin.type.c_str(), pin.bank.c_str());
    } else {
      assigned_location =
          stringf("H%s_%s_20_10P", pin.type.c_str(), pin.bank.c_str());
    }
    entry->comments.push_back(stringf("# Remap location from %s to %s",
                                      location.c_str(),
                                      assigned_location.c_str()));
  }
  return assigned_location;
}

/*
  Get the wrapped instance
*/
size_t PRIMITIVES_EXTRACTOR::get_wrapped_instance(
    const nlohmann::json& wrapped_instances, const std::string& name) {
  log_assert(name.size());
  size_t index = 0;
  bool found = false;
  for (auto& inst : wrapped_instances) {
    std::string inst_name = (std::string)(inst["name"]);
    if (inst_name == name || ((inst_name.size() > (name.size() + 1)) &&
                              (inst_name.rfind("." + name) ==
                               (inst_name.size() - name.size() - 1)))) {
      found = true;
      break;
    }
    index++;
  }
  log_assert(found);
  return index;
}

/*
  Get the wrapped instance's input net
*/
std::string PRIMITIVES_EXTRACTOR::get_input_wrapped_net(
    const nlohmann::json& wrapped_instances, size_t index,
    const FABRIC_CLOCK* clk) {
  log_assert(wrapped_instances.is_array());
  log_assert(index < wrapped_instances.size());
  const nlohmann::json& instance = wrapped_instances[index];
  log_assert(instance["connectivity"].contains(clk->iport));
  std::string wrapped_net = instance["connectivity"][clk->iport];
  log_assert(wrapped_net.size());
  // Any subsequence wire
  for (auto& instance : wrapped_instances) {
    if (instance["module"] == "WIRE" /*&&
        ((clk->is_fabric_clkbuf) ||
         (instance.contains("linked_object") &&
          sort_name(instance["linked_object"]) == clk->linked_object))*/) {
      if (instance["connectivity"]["O"] == wrapped_net) {
        wrapped_net = instance["connectivity"]["I"];
      }
    }
  }
  bool found = false;
  for (auto& fabric : wrapped_instances) {
    // All instance are either primitive or WIRE or fabric
    // primitive and WIRE module name is fix
    // for fabric, the module name format is "fabric_<project>"
    if (((std::string)(fabric["module"])).find("fabric_") == 0) {
      if (fabric["connectivity"].contains(wrapped_net)) {
        // good
        found = true;
      }
      break;
    }
  }
  if (!found) {
    wrapped_net = "";
  }
  return wrapped_net;
}

/*
  Get the wrapped instance's output net
*/
std::string PRIMITIVES_EXTRACTOR::get_output_wrapped_net(
    const nlohmann::json& wrapped_instances, size_t index,
    const FABRIC_CLOCK* clk) {
  log_assert(wrapped_instances.is_array());
  log_assert(index < wrapped_instances.size());
  const nlohmann::json& instance = wrapped_instances[index];
  log_assert(instance["connectivity"].contains(clk->oport));
  std::string wrapped_net = instance["connectivity"][clk->oport];
  log_assert(wrapped_net.size());
  // Any subsequence wire
  for (auto& instance : wrapped_instances) {
    if (instance["module"] == "WIRE" /*&&
        ((clk->is_fabric_clkbuf) ||
         (instance.contains("linked_object") &&
          sort_name(instance["linked_object"]) == clk->linked_object))*/) {
      if (instance["connectivity"]["I"] == wrapped_net) {
        wrapped_net = instance["connectivity"]["O"];
      }
    }
  }
  bool found = false;
  for (auto& fabric : wrapped_instances) {
    // All instance are either primitive or WIRE or fabric
    // primitive and WIRE module name is fix
    // for fabric, the module name format is "fabric_<project>"
    if (((std::string)(fabric["module"])).find("fabric_") == 0) {
      if (fabric["connectivity"].contains(wrapped_net)) {
        // good
        found = true;
      }
      break;
    }
  }
  if (!found) {
    wrapped_net = "";
  }
  return wrapped_net;
}

/*
  Get the fabric data of the last data primitive
*/
std::string PRIMITIVES_EXTRACTOR::get_fabric_data(
    const nlohmann::json& wrapped_instances, const std::string& object,
    std::vector<std::string>& data_nets, std::vector<bool>& found_nets,
    bool& input) {
  log_assert(data_nets.size() == 0);
  log_assert(found_nets.size() == 0);
  INSTANCE* instance = nullptr;
  size_t instance_index = 0;
  std::string reason = "";
  POST_MSG(4, "Data signal from object %s", object.c_str());
  size_t i = 0;
  for (auto& inst : m_instances) {
    if (std::find(inst->linked_objects.begin(), inst->linked_objects.end(),
                  object) != inst->linked_objects.end()) {
      if (inst->module != "WIRE" && inst->module != "CLK_BUF") {
        instance = inst;
        instance_index = i;
      }
    }
    i++;
  }
  if (instance != nullptr) {
    const PRIMITIVE_DB* db = instance->primitive->db;
    log_assert(db != nullptr);
    input = db->is_in_dir();
    if (db->data_signal.size()) {
      std::string linked_object = instance->linked_object();
      std::string data_port = get_original_name(db->data_signal);
      std::pair<std::string, std::string> wrapped_reason =
          get_wrapped_instance_net_by_port(wrapped_instances, instance->module,
                                           linked_object, data_port, data_nets);
      log_assert(wrapped_reason.first.empty() == wrapped_reason.second.empty());
      reason = wrapped_reason.second;
      if (reason.empty()) {
        get_wrapped_instance_potential_next_wire(
            wrapped_instances, db->is_in_dir() ? "I" : "O",
            db->is_in_dir() ? "O" : "I", data_nets);
        found_nets = check_fabric_port(wrapped_instances, data_nets);
        if (data_nets.size() == found_nets.size()) {
          bool found = false;
          for (size_t i = 0; i < data_nets.size(); i++) {
            POST_MSG(5, "Module=%s Linked-object=%s Port=%s Net=%s - %s",
                     instance->module.c_str(), linked_object.c_str(),
                     data_port.c_str(), data_nets[i].c_str(),
                     found_nets[i] ? "Found" : "Not found");
            if (found_nets[i]) {
              found = true;
            }
          }
          if (!found) {
            if ((instance->module == "I_BUF" ||
                 instance->module == "I_BUF_DS") &&
                (((instance_index + 1) < m_instances.size() &&
                  m_instances[instance_index + 1]->module == "CLK_BUF" &&
                  instance->linked_object() ==
                      m_instances[instance_index + 1]->linked_object()) ||
                 ((instance_index + 2) < m_instances.size() &&
                  m_instances[instance_index + 1]->module == "WIRE" &&
                  instance->linked_object() ==
                      m_instances[instance_index + 1]->linked_object() &&
                  m_instances[instance_index + 2]->module == "CLK_BUF" &&
                  instance->linked_object() ==
                      m_instances[instance_index + 2]->linked_object()))) {
              reason = stringf(
                  "Clock data from object %s port %s is not routed to fabric",
                  object.c_str(), data_port.c_str());
            } else {
              reason = stringf(
                  "Fail to map all data signal(s) from object %s port %s to "
                  "fabric",
                  object.c_str(), data_port.c_str());
            }
          }
        }
      }
    } else {
      reason =
          stringf("Object %s is primitive %s but data signal is not defined",
                  object.c_str(), db->name.c_str());
    }
  } else {
    reason = stringf("Unable to find instance for object %s", object.c_str());
  }
  if (reason.size()) {
    POST_MSG(5, "Fail reason: %s", reason.c_str());
  }
  return reason;
}

/*
  Get the net that connected to the port of the wrapped instance
*/
std::pair<std::string, std::string>
PRIMITIVES_EXTRACTOR::get_wrapped_instance_net_by_port(
    const nlohmann::json& wrapped_instances, const std::string& module,
    const std::string& linked_object, const std::string& port,
    std::vector<std::string>& nets) {
  log_assert(nets.size() == 0);
  bool found_instance = false;
  bool found_port = false;
  std::pair<std::string, std::string> reason("", "");
  for (auto& instance : wrapped_instances) {
    if (instance["module"] == module && instance.contains("linked_object") &&
        sort_name(instance["linked_object"]) == linked_object) {
      found_instance = true;
      for (auto iter : instance["connectivity"].items()) {
        std::string key = iter.key();
        nlohmann::json value = iter.value();
        if (key == port) {
          found_port = true;
          if (value.is_array()) {
            for (nlohmann::json v : value) {
              log_assert(v.is_string());
              nets.push_back((std::string)(v));
            }
          } else {
            log_assert(value.is_string());
            nets.push_back((std::string)(value));
          }
        }
      }
      break;
    }
  }
  if (found_instance) {
    if (found_port) {
      if (nets.size() == 0) {
        reason = std::make_pair("Fail",
                                stringf("Unable to find linked-object %s "
                                        "wrapped-instance port %s data net",
                                        linked_object.c_str(), port.c_str()));
      }
    } else {
      reason = std::make_pair("Skip",
                              stringf("User design does not utilize "
                                      "linked-object %s wrapped-instance port "
                                      "%s",
                                      linked_object.c_str(), port.c_str()));
    }
  } else {
    reason = std::make_pair(
        "Fail", stringf("Unable to find linked-object %s wrapped-instance",
                        linked_object.c_str()));
  }
  return reason;
}

/*
  Get the subsequent potential wire
*/
void PRIMITIVES_EXTRACTOR::get_wrapped_instance_potential_next_wire(
    const nlohmann::json& wrapped_instances, const std::string& src,
    const std::string& dest, std::vector<std::string>& nets) {
  log_assert(src == "I" || src == "O");
  log_assert(dest == "I" || dest == "O");
  log_assert(src != dest);
  for (size_t i = 0; i < nets.size(); i++) {
    for (auto& instance : wrapped_instances) {
      if (instance["module"] == "WIRE") {
        if (instance["connectivity"][src] == nets[i]) {
          nets[i] = instance["connectivity"][dest];
          break;
        }
      }
    }
  }
}

/*
  Make sure the net exists as port of the fabric
*/
std::vector<bool> PRIMITIVES_EXTRACTOR::check_fabric_port(
    const nlohmann::json& wrapped_instances,
    const std::vector<std::string> nets) {
  std::vector<bool> founds;
  for (auto& fabric : wrapped_instances) {
    // All instance are either primitive or WIRE or fabric
    // primitive and WIRE module name is fix
    // for fabric, the module name format is "fabric_<project>"
    if (((std::string)(fabric["module"])).find("fabric_") == 0) {
      for (auto& net : nets) {
        founds.push_back(fabric.contains("connectivity") &&
                         fabric["connectivity"].contains(net));
      }
      break;
    }
  }
  return founds;
}

/*
  Write string to the text output
*/
void PRIMITIVES_EXTRACTOR::file_write_string(std::ofstream& file,
                                             const std::string& string,
                                             int size) {
  if (size == -1) {
    file << string.c_str();
  } else {
    file << stringf("%-*s", size, string.c_str()).c_str();
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
      file_write_string(sdc, assignment.str1, (int)(col1 + 1));
      file_write_string(sdc, assignment.str2, (int)(col2 + 1));
      if (assignment.str4.size()) {
        file_write_string(sdc, assignment.str3, (int)(col3 + 1));
        if (assignment.str5.size()) {
          log_assert(assignment.str6.size());
          log_assert(assignment.str7.size());
          file_write_string(sdc, assignment.str4, (int)(col4 + 1));
          file_write_string(sdc, assignment.str5, (int)(col5 + 1));
          file_write_string(sdc, assignment.str6, (int)(col6 + 1));
          file_write_string(sdc, assignment.str7);
        } else {
          file_write_string(sdc, assignment.str4);
          log_assert(assignment.str6.empty());
          log_assert(assignment.str7.empty());
        }
      } else {
        file_write_string(sdc, assignment.str3);
        log_assert(assignment.str5.empty());
        log_assert(assignment.str6.empty());
        log_assert(assignment.str7.empty());
      }
      sdc << "\n";
    }
    sdc << "\n";
  }
  while (sdc_entries.size()) {
    delete sdc_entries.back();
    sdc_entries.pop_back();
  }
}