#ifndef PRIMITIVES_EXTRACTOR_H
#define PRIMITIVES_EXTRACTOR_H

#include <map>
#include <string>
#include <vector>

#include "kernel/rtlil.h"

enum IO_DIR { IN, OUT, INOUT, UNKNOWN };

enum PORT_REQ { DONT_CARE, IS_PORT, NOT_PORT };

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
  const PRIMITIVE_DB* is_supported_primitive(const std::string& name,
                                             PORT_REQ req);
  void get_primitive_parameters(Yosys::RTLIL::Cell* cell, PRIMITIVE* primitive);
  bool trace_and_create_port(Yosys::RTLIL::Module* module,
                             std::vector<PORT_INFO>& port_infos);
  bool get_connected_port(Yosys::RTLIL::Module* module,
                          const std::string& cell_port_name,
                          const std::string& connection, IO_DIR dir,
                          std::vector<PORT_INFO>& port_infos,
                          std::vector<size_t>& port_trackers,
                          std::vector<PORT_INFO>& connected_ports,
                          int loop=0);
  bool get_port_cell_connections(
      Yosys::RTLIL::Cell* cell, const PRIMITIVE_DB* db,
      std::map<std::string, std::string>& primary_connections,
      std::map<std::string, std::string>& secondary_connections);
  std::map<std::string, std::string> is_connected_cell(
      Yosys::RTLIL::Cell* cell, const PRIMITIVE_DB* db,
      const std::string& connection);
  void trace_clk_buf(Yosys::RTLIL::Module* module);
  bool trace_next_primitive(Yosys::RTLIL::Module* module,
                            const std::string& module_name, PRIMITIVE*& parent,
                            const std::string& connection);
  void get_chunks(const Yosys::RTLIL::SigChunk& chunk,
                  std::vector<std::string>& signals);
  void get_signals(const Yosys::RTLIL::SigSpec& sig,
                   std::vector<std::string>& signals);
  void gen_instances();
  void gen_instance(std::vector<std::string> linked_objects,
                    const PRIMITIVE* primitive);
  void gen_wire(const PORT_PRIMITIVE* port, const std::string& child);
  void write_instance(const INSTANCE* instance, std::ofstream& json);
  void write_instance_map(std::map<std::string, std::string> map,
                          std::ofstream& json, uint32_t space = 4);
  void write_instance_array(std::vector<std::string> array, std::ofstream& json,
                            uint32_t space = 4);
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