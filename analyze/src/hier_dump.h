// ---------------------------------------------------------------
// Copyright : RapidSilicon (02.2022)
//
//
// ---------------------------------------------------------------

#ifndef HIER_DUMP_H
#define HIER_DUMP_H

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string.h>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>

#include <nlohmann_json/json.hpp>

#include "veri_file.h"
#include "vhdl_file.h"
#include "VeriModule.h"
#include "VhdlUnits.h"
#include "VhdlIdDef.h"
#include "VhdlStatement.h"
#include "VeriId.h"
#include "VeriConstVal.h"
#include "file_sort_veri_tokens.h"
#include "hier_tree.h"
#include "Netlist.h"
#include "VhdlExpression.h"
#include "VhdlName.h"
#include "VeriExpression.h"
#include "VhdlValue_Elab.h"
#include "file_sort_vhdl_tokens.h"
#include "hdl_file_sort.h"
#include "port_dump.h"

using namespace Verific ;
using json = nlohmann::json;

class hierDump : public portDump {
    public:
        hierDump(std::string fileName): portDump(fileName) {}

        std::string getVeriMode(int mode);

        std::string getVhdlMode(int mode);

        void saveVeriInfo(Array *verilogModules, json& hierInfo);

        void saveVhdlInfo(Array *vhdlModules, json& hierInfo);

        void SetVeriModuleId(VeriModule* veri_mod, std::string paramList);

        std::string getFileId(std::string fileName);

        void saveVeriModulePortsInfo(VeriModule* veriMod, json& module, std::unordered_set<std::string>& portNames);

        void saveVeriModuleInsts(VeriModule* veriMod, json& module);

        std::string getParamValue(VeriExpression *connection);

        std::string saveVeriModuleInstParamInfo(VeriModuleInstantiation* veriMod, json& module);

        std::vector<std::string> getVeriModuleParamList(VeriModuleInstantiation* veriMod);

        void saveVeriModuleInternalSignals(VeriModule* veriMod, json& module, std::unordered_set<std::string>& portNames);

        void saveVeriModuleParamsInfo(VeriModule* veriMod, json& module);

        void saveVhdlModuleParamsInfo(VhdlPrimaryUnit* mod, json& module);

        void saveVhdlModulePortsInfo(VhdlPrimaryUnit* mod, json& module, std::unordered_set<std::string>& portNames);

        void saveVhdlModuleInternalSignals(VhdlPrimaryUnit* mod, json& module, std::unordered_set<std::string>& portNames);

        void saveVhdlModuleInsts(VhdlPrimaryUnit* mod, json& module);

        std::string saveVhdlModuleInstParamInfo(VhdlComponentInstantiationStatement* mod, json& module);

        void SetVhdlModuleId(VhdlPrimaryUnit* mod, std::string paramList);

        void saveInfo(Array* verilogModules, Array* vhdlModules);

        void saveJson();

    private:
        std::unordered_map<std::string, std::string> fileIDs;
        std::unordered_set<std::string> moduleIDs;
        json modules;
        json tree;
};

#endif
