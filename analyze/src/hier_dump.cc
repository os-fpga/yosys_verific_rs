// ---------------------------------------------------------------
// Copyright : RapidSilicon (02.2022)
//
//
// ---------------------------------------------------------------

#include "hier_dump.h"

        std::string hierDump::getVeriMode(int mode)
        {
            switch (mode) {
                case veri_file::VERILOG_95         : return "Verilog 95" ;
                case veri_file::VERILOG_2K         : return "Verilog 2000" ;
                case veri_file::SYSTEM_VERILOG_2005: return "SystemVerilog 2005" ;
                case veri_file::SYSTEM_VERILOG_2009: return "SystemVerilog 2009" ;
                case veri_file::SYSTEM_VERILOG_2012: return "SystemVerilog 2012" ;
                case veri_file::SYSTEM_VERILOG     : return "SystemVerilog" ;
                case veri_file::VERILOG_AMS        : return "AMS" ;
                case veri_file::UNDEFINED          : return "Undefined"; 
            }
            return "Undefined" ;
        }

        std::string hierDump::getVhdlMode(int mode)
        {
            switch (mode) {
                case vhdl_file::VHDL_87            : return "VHDL_87";
                case vhdl_file::VHDL_93            : return "VHDL_93";
                case vhdl_file::VHDL_2K            : return "VHDL_2K";
                case vhdl_file::VHDL_2008          : return "VHDL_2008";
                case vhdl_file::VHDL_2019          : return "VHDL_2019";
                case vhdl_file::VHDL_PSL           : return "VHDL_PSL";
                case vhdl_file::UNDEFINED          : return "Undefined"; 
            }
            return "Undefined" ;
        }

void hierDump::saveVeriInfo(Array *verilogModules, json& tree) {
    int p;
    VeriModule* veriMod;
    FOREACH_ARRAY_ITEM(verilogModules, p, veriMod) {
        if (!veriMod)
            continue;
        json module;
        module["topModule"] = veriMod->Name();
        LineFile* lineFile;
        std::string vFile = lineFile->GetFileName(veriMod->Linefile());
        module["file"] = getFileId(vFile);
        unsigned vLine = lineFile->GetLineNo(veriMod->StartingLinefile());
        module["line"] = vLine;
        module["language"] = getVeriMode(veriMod->GetAnalysisDialect());
        saveVeriModuleParamsInfo(veriMod, module);
        std::unordered_set<std::string> portNames;
        saveVeriModulePortsInfo(veriMod, module, portNames);
        saveVeriModuleInternalSignals(veriMod, module, portNames);
        saveVeriModuleInsts(veriMod, module);
        tree.push_back(module);
    }
}

void hierDump::SetModuleId(VeriModule* veriMod, std::string paramList) {
    if (moduleIDs.find(paramList) == moduleIDs.end()) {
        json module;
        module["module"] = veriMod->Name();
        LineFile* lineFile;
        std::string vFile = lineFile->GetFileName(veriMod->Linefile());
        module["file"] = getFileId(vFile);
        unsigned vLine = lineFile->GetLineNo(veriMod->StartingLinefile());
        module["line"] = vLine;
        module["language"] = getVeriMode(veriMod->GetAnalysisDialect());
        saveVeriModuleParamsInfo(veriMod, module);
        std::unordered_set<std::string> portNames;
        saveVeriModulePortsInfo(veriMod, module, portNames);
        saveVeriModuleInternalSignals(veriMod, module, portNames);
        saveVeriModuleInsts(veriMod, module);
        modules[paramList] = module;
        moduleIDs.insert(paramList);
    }
}

std::string hierDump::getFileId(std::string fileName) {
    static int currFileID = 1;
    if (fileIDs.find(fileName) == fileIDs.end()) {
        fileIDs[fileName] = std::to_string(currFileID);
        currFileID++;
    }
    return fileIDs[fileName];
}

void hierDump::saveVeriModulePortsInfo(VeriModule* veriMod, json& module, std::unordered_set<std::string>& portNames) {
        Array* ports = veriMod->GetPorts();
        int j;
        VeriIdDef* port;
        FOREACH_ARRAY_ITEM(ports, j, port) {
            if (!port) {
                continue;
            }
            json range;
            range["msb"] = port->LeftRangeBound();
            range["lsb"] = port->RightRangeBound();
            std::string type = types.find(port->Type()) != types.end() ? types[port->Type()] : "Unknown";
            module["ports"].push_back({{"name", port->GetName()}, {"direction", directions[port->Dir()]}, {"range", range}, {"type", type}});
            portNames.insert(port->GetName());
        }
}

void hierDump::saveVeriModuleInsts(VeriModule* veriMod, json& module) {
    Array * items = veriMod->GetModuleItems();
    VeriModuleItem *mi ;
    int i;
    FOREACH_ARRAY_ITEM(items, i, mi) {
        if(!mi) {
            continue;
        }
        switch (mi->GetClassId()) {
            case ID_VERIMODULEINSTANTIATION: {
                                                 VeriModuleInstantiation* moduleInstance = static_cast<VeriModuleInstantiation*>(mi);
                                                 if (!moduleInstance->GetInstantiatedModule())
                                                     return;
                                                 unsigned i;
                                                 VeriInstId* instance;
                                                 FOREACH_ARRAY_ITEM(moduleInstance->GetInstances(), i, instance) {
                                                     LineFile* lineFile;
                                                     std::string vFile = lineFile->GetFileName(instance->Linefile());
                                                     unsigned vLine = lineFile->GetLineNo(instance->StartingLinefile());

                                                     json params = json::array();
                                                     std::string paramList = saveVeriModuleInstParamInfo(moduleInstance, params);
                                                     paramList = moduleInstance->GetModuleName() + paramList;
                                                     SetModuleId(moduleInstance->GetInstantiatedModule(), paramList);
                                                     module["moduleInsts"].push_back({{"instName", instance->Name()}, {"file", getFileId(vFile)}, {"line", vLine}, {"parameters", params}, {"module", paramList}});
                                                 }
                                             }
            default: {
                         // Skip all other Ids
                         break;
                     }
        }
    }
}

std::string hierDump::getParamValue(VeriExpression *connection) {
    switch (connection->GetClassId()) {
        case ID_VERICONSTVAL: {
                                  return std::to_string(static_cast<VeriConstVal*>(connection)->Integer());
                              }
        case ID_VERIINTVAL: {
                                return std::to_string(static_cast<VeriConst*>(connection)->Integer());
                            }
        case ID_VERIIDREF: {
                               VeriIdRef *idRef = static_cast<VeriIdRef*>(connection);
                               VeriIdDef *id = idRef->FullId();
                               VeriExpression *initVal = id->GetInitialValue() ; 
                               return getParamValue(initVal);
                           }
        default: {
                     return "Unknown";
                 }
    }
}

std::string hierDump::saveVeriModuleInstParamInfo(VeriModuleInstantiation* veriMod, json& module) {
    std::string paramList;
    unsigned i ;
    VeriExpression *param ;
    FOREACH_ARRAY_ITEM(veriMod->GetParamValues(), i, param) {
        if (!param)
            continue;
        if (param->GetClassId() == ID_VERIPORTCONNECT) {
            VeriPortConnect *port = static_cast<VeriPortConnect*>(param);
            VeriExpression *connection = port->GetConnection();
            std::string val = getParamValue(connection);
            paramList.append("_");
            paramList.append(port->NamedFormal());
            paramList.append("#");
            paramList.append(val);
            module.push_back({{"name", port->NamedFormal()}, {"value", val}});
        }
    }
    return paramList;
}


void hierDump::saveVeriModuleInternalSignals(VeriModule* veriMod, json& module, std::unordered_set<std::string>& portNames) {
    Array * items = veriMod->GetModuleItems();
    VeriModuleItem *mi ;
    int i;
    FOREACH_ARRAY_ITEM(items, i, mi) {
        if(!mi) {
            continue;
        }
        switch (mi->GetClassId()) {
            case ID_VERINETDECL:
            case ID_VERIDATADECL: {
                                      VeriDataDecl *dataDecl = static_cast<VeriDataDecl*>(mi) ;
                                      if (dataDecl->IsParamDecl() || dataDecl->IsLocalParamDecl() || dataDecl->IsIODecl())
                                          continue;

                                      VeriIdDef *id_def ;
                                      unsigned j ;
                                      FOREACH_ARRAY_ITEM(dataDecl->GetIds(), j, id_def) {
                                          if (!id_def || portNames.find(id_def->Name()) != portNames.end()) continue ;
                                          json range;
                                          range["msb"] = id_def->LeftRangeBound();
                                          range["lsb"] = id_def->RightRangeBound();
                                          std::string type = types.find(id_def->Type()) != types.end() ? types[id_def->Type()] : "Unknown";
                                          module["internalSignals"].push_back({{"name", id_def->GetName()}, {"range", range}, {"type", type}});
                                      }
                                      break ;
                                  }
            default: {
                         // Skip all other Ids
                         break;
                     }
        }
    }
}

void hierDump::saveVeriModuleParamsInfo(VeriModule* veriMod, json& module) {
    Array* params = veriMod->GetParameters();
    unsigned i ;
    VeriIdDef *paramId ;
    FOREACH_ARRAY_ITEM(params, i, paramId) {
        if (!paramId) continue ;
        VeriParamId *id = static_cast<VeriParamId*>(paramId) ;
        VeriExpression *initVal = id->GetInitialValue() ; 
        long val = 0;
        switch (initVal->GetClassId()) {
            case ID_VERIINTVAL: {
                                    VeriIntVal *intVal = static_cast<VeriIntVal*>(initVal) ;
                                    val = intVal->GetNum();
                                    break;
                                }
            default: {
                         std::cout << "Unknown value: " << initVal->GetClassId() << " for parameter " << paramId->GetName() << std::endl;
                         break;
                     }
        }
        module["parameters"].push_back({{"name", paramId->GetName()}, {"value", val}});
    }
}
    void hierDump::saveInfo(Array* verilogModules, Array* vhdlModules) {
        saveVeriInfo(verilogModules, tree);
    }

    void hierDump::saveJson() {
        json fileMap;
        for (auto id : fileIDs) {
            fileMap[id.second] = id.first;
        }

        json result;
        result["fileIDs"] = fileMap;
        result["modules"] = modules;
        result["hierTree"] = tree;
        std::ofstream o(file);
        o << std::setw(4) << result << std::endl;
    }
