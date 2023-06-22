// ---------------------------------------------------------------
// Copyright : RapidSilicon (02.2022)
//
//
// ---------------------------------------------------------------

#include "port_dump.h"

void portDump::saveVeriInfo(Array *verilogModules, json& portInfo) {
    int p;
    VeriModule* veriMod;
    FOREACH_ARRAY_ITEM(verilogModules, p, veriMod) {
        if (!veriMod)
            continue;
        json module;
        module["topModule"] = veriMod->Name();
        saveVeriModulePortsInfo(veriMod, module);
        portInfo.push_back(module);
    }
}

void portDump::saveVeriModulePortsInfo(VeriModule* veriMod, json& module) {
    Array* ports = veriMod->GetPorts();
    int j;
    VeriIdDef* port;
    FOREACH_ARRAY_ITEM(ports, j, port) {
        if (!port)
            continue;
        json range;
        range["msb"] = port->LeftRangeBound();
        range["lsb"] = port->RightRangeBound();
        std::string type = types.find(port->Type()) != types.end() ? types[port->Type()] : "Unknown";
        module["ports"].push_back({{"name", port->GetName()}, 
                {"direction", directions[port->Dir()]}, 
                {"range", range}, 
                {"type", type}});
    }
}

std::string portDump::parseVhdlExpressionStr(VhdlExpression *expr) {
    std::string res;
    if (!expr)
        return res;
    switch (expr->GetClassId()) {
        case ID_VHDLBITSTRINGLITERAL:
            {
                res = static_cast<VhdlBitStringLiteral*>(expr)->GetBasedBitString();
                break;
            }
        default:
            {
                res = std::to_string(parseVhdlExpression(expr));
                break;
            }
    }
    return res;
}

long portDump::parseVhdlExpression(VhdlExpression *expr) {
    if (!expr)
        return 0;
    switch (expr->GetClassId()) {
        case ID_VHDLIDREF:
            {
                VhdlIdRef *ref = static_cast<VhdlIdRef*>(expr);
                if (!ref)
                    return 0;
                VhdlIdDef *id = ref->GetSingleId();
                if (!id)
                    return 0;
                switch (id->GetClassId()) {
                    case ID_VHDLINTERFACEID:
                        {
                            VhdlInterfaceId *interfaceId = static_cast<VhdlInterfaceId*>(id);
                            return parseVhdlExpression(interfaceId->GetInitAssign());
                        }
                    case ID_VHDLCONSTANTID:
                        {
                            // TODO: get value of const declaration. 
                            // VhdlConstantId *constId = static_cast<VhdlConstantId*>(id);
                            return 0;
                        }
                    case ID_VHDLENUMERATIONID:
                        {
                            VhdlEnumerationId *enumerationid = static_cast<VhdlEnumerationId*>(id);
                            return parseVhdlExpression(enumerationid->GetInitAssign());
                        }
                    default:
                        {
                            std::cout << "Unknown type: " << id->GetClassId() << std::endl;
                            return 0;
                        }
                }
            }
        case ID_VHDLOPERATOR:
            {
                VhdlOperator* pOperator = static_cast<VhdlOperator*>(expr);
                unsigned op = pOperator->GetOperatorToken();
                long l = parseVhdlExpression(pOperator->GetLeftExpression());
                long r = parseVhdlExpression(pOperator->GetRightExpression());
                switch (op) {
                    case VHDL_PLUS:
                        return l + r;
                    case VHDL_MINUS:
                        return l - r;
                    case VHDL_STAR:
                        return l * r;
                    case VHDL_SLASH:
                        return l / r;
                    case VHDL_rem:
                        return l % r;
                    case VHDL_EXPONENT :
                        return std::pow(l, r);
                    default:
                        std::cout << "Unknown operation: " << op << std::endl;
                        return 0;
                }
            }
        case ID_VHDLINTEGER:
            {
                return static_cast<VhdlInteger*>(expr)->GetValue();
            }
        case ID_VHDLINDEXEDNAME:
            {
                VhdlIndexedName *indexdName = static_cast<VhdlIndexedName*>(expr);
                return parseVhdlExpression(indexdName->GetPrefixId()->GetInitAssign());
            }
        default:
            {
                std::cout << "Unknown type: " << expr->GetClassId() << std::endl;
                return 0;
            }
    }
}

void portDump::parseVhdlRange(VhdlDiscreteRange *pDiscreteRange, int& msb, int& lsb) {
    if (pDiscreteRange && pDiscreteRange->GetClassId() == ID_VHDLRANGE) {
        VhdlRange *pRange = static_cast<VhdlRange*>(pDiscreteRange) ;
        VhdlExpression *pLHS = pRange->GetLeftExpression() ;
        VhdlExpression *pRHS = pRange->GetRightExpression() ;
        msb = parseVhdlExpression(pLHS);
        lsb = parseVhdlExpression(pRHS);
        if (pRange->GetDir() != VHDL_downto) {
            std::swap(lsb, msb);
        }
    }
}

void portDump::saveVhdlInfo(Array *vhdlModules, json& portInfo) {
    int k;
    VhdlPrimaryUnit* mod;
    FOREACH_ARRAY_ITEM(vhdlModules, k, mod) {
        if (!mod)
            continue;
        json module;
        module["topModule"] = mod->Name();
        saveVhdlModulePortsInfo(mod, module);
        portInfo.push_back(module);
    }
}

void portDump::saveVhdlModulePortsInfo(VhdlPrimaryUnit* mod, json& module) {
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
            }
        }
}
