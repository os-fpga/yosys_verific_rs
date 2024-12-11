##################################
#
# Author: Chai, Chung Shien
#
#   This script auto-gen 
#     a. yaml_info_auto.h
#     b. primitive_path_auto.h
#
##################################
import sys
import os
import yaml
import re
import json

SUPPORTED_VERSION = {
  "genesis3" : [
    # Do not need input but can output clock
    ["BOOT_CLOCK", "STANDALONE_CLOCK", "IN"],
    ["CLK_BUF", "CLOCK", "IN"],
    # Do not need input/output but connect to FABIRC
    ["DLY_SEL_DECODER", "STANDALONE_FABRIC", "INOUT"],
    ["FCLK_BUF", "FABRIC_CLOCK", "INOUT"],
    ["I_BUF", "DATA", "IN"],
    ["I_BUF_DS", "DATA", "IN"],
    ["I_DDR", "DATA", "IN"],
    ["I_DELAY", "DATA", "IN"],
    ["I_SERDES", "DATA", "IN"],
    ["O_BUFT", "DATA", "OUT"],
    ["O_BUFT_DS", "DATA", "OUT"],
    ["O_DDR", "DATA", "OUT"],
    ["O_DELAY", "DATA", "OUT"],
    ["O_SERDES", "DATA", "OUT"],
    # Do not need connect to FABIRC but can connect to DATA (physical port)
    ["O_SERDES_CLK", "STANDALONE_DATA", "OUT"],
    ["PLL", "CLOCK", "IN"],
    ["SOC_FPGA_INTF_AHB_M", "SOC", "UNKNOWN"],
    ["SOC_FPGA_INTF_AHB_S", "SOC", "UNKNOWN"],
    ["SOC_FPGA_INTF_DMA", "SOC", "UNKNOWN"],
    ["SOC_FPGA_INTF_IRQ", "SOC", "UNKNOWN"],
    ["SOC_FPGA_INTF_JTAG", "SOC", "UNKNOWN"],
    ["SOC_FPGA_TEMPERATURE", "SOC", "UNKNOWN"]
  ]
}

def print_header(header) :

  header.write("/*\n")
  header.write("  This file is auto-generated\n")
  header.write("  Author: Chai, Chung Shien\n")
  header.write("*/\n\n")
  header.write("#ifndef YAML_INFO_H\n")
  header.write("#define YAML_INFO_H\n\n")
  header.write("#include <map>\n")
  header.write("#include <string>\n")
  header.write("#include <vector>\n\n")
  header.write("struct PORT_BASIC {\n")
  header.write("  PORT_BASIC(bool i, int s, std::vector<std::string> a) :\n")
  header.write("    is_input(i), size(s), attributes(a) {\n")
  header.write("    }\n")
  header.write("  bool is_attribute(const std::string& attr) const {\n")
  header.write("    return (std::find(attributes.begin(), attributes.end(), attr) !=\n")
  header.write("            attributes.end());\n")
  header.write("  }\n")
  header.write("  const bool is_input = false;\n")
  header.write("  const int size = 0;\n")
  header.write("  const std::vector<std::string> attributes;\n")
  header.write("};\n\n")
  header.write("struct YAML {\n")
  header.write("  YAML(const std::string& n,\n")
  header.write("       const std::string& c,\n")
  header.write("       const std::string& g,\n")
  header.write("       const std::string& d,\n")
  header.write("       std::map<std::string, PORT_BASIC> p,\n")
  header.write("       std::map<std::string, std::string> fcm,\n")
  header.write("       std::vector<std::string> nfp) :\n")
  header.write("    name(n),\n")
  header.write("    category(c),\n")
  header.write("    group(g),\n")
  header.write("    direction(d),\n")
  header.write("    ports(p),\n")
  header.write("    fabric_control_map(fcm),\n")
  header.write("    non_fabric_ports(nfp) {\n")
  header.write("    }\n")
  header.write("  const std::string name = \"\";\n")
  header.write("  const std::string category = \"\";\n")
  header.write("  const std::string group = \"\";\n")
  header.write("  const std::string direction = \"\";\n")
  header.write("  const std::map<std::string, PORT_BASIC> ports;\n")
  header.write("  const std::map<std::string, std::string> fabric_control_map;\n")
  header.write("  const std::vector<std::string> non_fabric_ports;\n")
  header.write("};\n\n")

def extract_primitive_info(path) :

  file = open(path)
  info = yaml.safe_load(file)
  assert "ports" in info
  assert "category" in info
  extracted_info = {  "category" : info["category"], 
                      "ports" : {}, 
                      "fabric_control_map" : {},
                      "non_fabric_map" : [] }
  for port, port_info in info["ports"].items() :
    assert "dir" in port_info
    assert port_info["dir"] in ["input", "output"]
    attributes = []
    if "bb_attributes" in port_info :
      assert len(port_info["bb_attributes"])
      if isinstance(port_info["bb_attributes"], list) :
        for attribute in port_info["bb_attributes"] :
          attribute = attribute.strip()
          assert len(attribute)
          assert attribute not in attributes
      else :
        isinstance(port_info["bb_attributes"], str)
        attr = port_info["bb_attributes"].strip()
        attr = attr.split(",")
        assert len(attr)
        for a in attr :
          a = a.strip()
          assert len(a)
          attributes.append(a)
    index = port.rfind("[")
    size = 1
    if len(port) >= 6 and index != -1 and port.find(":0]") == (len(port) - 3) :
      port_name = port[:index]
      size_str = port[index+1:-3]
      assert len(size_str)
      if size_str.isdigit() :
        size = int(size_str) + 1
      else :
        size = -1
    else :
      port_name = port
    extracted_info["ports"][port_name] = { "dir" : port_info["dir"], 
                                           "attributes" : attributes,
                                           "size" : size }
  if "fabric_control_map" in info :
    for port, mapping in info["fabric_control_map"].items() :
      assert port in extracted_info["ports"]
      extracted_info["fabric_control_map"][port] = mapping
  if "non_fabric_map" in info :
    extracted_info["non_fabric_map"] = info["non_fabric_map"]
  file.close()
  return extracted_info

def print_port_info(header, info) :

  total_port = len(info["ports"])
  port_index = 0
  for port, port_info in info["ports"].items() :
    header.write("            { \"%s\", PORT_BASIC(bool(%d), %d, {\n" % \
                    (port, \
                      port_info["dir"] == "input", \
                      port_info["size"]))
    header.write("              ")
    total_attribute = len(port_info["attributes"])
    for attribute_index, attribute in enumerate(port_info["attributes"]) :
      header.write("  \"%s\"" % (attribute))
      if attribute_index < (total_attribute - 1) :
        header.write(",")
      header.write("\n")
      header.write("              ")
    header.write("})}")
    if port_index < (total_port - 1) :
      header.write(",")
    header.write("\n")
    port_index += 1

def print_fabric_control_map_info(header, info) :

  total_port = len(info["fabric_control_map"])
  port_index = 0
  for port, mapping in info["fabric_control_map"].items() :
    header.write("            { \"%s\", \"%s\" }" % (port, mapping))
    if port_index < (total_port - 1) :
      header.write(",")
    header.write("\n")
    port_index += 1

def print_non_fabric_map_info(header, info) :

  total_port = len(info["non_fabric_map"])
  for port_index, port in enumerate(info["non_fabric_map"]) :
    header.write("            \"%s\"" % (port))
    if port_index < (total_port - 1) :
      header.write(",")
    header.write("\n")
    port_index += 1

def print_path_header(header) :

  header.write("/*\n")
  header.write("  This file is auto-generated\n")
  header.write("  Author: Chai, Chung Shien\n")
  header.write("*/\n\n")
  header.write("#ifndef PRIMITIVE_PATH_H\n")
  header.write("#define PRIMITIVE_PATH_H\n\n")
  header.write("#include <string>\n")
  header.write("#include <vector>\n\n")
  header.write("struct NODE {\n")
  header.write("  NODE(const std::string& pri, const std::string& p, const std::string& g) :\n")
  header.write("    primitive(pri), port(p), group(g) {}\n")
  header.write("  const std::string primitive = \"\";\n")
  header.write("  const std::string port = \"\";\n")
  header.write("  const std::string group = \"\";\n")
  header.write("};\n\n")
  header.write("struct PATH {\n")
  header.write("  PATH(NODE s, std::vector<NODE> d) :\n")
  header.write("    src(s), dests(d) {}\n")
  header.write("  const NODE src;\n")
  header.write("  const std::vector<NODE> dests;\n")
  header.write("};\n\n")
  header.write("struct PATHS {\n")
  header.write("  PATHS(std::vector<PATH> d, std::vector<PATH> rd, std::vector<PATH> c,\n")
  header.write("        std::vector<PATH> rc, std::vector<PATH> cc, std::vector<NODE> fcd,\n")
  header.write("        std::vector<NODE> ccd, std::vector<NODE> fcs, std::vector<NODE> lpfcs,\n")
  header.write("        std::vector<NODE> ccs) :\n")
  header.write("    data_paths(d), reversed_data_paths(rd),\n")
  header.write("    clock_paths(c), reversed_clock_paths(rc),\n")
  header.write("    clock_child_paths(cc), fast_clock_drives(fcd),\n")
  header.write("    core_clock_drives(ccd), fast_clock_sinks(fcs),\n")
  header.write("    low_priority_fast_clock_sinks(lpfcs), core_clock_sinks(ccs) {}\n")
  header.write("  const std::vector<PATH> data_paths;\n")
  header.write("  const std::vector<PATH> reversed_data_paths;\n")
  header.write("  const std::vector<PATH> clock_paths;\n")
  header.write("  const std::vector<PATH> reversed_clock_paths;\n")
  header.write("  const std::vector<PATH> clock_child_paths;\n")
  header.write("  const std::vector<NODE> fast_clock_drives;\n")
  header.write("  const std::vector<NODE> core_clock_drives;\n")
  header.write("  const std::vector<NODE> fast_clock_sinks;\n")
  header.write("  const std::vector<NODE> low_priority_fast_clock_sinks;\n")
  header.write("  const std::vector<NODE> core_clock_sinks;\n")
  header.write("};\n\n")

def get_node(node, primitives) :

  assert len(node) == 2
  if node[0] == "FABRIC" :
    assert node[0] not in primitives
    assert node[1] == ""
    return "\"FABRIC\", \"\", \"FABRIC\""
  else :
    assert node[0] in primitives
    assert node[1] in primitives[node[0]]["ports"]
    group = primitives[node[0]]["group"]
    assert group != "FABRIC"
    return "\"%s\", \"%s\", \"%s\"" % (node[0], node[1], group)

def add_path_src(paths, primitive, port) :

  assert isinstance(paths, list)
  index = -1
  for i, path in enumerate(paths) :
    if path["src"][0] == primitive and path["src"][1] == port :
      index = i
      break
  if index == -1 :
    paths.append({"src" : [primitive, port], "dests" : []})
  return index

def add_path_dest(dests, primitive, port) :

  found = False
  for dest in dests :
    if dest[0] == primitive and dest[1] == port :
      found = True
      break
  if not found :
    dests.append([primitive, port])

def reverse_path(paths) :

  assert isinstance(paths, list)
  rpaths = []
  for path in paths :
    assert isinstance(path, dict)
    assert "src" in path
    assert "dests" in path
    for dest in path["dests"] :
      if dest[0] != "FABRIC" :
        index = add_path_src(rpaths, dest[0], dest[1])
        add_path_dest(rpaths[index]["dests"], path["src"][0], path["src"][1])
  return rpaths

def print_path(header, primitives, paths) :

  assert isinstance(paths, list)
  total_paths = len(paths)
  for i, path in enumerate(paths) :
    assert isinstance(path, dict)
    assert "src" in path
    assert "dests" in path
    header.write("        PATH(NODE(%s), {\n" % (get_node(path["src"], primitives)))
    for j, dest in enumerate(path["dests"]) :
      header.write("          NODE(%s)" % (get_node(dest, primitives)))
      if j < (len(path["dests"]) - 1) :
        header.write(",")
      header.write("\n")
    header.write("        })")
    if i < (total_paths - 1) :
      header.write(",")
    header.write("\n")

def build_clock_paths(paths) :

  assert isinstance(paths, dict)
  assert "fast_clock_drives" in paths
  assert "core_clock_drives" in paths
  assert "fast_clock_sinks" in paths
  assert "low_priority_fast_clock_sinks" in paths
  assert "core_clock_sinks" in paths
  assert "manual" in paths
  clock_paths = paths["manual"]
  assert isinstance(clock_paths, list)
  for fcd in paths["fast_clock_drives"] :
    index = add_path_src(clock_paths, fcd[0], fcd[1])
    for fcs in paths["fast_clock_sinks"] :
      add_path_dest(clock_paths[index]["dests"], fcs[0], fcs[1])
    for lpfcs in paths["low_priority_fast_clock_sinks"] :
      add_path_dest(clock_paths[index]["dests"], lpfcs[0], lpfcs[1])
  for ccd in paths["core_clock_drives"] :
    index = add_path_src(clock_paths, ccd[0], ccd[1])
    for ccs in paths["core_clock_sinks"] :
      add_path_dest(clock_paths[index]["dests"], ccs[0], ccs[1])
  return clock_paths

def print_path_nodes(header, primitives, nodes) :

  assert isinstance(nodes, list)
  for i, node in enumerate(nodes) :
    header.write("        NODE(%s)" % get_node(node, primitives))
    if i < (len(nodes) - 1) :
      header.write(",")
    header.write("\n")

def print_paths(header, primitives, paths) :

  assert isinstance(paths, dict)
  assert "data_paths" in paths 
  assert "clock_child_paths" in paths
  assert "clock_matrix" in paths
  clock_paths = build_clock_paths(paths["clock_matrix"])
  header.write("      { // data_paths\n")
  print_path(header, primitives, paths["data_paths"])
  header.write("      },\n")
  header.write("      { // reversed_data_paths\n")
  print_path(header, primitives, reverse_path(paths["data_paths"]))
  header.write("      },\n")
  header.write("      { // clock_paths\n")
  print_path(header, primitives, clock_paths)
  header.write("      },\n")
  header.write("      { // reversed_clock_paths\n")
  print_path(header, primitives, reverse_path(clock_paths))
  header.write("      },\n")
  header.write("      { // clock_child_paths\n")
  print_path(header, primitives, paths["clock_child_paths"])
  header.write("      },\n")
  header.write("      { // fast_clock_drives\n")
  print_path_nodes(header, primitives, paths["clock_matrix"]["fast_clock_drives"])
  header.write("      },\n")
  header.write("      { // core_clock_drives\n")
  print_path_nodes(header, primitives, paths["clock_matrix"]["core_clock_drives"])
  header.write("      },\n")
  header.write("      { // fast_clock_sinks\n")
  print_path_nodes(header, primitives, paths["clock_matrix"]["fast_clock_sinks"])
  header.write("      },\n")
  header.write("      { // low_priority_fast_clock_sinks\n")
  print_path_nodes(header, primitives, paths["clock_matrix"]["low_priority_fast_clock_sinks"])
  header.write("      },\n")
  header.write("      { // core_clock_sinks\n")
  print_path_nodes(header, primitives, paths["clock_matrix"]["core_clock_sinks"])
  header.write("      }\n")

def main() :

  print("***************************************")
  print("*")
  print("* Auto generate Primitive headers")
  print("*")
  assert len(sys.argv) >= 6
  assert (len(sys.argv) % 2) == 0
  paths = json.loads(open(sys.argv[1]).read())
  header = open(sys.argv[2], "w")
  path_header = open(sys.argv[3], "w")
  print_header(header)
  print_path_header(path_header)
  header.write("const std::map<std::string, std::map<std::string, YAML>> YAML_DB = {\n")
  path_header.write("const std::map<std::string, PATHS> PATH_DB = {\n")
  total_version = (len(sys.argv) - 4)//2
  assert total_version
  for i in range(total_version) :
    version = sys.argv[4+i]
    version_path = sys.argv[4+i+1]
    print("*   Generate for %s from %s" % (version, sys.argv[i+1]))
    assert version in SUPPORTED_VERSION, "Version %s is not supported" % version
    assert os.path.exists(version_path), "Path %s does not exist" % version_path
    assert len(SUPPORTED_VERSION[version])
    assert version in paths
    header.write("  {\n")
    header.write("    \"%s\", {\n" % version)
    total_primtive = len(SUPPORTED_VERSION[version])
    primitives = {}
    for j, infos in enumerate(SUPPORTED_VERSION[version]) :
      assert len(infos) == 3
      primitive = infos[0]
      group = infos[1]
      direction = infos[2]
      assert group.casefold() != "FABRIC".casefold()
      assert direction in ["IN", "OUT", "INOUT", "UNKNOWN"]
      assert primitive not in primitives
      primitives[primitive] = {"group" : group, "ports" : {}}
      path = "%s/%s.yaml" % (version_path, primitive)
      assert os.path.exists(path), "Path %s does not exist" % path
      info = extract_primitive_info(path)
      for port in info["ports"] :
        assert port not in primitives[primitive]
        primitives[primitive]["ports"][port] = info["ports"][port]["dir"]
      header.write("      {\n")
      header.write("        \"%s\", YAML(\n" % primitive)
      header.write("          \"%s\", \"%s\", \"%s\", \"%s\", {\n" % (primitive, info["category"], group, direction))
      print_port_info(header, info)
      header.write("          }, {\n")
      print_fabric_control_map_info(header, info)
      header.write("          }, {\n")
      print_non_fabric_map_info(header, info)
      header.write("          }\n")
      header.write("        )\n")
      header.write("      }")
      if j < (total_primtive - 1) :
        header.write(",")
      header.write("\n")
    header.write("    }\n")
    header.write("  }")
    if i < (total_version - 1) :
      header.write(",")
    header.write("\n")
    # PATH
    path_header.write("  {\n")
    path_header.write("    \"%s\", PATHS(\n" % version)
    print_paths(path_header, primitives, paths[version])
    path_header.write("    )\n")
    path_header.write("  }")
    if i < (total_version - 1) :
      path_header.write(",")
    path_header.write("\n")
  print("*")
  print("***************************************")
  header.write("};\n\n")
  header.write("#endif\n\n")
  header.close()
  path_header.write("};\n\n")
  path_header.write("#endif\n\n")
  path_header.close()

if __name__ == "__main__":
  main()
