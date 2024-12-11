#ifndef PRIMITIVES_EXTRACTOR_H
#define PRIMITIVES_EXTRACTOR_H

#include <json.hpp>
#include <map>
#include <string>
#include <vector>

#include "kernel/rtlil.h"

struct MSG;
struct DRIVE_SINK;
struct NET;
struct PORT;
struct CLOCK_DRIVEN_TO;
struct PRIMITIVE;
struct YAML;
struct OBJECT;

/*
  Both structures are for SDC
*/
struct SDC_ASSIGNMENT {
  SDC_ASSIGNMENT(const std::string& s1, const std::string& s2,
                 const std::string& s3, const std::string& s4 = "",
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

class PRIMITIVES_EXTRACTOR {
 public:
  PRIMITIVES_EXTRACTOR(const std::string& technology);
  ~PRIMITIVES_EXTRACTOR();
  static void get_signals(const Yosys::RTLIL::SigSpec& sig,
                          std::vector<std::string>& signals);
  static bool is_real_net(const std::string& net);
  bool extract(Yosys::RTLIL::Module* module, const std::string& user_sdc,
               const std::string& config_json, const std::string& sdc,
               const std::string& clk_pin_xml,
               std::map<std::string, bool>& fabric_ports_dir,
               std::vector<std::string>& errors);

 private:
  void post_msg(uint32_t offset, bool is_error, const std::string& msg);
  void remove_msg();

  /*
    Steps to get primitives
  */
  bool get_primitives(Yosys::RTLIL::Module* module,
                      const std::map<std::string, bool>& fabric_ports_dir);
  bool get_primitives(Yosys::RTLIL::Module* module,
                      const std::map<std::string, bool>& fabric_ports_dir,
                      std::map<std::string, DRIVE_SINK*>& out_nets,
                      std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets);
  bool get_yaml_port_iopad(const YAML* yaml, const std::string& port,
                           const std::map<std::string, bool>& fabric_ports_dir,
                           bool& is_input);
  bool assign_drive_sink(
      const std::map<std::string, bool>& fabric_ports_dir,
      const std::map<std::string, DRIVE_SINK*>& out_nets,
      const std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets);
  bool assign_drive_sink(NET*& net, bool dc_is_iopad, const DRIVE_SINK* ds,
                         bool ds_is_iopad);
  bool assign_iopad(
      Yosys::RTLIL::Module* module,
      const std::map<std::string, DRIVE_SINK*>& out_nets,
      const std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets);
  NET* get_net_from_drive_sink(const DRIVE_SINK* drive_sink,
                               const std::string& net_name);
  bool get_iopad_nets(
      const std::map<std::string, DRIVE_SINK*>& out_nets,
      const std::map<std::string, std::vector<DRIVE_SINK*>>& in_nets,
      const std::string& name, std::vector<NET*>& nets);
  bool validate_primitive();

  /*
    Steps to validate netlist
  */
  bool validate_netlist();
  bool validate_data_port_iopad();
  bool validate_fabric_control_port();
  bool validate_non_fabric_port();
  bool validate_path(const std::string& path_name, const void* paths_ptr,
                     bool must_connect);

  /*
    Steps to build chain
  */
  bool build_chain(std::vector<PRIMITIVE*>& parents,
                   std::vector<const PRIMITIVE*>& core_clocks,
                   const std::string& sdc);
  const void* search_valid_paths(const void* customized_paths,
                                 const std::string& primitive);
  bool trace_child_data_path(PRIMITIVE*& grandparent, PRIMITIVE*& parent,
                             uint32_t space);
  bool validate_data_path_clock(uint32_t space, const PORT* port,
                                size_t clock_count, size_t standalone_count);
  bool validate_data_path_data(uint32_t space, const PORT* port,
                               size_t data_count, size_t fabric_count,
                               size_t standalone_count);
  bool validate_data_path_fabric(uint32_t space, const PORT* port,
                                 size_t data_count, size_t fabric_count,
                                 size_t standalone_count);
  bool validate_data_path_standalone(uint32_t space, const PORT* port,
                                     size_t data_count, size_t fabric_count,
                                     size_t standalone_count);
  bool trace_child_clock_path(PRIMITIVE*& grandparent, PRIMITIVE*& parent,
                              uint32_t space);
  bool trace_missing_primitive(std::vector<PRIMITIVE*>& parents);
  bool trace_if_primitive_exist(std::vector<PRIMITIVE*>& cells,
                                PRIMITIVE* cell);
  bool assign_location(std::vector<PRIMITIVE*>& datas, const std::string& sdc);
  bool determine_mode(std::vector<PRIMITIVE*>& datas);
  bool determine_standalone_fabric_location(
      std::vector<PRIMITIVE*>& standalones);
  bool determine_core_clock(std::vector<PRIMITIVE*>& clocks,
                            std::vector<const PRIMITIVE*>& core_clocks);
  bool determine_fast_clock(std::vector<PRIMITIVE*>& clocks);

  /*
    Print summary of the chain
  */
  void summarize(const std::vector<PRIMITIVE*>& parents);
  void summarize(bool input, const std::string& object,
                 const std::string& trace, int max_in_object, int max_in_trace,
                 int max_out_trace, int max_out_object, bool print_dir = true);

  /*
    Steps to write SDC and XML information
  */
  bool write_sdc(std::vector<PRIMITIVE*>& parents,
                 std::vector<const PRIMITIVE*>& clocks,
                 const std::string& sdc_file, const std::string& clk_pin_xml);
  std::string get_sdc_mode(const PRIMITIVE* primitive);
  void write_sdc_core_clock(std::vector<const PRIMITIVE*>& clocks,
                            std::ofstream& sdc, std::ofstream& xml);
  void write_sdc_data(std::vector<PRIMITIVE*>& parents, std::ofstream& sdc);
  void write_sdc_fabric_control_map(std::vector<PRIMITIVE*>& parents,
                                    std::ofstream& sdc);
  void write_sdc_fabric_control_map(
      const PRIMITIVE* grandparent, const PRIMITIVE* primitive,
      std::vector<const PRIMITIVE*>& written_cells,
      std::vector<SDC_ENTRY*>& sdc_entries,
      std::map<std::string, const PORT*>& used_ports);

  /*
    Steps to write JSON
  */
  void write_json(std::vector<PRIMITIVE*>& parents,
                  const std::string& config_json);
  std::string gen_space_str(uint32_t space);
  void write_json_chars(std::ofstream& json, const std::string& str);
  void write_json_string(std::ofstream& json, uint32_t space,
                         const std::string& str, bool comma, bool new_line);
  void write_json_object_bool(std::ofstream& json, uint32_t space,
                              const std::string& object, bool status,
                              bool comma, bool new_line);
  void write_json_object_str(std::ofstream& json, uint32_t space,
                             const std::string& object, const std::string& str,
                             bool comma, bool new_line);
  void write_json_object_vector(std::ofstream& json, uint32_t space,
                                const std::string& object,
                                const std::vector<std::string>& vec, bool comma,
                                bool new_line);
  void write_json_object_dict(std::ofstream& json, uint32_t space,
                              const std::string& object,
                              const std::map<std::string, std::string>& dict1,
                              const std::map<std::string, std::string>& dict2,
                              bool comma, bool new_line);
  void write_json_primitive(std::ofstream& json, const PRIMITIVE* primitive,
                            std::vector<const PRIMITIVE*>& written_primitives);
  void write_json_primitive_objects(
      std::ofstream& json, const std::string& location_object,
      const std::map<std::string, OBJECT*>& objects,
      const std::map<std::string, std::string>& parameters);
  void write_json_primitive_port(std::ofstream& json, const PORT* port);
  void write_json_primitive_clock_drive_to(
      std::ofstream& json, const std::vector<const CLOCK_DRIVEN_TO*>& clocks);
  void file_write_string(std::string& line, const std::string& string,
                         int size = -1);
  void write_sdc_entries(std::ofstream& sdc,
                         std::vector<SDC_ENTRY*>& sdc_entries);

  /*
    Get all IDs
  */
  std::string get_drive_sinks_id(const std::vector<DRIVE_SINK*> dss);
  std::string get_nets_id(const std::vector<NET*> nets);
  std::string get_port_id(const DRIVE_SINK* ds);

 private:
  // Constant member
  const std::string m_technology = "";

  //
  bool m_status = true;
  bool m_basic_status = true;
  bool m_netlist_status = true;
  PRIMITIVE* m_fabric = nullptr;
  std::map<std::string, PRIMITIVE*> m_primitives;
  std::vector<MSG*> m_msgs;
  std::vector<std::string> m_errors;
};

#endif