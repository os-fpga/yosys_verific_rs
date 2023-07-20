// ---------------------------------------------------------------
// Copyright : RapidSilicon (02.2022)
//
//
// ---------------------------------------------------------------

#include "hier_dump.h"



#include "VhdlDataFlow_Elab.h"

bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

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
        if (veriMod->IsCellDefine())
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

void hierDump::saveVhdlInfo(Array *vhdlModules, json& tree) {
    int k;
    VhdlPrimaryUnit* mod;
    FOREACH_ARRAY_ITEM(vhdlModules, k, mod) {
        if (!mod)
            return;
        json module;
        module["topModule"] = mod->Name();
        LineFile* lineFile;
        std::string vFile = lineFile->GetFileName(mod->Linefile());
        module["file"] = getFileId(vFile);
        unsigned vLine = lineFile->GetLineNo(mod->StartingLinefile());
        module["line"] = vLine;
        module["language"] = getVhdlMode(mod->GetAnalysisDialect());
        saveVhdlModuleParamsInfo(mod, module);
        std::unordered_set<std::string> portNames;
        saveVhdlModulePortsInfo(mod, module, portNames);
        saveVhdlModuleInternalSignals(mod, module, portNames);
        saveVhdlModuleInsts(mod, module);
        tree.push_back(module);
    }
}
void hierDump::SetVeriModuleId(VeriModule* veriMod, std::string paramList) {
    if (!veriMod)
        return;
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
    if (!veriMod)
        return;
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
    if (!veriMod)
        return;
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
                                                 if (!moduleInstance->GetInstantiatedModule()) {
                                                     VhdlPrimaryUnit* primUnit = moduleInstance->GetInstantiatedUnit();
                                                     unsigned i;
                                                     VeriInstId* instance;
                                                     FOREACH_ARRAY_ITEM(moduleInstance->GetInstances(), i, instance) {
                                                         if (!instance)
                                                             continue;
                                                         LineFile* lineFile;
                                                         std::string vFile = lineFile->GetFileName(instance->Linefile());
                                                         unsigned vLine = lineFile->GetLineNo(instance->StartingLinefile());
                                                         json params = json::array();
                                                         std::string paramList = saveVeriModuleInstParamInfo(moduleInstance, params);
                                                         paramList = moduleInstance->GetModuleName() + paramList;
                                                         SetVhdlModuleId(primUnit, paramList);
							 if (instance->Name()) {
                                                           module["moduleInsts"].push_back({{"instName", instance->Name()}, {"file", getFileId(vFile)}, {"line", vLine}, {"parameters", params}, {"module", paramList}});
							 }
                                                     }
                                                 }
                                                 else {
                                                     unsigned i;
                                                     VeriInstId* instance;
                                                     FOREACH_ARRAY_ITEM(moduleInstance->GetInstances(), i, instance) {
                                                         if (!instance)
                                                             continue;
                                                         LineFile* lineFile;
                                                         std::string vFile = lineFile->GetFileName(instance->Linefile());
                                                         unsigned vLine = lineFile->GetLineNo(instance->StartingLinefile());
                                                         json params = json::array();
                                                         std::string paramList = saveVeriModuleInstParamInfo(moduleInstance, params);
                                                         paramList = moduleInstance->GetModuleName() + paramList;
                                                         SetVeriModuleId(moduleInstance->GetInstantiatedModule(), paramList);
							 // Fix EDA-1361 : make sure instance name exists
							 if (instance->Name()) {
                                                           module["moduleInsts"].push_back({{"instName", instance->Name()}, {"file", getFileId(vFile)}, {"line", vLine}, {"parameters", params}, {"module", paramList}});
							 }
                                                     }
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
    if (!connection)
        return "Unknown";
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
                               if (!id)
                                   return "Unknown";
                               VeriExpression *initVal = id->GetInitialValue() ; 
                               return getParamValue(initVal);
                           }
        default: {
                     return "Unknown";
                 }
    }
    return "Unknown";
}

std::vector<std::string> hierDump::getVeriModuleParamList(VeriModuleInstantiation* veriMod) {
    std::vector<std::string> paramList;
    if (!veriMod)
        return paramList;

    if (veriMod->GetInstantiatedModule()) {
        VeriModule* module = veriMod->GetInstantiatedModule();
        Array* params = module ? module->GetParameters() : NULL;
        unsigned i ;
        VeriIdDef *paramId ;
        FOREACH_ARRAY_ITEM(params, i, paramId) {
            if (!paramId) continue ;
            paramList.push_back(paramId->GetName());
        }
    }
    else {
        VhdlPrimaryUnit* primUnit = veriMod->GetInstantiatedUnit();
        Array * params = primUnit ? primUnit->GetGenericClause() : NULL;
        unsigned i ;
        VhdlInterfaceDecl *paramId ;
        FOREACH_ARRAY_ITEM(params, i, paramId) {
            if (!paramId) continue ;
            Array *ppp = paramId->GetIds();
            int ii;
            VhdlInterfaceId* p;
            FOREACH_ARRAY_ITEM(ppp, ii, p) {
                if (!p)
                    continue;
                paramList.push_back(p->GetPrettyPrintedString());
            }
        }
    }
    return paramList;
}

std::string hierDump::saveVeriModuleInstParamInfo(VeriModuleInstantiation* veriMod, json& module) {
    std::string paramList;
    unsigned i ;
    VeriExpression *param ;
    std::vector<std::string> list = getVeriModuleParamList(veriMod);
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
        else if (param->GetClassId() == ID_VERIINTVAL) {
            std::string val = getParamValue(param);
            paramList.append("_");
            paramList.append(list[i]);
            paramList.append("#");
            paramList.append(val);
            module.push_back({{"name", list[i]}, {"value", val}});
        }
    }
    return paramList;
}

void hierDump::saveVhdlModuleInsts(VhdlPrimaryUnit* mod, json& module) {
    MapIter mi ;
    VhdlSecondaryUnit *sec ;
    FOREACH_VHDL_SECONDARY_UNIT(mod, mi, sec) {
        if (!sec)
            continue;
        unsigned i ;
        VhdlStatement *pStmt ;
        FOREACH_ARRAY_ITEM(sec->GetStatementPart(), i, pStmt) {
            if (!pStmt)
                continue;
            switch (pStmt->GetClassId()) {
                case ID_VHDLCOMPONENTINSTANTIATIONSTATEMENT: {
                                                                 VhdlComponentInstantiationStatement *inst = static_cast<VhdlComponentInstantiationStatement*>(pStmt);
                                                                 LineFile* lineFile;
                                                                 std::string vFile = lineFile->GetFileName(inst->Linefile());
                                                                 unsigned vLine = lineFile->GetLineNo(inst->StartingLinefile());
                                                                 json params = json::array();
                                                                 std::string paramList = saveVhdlModuleInstParamInfo(inst, params);
                                                                 VhdlLibrary *instLib = vhdl_file::GetLibrary(pStmt->GetLibraryOfInstantiatedUnit(), 1) ;
                                                                 if (!instLib) 
                                                                     instLib = vhdl_file::GetWorkLib() ;
                                                                 VhdlPrimaryUnit* primUnit = instLib ? instLib->GetPrimUnit(pStmt->GetInstantiatedUnitName()) : 0 ;
                                                                 if (primUnit && primUnit->IsVerilogModule()) { // instance of Verilog module
                                                                     VeriModule *veriModule = vhdl_file::GetVerilogModuleFromlib(instLib->Name(), primUnit->Name()) ;
                                                                     SetVeriModuleId(veriModule, paramList);
                                                                 } else { // Instance of vhdl unit
                                                                     SetVhdlModuleId(primUnit, paramList);
                                                                 }
                                                                 module["moduleInsts"].push_back({{"instName", inst->GetLabel() ? inst->GetLabel()->GetPrettyPrintedString() : ""}, {"file", getFileId(vFile)}, {"line", vLine}, {"parameters", params}, {"module", paramList}});
                                                                 break;
                                                             }
                default: {
                             // Skip all other Ids
                             break;
                         }
            }
        }
    }
}

void hierDump::SetVhdlModuleId(VhdlPrimaryUnit* unit, std::string paramList) {
    if (!unit)
        return;
    if (moduleIDs.find(paramList) == moduleIDs.end()) {
        json module;
        module["module"] = unit->Name();
        LineFile* lineFile;
        std::string vFile = lineFile->GetFileName(unit->Linefile());
        module["file"] = getFileId(vFile);
        unsigned vLine = lineFile->GetLineNo(unit->StartingLinefile());
        module["line"] = vLine;
        module["language"] = getVhdlMode(unit->GetAnalysisDialect());
        saveVhdlModuleParamsInfo(unit, module);
        std::unordered_set<std::string> portNames;
        saveVhdlModulePortsInfo(unit, module, portNames);
        saveVhdlModuleInternalSignals(unit, module, portNames);

        // Thierry : we see potential recursive call (saveVhdlModuleInsts -> 
        // SetVhdlModuleId -> saveVhdlModuleInsts) so we need to add 
        // "paramList" into "moduleIDs" before caling "saveVhdlModuleInsts"
        // so that we do not re-enter this "if" block in the sub-recursive calls.
        // (but why do we have recursivity with loop here ???)
        //saveVhdlModuleInsts(unit, module);

        modules[paramList] = module;
        moduleIDs.insert(paramList);

        // Thierry : call "saveVhdlModuleInsts" after "moduleIDs.insert(paramList)"
        // so that we know we already go through here with "paramList" module ID.
        //
        saveVhdlModuleInsts(unit, module);
    }
}

std::string hierDump::saveVhdlModuleInstParamInfo(VhdlComponentInstantiationStatement* mod, json& module) {
    if (!mod)
        return "Unknown";
    VhdlIdDef* unit = mod->GetInstantiatedUnit();
    std::string paramList = mod->GetInstantiatedUnit() ? mod->GetInstantiatedUnit()->GetPrettyPrintedString() : "";
    unsigned j ;
    VhdlIdDef *param;
    FOREACH_ARRAY_ITEM(unit->GetGenerics(), j, param) {
        if (!param) continue ;
        paramList.append("_");
        paramList.append(param->GetPrettyPrintedString());
        paramList.append("#");
        std::string exprInitAssign = parseVhdlExpressionStr(param->GetInitAssign());
        paramList.append(exprInitAssign);
        module.push_back({{"name", param->GetPrettyPrintedString()}, {"value", exprInitAssign}});
    }
    return paramList;
}

void hierDump::saveVhdlModuleInternalSignals(VhdlPrimaryUnit* mod, json& module, std::unordered_set<std::string>& portNames) {
    MapIter mi ;
    VhdlSecondaryUnit *sec ;
    FOREACH_VHDL_SECONDARY_UNIT(mod, mi, sec) {
        if (!sec)
            continue;
        Array *declPart = sec->GetDeclPart();
        unsigned i ;
        VhdlTreeNode *elem ;
        FOREACH_ARRAY_ITEM(declPart, i, elem) {
            if (!elem)
                continue;
            switch (elem->GetClassId()) {
                case ID_VHDLSIGNALDECL: {
                                            VhdlSignalDecl *pSignalDecl = static_cast<VhdlSignalDecl*>(elem);
                                            int msb = 0;
                                            int lsb = 0;
                                            std::string signalType = "Unknown";
                                            VhdlSubtypeIndication* type = pSignalDecl->GetSubtypeIndication();
                                            if (!type)
                                                continue;
                                            switch(type->GetClassId()) {
                                                case ID_VHDLINDEXEDNAME: {
                                                                             signalType = type->GetPrefix() ? type->GetPrefix()->OrigName() : "Unknown";
                                                                             unsigned i ;
                                                                             VhdlDiscreteRange *pDiscreteRange ;
                                                                             FOREACH_ARRAY_ITEM(type->GetAssocList(), i, pDiscreteRange) {
                                                                                 parseVhdlRange(pDiscreteRange, msb, lsb);
                                                                             }
                                                                             break; 
                                                                         }
                                                case ID_VHDLIDREF: {
                                                                       VhdlIdRef* ref = static_cast<VhdlIdRef*>(type);
                                                                       signalType = ref->GetSingleId() ? ref->GetSingleId()->Name() : "Unknown";
                                                                       break; 
                                                                   }
                                                case ID_VHDLEXPLICITSUBTYPEINDICATION: {
                                                                                           signalType = type->GetTypeMark() ? type->GetTypeMark()->Name() : "Unknown";
                                                                                           VhdlDiscreteRange *pDiscreteRange = static_cast<VhdlExplicitSubtypeIndication*>(type)->GetRangeConstraint();
                                                                                           parseVhdlRange(pDiscreteRange, msb, lsb);
                                                                                           break; 
                                                                                       }
                                                default: {
                                                             std::cout << "Unknown type: " << type->GetClassId() << std::endl;
                                                             break;
                                                         }
                                            }
                                            unsigned i ;
                                            VhdlIdDef *pId ;
                                            FOREACH_ARRAY_ITEM(pSignalDecl->GetIds(), i, pId) {
                                                if (!pId)
                                                    continue;
                                                json range;
                                                range["msb"] = msb;
                                                range["lsb"] = lsb;
                                                module["internalSignals"].push_back({{"name", pId->Name()}, {"range", range}, {"type", signalType}});
                                            }
                                            break;
                                        }
                default: {
                             // Skip all other Ids
                             break;
                         }
            }
        }
    }
}

void hierDump::saveVeriModuleInternalSignals(VeriModule* veriMod, json& module, std::unordered_set<std::string>& portNames) {
    if (!veriMod)
        return;
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
                                          if (!id_def || portNames.find(id_def->Name()) != portNames.end())
                                              continue ;
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
    if (!veriMod)
        return;
    Array* params = veriMod->GetParameters();
    unsigned i ;
    VeriIdDef *paramId ;
    FOREACH_ARRAY_ITEM(params, i, paramId) {
        if (!paramId) continue ;
        VeriParamId *id = static_cast<VeriParamId*>(paramId) ;
        VeriExpression *initVal = id->GetInitialValue() ; 
        long val = 0;
        if (!initVal) continue ;
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

void hierDump::saveVhdlModuleParamsInfo(VhdlPrimaryUnit* mod, json& module) {
    if (!mod)
        return;
    Array * params = mod->GetGenericClause();
    unsigned i ;
    VhdlInterfaceDecl *paramId ;
    FOREACH_ARRAY_ITEM(params, i, paramId) {
        if (!paramId) continue ;
        Array *ppp = paramId->GetIds();
        int ii;
        VhdlInterfaceId* p;
        FOREACH_ARRAY_ITEM(ppp, ii, p) {
            if (!p)
                continue;
            std::string exprInitAssign = parseVhdlExpressionStr(p->GetInitAssign());
            module["parameters"].push_back({{"name", p->GetPrettyPrintedString()}, {"value", exprInitAssign}});
        }
    }
}

void hierDump::saveVhdlModulePortsInfo(VhdlPrimaryUnit* mod, json& module, std::unordered_set<std::string>& portNames) {
    if (!mod)
        return;
    Array* ports = mod->GetPortClause();
    int j;
    VhdlInterfaceDecl* port;
    FOREACH_ARRAY_ITEM(ports, j, port) {
        if (!port)
            continue;
        int msb = 0;
        int lsb = 0;
        std::string portType = "Unknown";
        VhdlSubtypeIndication* type = port->GetSubtypeIndication();
        if (!type)
            continue;
        switch(type->GetClassId()) {
            case ID_VHDLINDEXEDNAME: {
                                         portType = type->GetPrefix() ? type->GetPrefix()->OrigName() : "Unknown";
                                         unsigned i ;
                                         VhdlDiscreteRange *pDiscreteRange ;
                                         FOREACH_ARRAY_ITEM(type->GetAssocList(), i, pDiscreteRange) {
                                             parseVhdlRange(pDiscreteRange, msb, lsb);
                                         }
                                         break; 
                                     }
            case ID_VHDLIDREF: {
                                   VhdlIdRef* ref = static_cast<VhdlIdRef*>(type);
                                   portType = ref->GetSingleId() ? ref->GetSingleId()->Name() : "Unknown";
                                   break; 
                               }
            case ID_VHDLEXPLICITSUBTYPEINDICATION: {
                                                       portType = type->GetTypeMark() ? type->GetTypeMark()->Name() : "Unknown";
                                                       VhdlDiscreteRange *pDiscreteRange = static_cast<VhdlExplicitSubtypeIndication*>(type)->GetRangeConstraint();
                                                       parseVhdlRange(pDiscreteRange, msb, lsb);
                                                       break; 
                                                   }
            default: {
                         std::cout << "Unknown type: " << type->GetClassId() << std::endl;
                         break;
                     }
        }
        Array *ppp = port->GetIds();
        int ii;
        VhdlInterfaceId* p;
        FOREACH_ARRAY_ITEM(ppp, ii, p) {
            if (!p)
                continue;
            json range;
            range["msb"] = msb;
            range["lsb"] = lsb;
            module["ports"].push_back({{"name", p->Name()}, {"direction", vhdlDirections[p->Mode()]}, {"range", range}, {"type", portType}});
            portNames.insert(p->Name());
        }
    }
}

void hierDump::saveInfo(Array* verilogModules, Array* vhdlModules) {
    saveVeriInfo(verilogModules, tree);
    saveVhdlInfo(vhdlModules, tree);
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
