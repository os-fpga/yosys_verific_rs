#ifndef PRIMITIVES_EXTRACTOR_H
#define PRIMITIVES_EXTRACTOR_H

#include <map>
#include <string>
#include <vector>

#include "kernel/rtlil.h"

enum IO_DIR { IN, OUT, INOUT, UNKNOWN };

struct MSG;
struct PRIMITIVE_DB;
struct PRIMITIVE;
struct PORT_PRIMITIVE;
struct INSTANCE;

class PRIMITIVES_EXTRACTOR {
 public:
  PRIMITIVES_EXTRACTOR(const std::string& technology);
  ~PRIMITIVES_EXTRACTOR();
  bool extract(Yosys::RTLIL::Design* design);
  void assign_location(
      const std::string& port, const std::string& location,
      std::unordered_map<std::string, std::string>& properties);
  void write_json(const std::string& file);

 private:
  void post_msg(uint32_t offset, const std::string& msg);
  void remove_msg();
  bool get_ports(Yosys::RTLIL::Module* module);
  const PRIMITIVE_DB* is_supported_primitive(const std::string& name);
  void get_primitive_parameters(Yosys::RTLIL::Cell* cell, PRIMITIVE* primitive);
  bool trace_and_create_port(Yosys::RTLIL::Module* module, IO_DIR dir,
                             const std::string& port_name,
                             const std::string& port_fullname,
                             const std::string& port_realname, int oindex,
                             int index, uint32_t width);
  std::map<std::string, std::string> is_connected_cell(
      Yosys::RTLIL::Cell* cell, const PRIMITIVE_DB* db,
      const std::string& connection);
  void trace_clk_buf(Yosys::RTLIL::Module* module);
  bool trace_next_primitive(Yosys::RTLIL::Module* module,
                            const std::string& module_name, PRIMITIVE*& parent,
                            const std::string& connection);
  void gen_instances();
  void gen_instance(const std::string& linked_object,
                    const PRIMITIVE* primitive);
  void gen_wire(const PORT_PRIMITIVE* port, const std::string& child);
  void write_instance(const INSTANCE* instance, std::ofstream& json);
  void write_instance_map(std::map<std::string, std::string> map,
                          std::ofstream& json);
  void write_json_object(uint32_t space, const std::string& key,
                         const std::string& value, std::ofstream& json);
  void write_json_data(const std::string& str, std::ofstream& json);

 private:
  std::vector<MSG*> m_msgs;
  std::vector<PORT_PRIMITIVE*> m_ports;
  std::vector<PRIMITIVE*> m_child_primitives;
  std::vector<INSTANCE*> m_instances;
  bool m_status = true;
  const std::string m_technology = "";
};

#endif