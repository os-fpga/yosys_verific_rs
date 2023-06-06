// ---------------------------------------------------------------
// Copyright : RapidSilicon (02.2022)
//
//
// ---------------------------------------------------------------

#ifndef PORT_DUMP_H
#define PORT_DUMP_H

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string.h>
#include <filesystem>
#include <unordered_map>
#include <set>
#include <vector>

#include <nlohmann_json/json.hpp>

#include "veri_file.h"
#include "vhdl_file.h"
#include "VeriModule.h"
#include "VhdlUnits.h"
#include "VhdlIdDef.h"
#include "VeriId.h"
#include "file_sort_veri_tokens.h"
#include "hier_tree.h"
#include "Netlist.h"
#include "VhdlExpression.h"
#include "VhdlName.h"
#include "VhdlValue_Elab.h"
#include "file_sort_vhdl_tokens.h"

using namespace Verific;
using json = nlohmann::json;

class portDump {

    public:
    std::unordered_map<int, std::string> directions = {
        {VERI_INPUT, "Input"},
        {VERI_OUTPUT, "Output"},
        {VERI_INOUT, "Inout"}
    };

    std::unordered_map<int, std::string> vhdlDirections = {
        {VHDL_in, "Input"},
        {VHDL_out, "Output"},
        {VHDL_inout, "Inout"}
    };

    std::unordered_map<int, std::string> types = {
        {VERI_WIRE, "WIRE"},
        {VERI_REG, "REG"},
        {VERI_STRUCT, "STRUCT"},
        {VERI_LOGIC, "LOGIC"},
        {VERI_BIT, "VERI_BIT"},
        {VERI_BYTE, "VERI_BYTE"},
        {VERI_CHAR, "VERI_CHAR"},
        {VERI_ENUM, "VERI_ENUM"},
        {VERI_SHORTINT, "VERI_SHORTINT"},
        {VERI_INT, "VERI_INT"},
        {VERI_INTEGER, "VERI_INTEGER"},
        {VERI_LONGINT, "VERI_LONGINT"},
        {VERI_SHORTREAL, "VERI_SHORTREAL"},
        {VERI_REAL_NUM, "VERI_REAL_NUM"},
        {VERI_REAL, "VERI_REAL"},
        {VERI_LONGREAL, "VERI_LONGREAL"},
        {VERI_UNION, "VERI_UNION"},
        {VERI_CLASS, "VERI_CLASS"},
        {VERI_PACKAGE, "VERI_PACKAGE"},
        {VERI_STRING, "VERI_STRING"},
        {VERI_REALTIME, "VERI_REALTIME"},
        {VERI_TIME, "VERI_TIME"}
    };

    portDump(std::string fileName): file(fileName) {}

    virtual void saveVeriInfo(Array *verilogModules, json& portInfo);

    void saveVeriModulePortsInfo(VeriModule* veriMod, json& module);

    long parseVhdlExpression(VhdlExpression *expr);

    std::string parseVhdlExpressionStr(VhdlExpression *expr);

    void parseVhdlRange(VhdlDiscreteRange *pDiscreteRange, int& msb, int& lsb);

    virtual void saveVhdlInfo(Array *vhdlModules, json& portInfo);

    void saveVhdlModulePortsInfo(VhdlPrimaryUnit* mod, json& module);

    void saveInfo(Array* verilogModules, Array* vhdlModules) {
        saveVhdlInfo(vhdlModules, portInfo);
        saveVeriInfo(verilogModules, portInfo);
    }

    void saveJson() {
        std::ofstream o(file);
        o << std::setw(4) << portInfo << std::endl;
    }

    protected:
    json portInfo = json::array();
    std::string file;
};
#endif
