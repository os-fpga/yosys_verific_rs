#ifndef PRIMITIVES_EXTRACTOR_H
#define PRIMITIVES_EXTRACTOR_H

#include <json.hpp>
#include <map>
#include <string>
#include <vector>

#include "kernel/rtlil.h"

enum IO_DIR { IN, OUT, INOUT, UNKNOWN };

enum PRIMITIVE_REQ {
  DONT_CARE,
  IS_PORT,
  NOT_PORT,
  IS_STANDALONE,
  IS_FABRIC_CLKBUF
};

struct MSG;
/*
  Structure that store simple information about port
*/
struct PORT_INFO {
  PORT_INFO(IO_DIR d, const std::string& pn, const std::string& pf,
            const std::string& pr, int oidx, uint32_t idx, uint32_t w)
      : dir(d),
        name(pn),
        fullname(pf),
        realname(pr),
        offset_index(oidx),
        index(idx),
        width(w) {}
  const IO_DIR dir = IO_DIR::UNKNOWN;
  const std::string name = "";
  const std::string fullname = "";
  const std::string realname = "";
  const int offset_index = 0;
  const int index = 0;
  const uint32_t width = 0;
};
struct PRIMITIVE_DB;
struct PRIMITIVE;
struct PORT_PRIMITIVE;
struct INSTANCE;
struct PIN_PORT;
struct FABRIC_CLOCK;

/*
  Both structures are for SDC
*/
struct SDC_ASSIGNMENT {
  SDC_ASSIGNMENT(const std::string& s1, const std::string& s2,
                 const std::string& s3, const std::string& s4,
                 const std::string& s5 = "", const std::string& s6 = "",
                 const std::string& s7 = "")
      : str1(s1), str2(s2), str3(s3), str4(s4), str5(s5), str6(s6), str7(s7) {}
  const std::string str1 = "";
  const std::string str2 = "";
  const std::string str3 = "";
  const std::string str4 = "";
  const std::string str5 = "";
  const std::string str6 = "";
  const std::string str7 = "";
};

struct SDC_ENTRY {
  std::vector<std::string> comments;
  std::vector<SDC_ASSIGNMENT> assignments;
};

/*
  Structure to store pin information
*/
struct PIN_PARSER {
  std::string type = "";
  std::string bank = "";
  bool is_clock = false;
  int index = 0;
  std::string failure_reason = "";
};

class PRIMITIVES_EXTRACTOR {
 public:
  PRIMITIVES_EXTRACTOR(const std::string& technology);
  ~PRIMITIVES_EXTRACTOR();
  bool extract(Yosys::RTLIL::Design* design);
  void assign_location(const std::string& port, const std::string& location,
                       std::unordered_map<std::string, std::string>& properties,
                       const std::string& internal_pin);
  std::vector<std::string> get_primitive_locations_by_name(
      const std::string& name, bool unique_location = false);
  void write_json(const std::string& file, bool simple = false);
  void write_sdc(const std::string& file,
                 const nlohmann::json& wrapped_instances);
  static void get_signals(const Yosys::RTLIL::SigSpec& sig,
                          std::vector<std::string>& signals);
  static bool is_real_net(const std::string& net);

 private:
  void post_msg(uint32_t offset, const std::string& msg);
  void post_sdc_comment(SDC_ENTRY*& entry, uint32_t offset,
                        const std::string& type, const std::string& comment);
  void remove_msg();
  bool get_ports(Yosys::RTLIL::Module* module);
  const PRIMITIVE_DB* is_supported_primitive(const std::string& name,
                                             PRIMITIVE_REQ req);
  void get_primitive_parameters(Yosys::RTLIL::Cell* cell, PRIMITIVE* primitive);
  void trace_and_create_port(Yosys::RTLIL::Module* module,
                             std::vector<PORT_INFO>& port_infos);
  bool get_connected_port(Yosys::RTLIL::Module* module,
                          const std::string& cell_port_name,
                          const std::string& connection, IO_DIR dir,
                          std::vector<PORT_INFO>& port_infos,
                          std::vector<size_t>& port_trackers,
                          std::vector<PORT_INFO>& connected_ports,
                          int loop = 0);
  bool get_port_cell_connections(
      Yosys::RTLIL::Cell* cell, const PRIMITIVE_DB* db,
      std::map<std::string, std::string>& primary_connections,
      std::map<std::string, std::string>& secondary_connections);
  std::map<std::string, std::string> is_connected_cell(
      Yosys::RTLIL::Cell* cell, const PRIMITIVE_DB* db,
      const std::string& connection);
  void trace_next_primitive(Yosys::RTLIL::Module* module,
                            const std::string& src_primitive_name,
                            const std::string& dest_primitive_name);
  bool trace_next_primitive(Yosys::RTLIL::Module* module, PRIMITIVE*& parent,
                            Yosys::RTLIL::Cell* cell,
                            const std::string& connection);
  void trace_fabric_clkbuf(Yosys::RTLIL::Module* module);
  void trace_gearbox_clock();
  static void get_chunks(const Yosys::RTLIL::SigChunk& chunk,
                         std::vector<std::string>& signals);
  void gen_instances();
  void gen_instances(const std::string& linked_object,
                     std::vector<std::string> linked_objects,
                     const PRIMITIVE* primitive,
                     const std::string& pre_primitive);
  void gen_instance(std::vector<std::string> linked_objects,
                    const PRIMITIVE* primitive,
                    const std::string& pre_primitive);
  void gen_wire(const std::string& linked_object,
                std::vector<std::string> linked_objects, const PRIMITIVE* port,
                const std::string& child);
  void determine_fabric_clock(Yosys::RTLIL::Module* module);
  bool need_to_route_to_fabric(Yosys::RTLIL::Module* module,
                               const std::string& module_type,
                               const std::string& module_name,
                               const std::string& port_name,
                               const std::string& net_name);
  PIN_PORT* get_pin_info(const std::string& name);
  void summarize();
  void summarize(const PRIMITIVE* primitive,
                 const std::vector<std::string> traces, bool is_in_dir);
  void summarize(const PRIMITIVE* primitive, const std::string& object_name,
                 const std::vector<std::string> objects,
                 const std::vector<std::string> traces,
                 const std::vector<std::string> full_traces, bool is_in_dir);
  void update_pin_info(const std::string& pin_name, const PRIMITIVE* primitive);
  void update_pin_traces(std::vector<std::string>& pin_traces,
                         const std::vector<std::string> traces, bool is_in_dir);
  void finalize(Yosys::RTLIL::Module* module);
  void write_instance(const INSTANCE* instance, std::ofstream& json,
                      bool simple);
  void write_instance_map(std::map<std::string, std::string> map,
                          std::ofstream& json, uint32_t space = 4);
  void write_instance_array(std::vector<std::string> array, std::ofstream& json,
                            uint32_t space = 4);
  void write_json_object(uint32_t space, const std::string& key,
                         const std::string& value, std::ofstream& json);
  void write_json_data(const std::string& str, std::ofstream& json);
  void write_sdc_internal_control_signal(
      std::vector<SDC_ENTRY*>& sdc_entries,
      const nlohmann::json& wrapped_instances, const std::string& module,
      const std::string& linked_object, const std::string& location,
      const std::string& port, const std::string& internal_signal);
  bool validate_location(const std::string& location, PIN_PARSER& pin);
  std::string get_assigned_location(SDC_ENTRY*& entry, const std::string& rule,
                                    const std::string& location,
                                    PIN_PARSER& pin);
  size_t get_wrapped_instance(const nlohmann::json& wrapped_instances,
                              const std::string& name);
  std::string get_input_wrapped_net(const nlohmann::json& wrapped_instances,
                                    size_t index, const FABRIC_CLOCK* clk);
  std::string get_output_wrapped_net(const nlohmann::json& wrapped_instances,
                                     size_t index, const FABRIC_CLOCK* clk);
  std::string get_fabric_data(const nlohmann::json& wrapped_instances,
                              const std::string& object,
                              std::vector<std::string>& data_nets,
                              std::vector<bool>& found_nets, bool& input);
  std::pair<std::string, std::string> get_wrapped_instance_net_by_port(
      const nlohmann::json& wrapped_instances, const std::string& module,
      const std::string& linked_object, const std::string& port,
      std::vector<std::string>& nets);
  void get_wrapped_instance_potential_next_wire(
      const nlohmann::json& wrapped_instances, const std::string& src,
      const std::string& dest, std::vector<std::string>& nets);
  std::vector<bool> check_fabric_port(const nlohmann::json& wrapped_instances,
                                      const std::vector<std::string> nets);
  void file_write_string(std::ofstream& file, const std::string& string,
                         int size = -1);
  void write_sdc_entries(std::ofstream& sdc,
                         std::vector<SDC_ENTRY*>& sdc_entries);

 private:
  const std::string m_technology = "";
  bool m_status = true;
  int m_max_in_object_name = 0;
  int m_max_out_object_name = 0;
  int m_max_object_name = 0;
  int m_max_trace = 0;
  std::vector<std::string> m_auto_assigned_location;
  std::map<std::string, std::string> m_location_mode;
  std::vector<std::string> m_internal_signal_net;
  std::vector<MSG*> m_msgs;
  std::vector<PORT_PRIMITIVE*> m_ports;
  std::vector<PRIMITIVE*> m_child_primitives;
  std::vector<INSTANCE*> m_instances;
  std::vector<FABRIC_CLOCK*> m_fabric_clocks;
  std::vector<PIN_PORT*> m_pin_infos;
};

#endif