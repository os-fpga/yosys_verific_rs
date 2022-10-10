// ---------------------------------------------------------------
// Copyright : RapidSilicon (02.2022)
//
//
// ---------------------------------------------------------------

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string.h>
#include <filesystem>
#include <unordered_map>
#include <set>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#include <limits.h>
#elif _WIN32
#include <Windows.h>
#include <limits>
#endif

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

#ifdef PRODUCTION_BUILD
#include "License_manager.hpp"
#endif

namespace fs = std::filesystem;

using namespace Verific ;
using json = nlohmann::json;

std::unordered_map<int, std::string> directions = {
    {VERI_INPUT, "Input"},
    {VERI_OUTPUT, "Output"},
    {VERI_INOUT, "Inout"}
};

std::unordered_map<int, std::string> vhdl_directions = {
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

void save_veri_module_ports_info(Array *verilog_modules, json& port_info) {
    int p;
    VeriModule* veri_mod;
    FOREACH_ARRAY_ITEM(verilog_modules, p, veri_mod) {
        if (!veri_mod)
            continue;
        json module;
        module["topModule"] = veri_mod->Name();
        Array* ports = veri_mod->GetPorts();
        int j;
        VeriIdDef* port;
        FOREACH_ARRAY_ITEM(ports, j, port) {
            if (!port)
                continue;
            json range;
            range["msb"] = port->LeftRangeBound();
            range["lsb"] = port->RightRangeBound();
            std::string type = types.find(port->Type()) != types.end() ? types[port->Type()] : "Unknown";
            module["ports"].push_back({{"name", port->GetName()}, {"direction", directions[port->Dir()]}, {"range", range}, {"type", type}});
        }
        port_info.push_back(module);
    }
}

long parse_vhdl_expression(VhdlExpression *expr) {
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
                            return parse_vhdl_expression(interfaceId->GetInitAssign());
                        }
                    case ID_VHDLCONSTANTID:
                        {
                            // TODO: get value of const declaration. 
                            // VhdlConstantId *constId = static_cast<VhdlConstantId*>(id);
                            return 0;
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
                long l = parse_vhdl_expression(pOperator->GetLeftExpression());
                long r = parse_vhdl_expression(pOperator->GetRightExpression());
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
        default:
            {
                std::cout << "Unknown type: " << expr->GetClassId() << std::endl;
                return 0;
            }
    }
}

void parse_vhdl_range(VhdlDiscreteRange *pDiscreteRange, int& msb, int& lsb) {
    if (pDiscreteRange && pDiscreteRange->GetClassId() == ID_VHDLRANGE) {
        VhdlRange *pRange = static_cast<VhdlRange*>(pDiscreteRange) ;
        VhdlExpression *pLHS = pRange->GetLeftExpression() ;
        VhdlExpression *pRHS = pRange->GetRightExpression() ;
        msb = parse_vhdl_expression(pLHS);
        lsb = parse_vhdl_expression(pRHS);
        if (pRange->GetDir() != VHDL_downto) {
            std::swap(lsb, msb);
        }
    }
}

void save_vhdl_module_ports_info(Array *vhdl_modules, Array *netlists, json& port_info) {
    int k;
    VhdlPrimaryUnit* mod;
    FOREACH_ARRAY_ITEM(vhdl_modules, k, mod) {
        if (!mod)
            return;

        json module;
        module["topModule"] = mod->Name();
        Array* ports = mod->GetPortClause();
        int j;
        VhdlInterfaceDecl* port;
        FOREACH_ARRAY_ITEM(ports, j, port) {
            if (!port)
                return;
            int msb = 0;
            int lsb = 0;
            std::string port_type = "Unknown";
            VhdlSubtypeIndication* type = port->GetSubtypeIndication();
            if (!type)
                return;
            switch(type->GetClassId()) {
                case ID_VHDLINDEXEDNAME: {
                                             port_type = type->GetPrefix() ? type->GetPrefix()->OrigName() : "Unknown";
                                             unsigned i ;
                                             VhdlDiscreteRange *pDiscreteRange ;
                                             FOREACH_ARRAY_ITEM(type->GetAssocList(), i, pDiscreteRange) {
                                                 parse_vhdl_range(pDiscreteRange, msb, lsb);
                                             }
                                             break; 
                                         }
                case ID_VHDLIDREF: {
                                       VhdlIdRef* ref = static_cast<VhdlIdRef*>(type);
                                       port_type = ref->GetSingleId() ? ref->GetSingleId()->Name() : "Unknown";
                                       break; 
                                   }
                case ID_VHDLEXPLICITSUBTYPEINDICATION: {
                                                           port_type = type->GetTypeMark() ? type->GetTypeMark()->Name() : "Unknown";
                                                           VhdlDiscreteRange *pDiscreteRange = static_cast<VhdlExplicitSubtypeIndication*>(type)->GetRangeConstraint();
                                                           parse_vhdl_range(pDiscreteRange, msb, lsb);
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
                    return;
                json range;
                range["msb"] = msb;
                range["lsb"] = lsb;
                module["ports"].push_back({{"name", p->Name()}, {"direction", vhdl_directions[p->Mode()]}, {"range", range}, {"type", port_type}});
            }
        }
        port_info.push_back(module);
    }
}

// ------------------------------
// getFullPath
// ------------------------------
bool getFullPath(const fs::path& path,
        fs::path* result) {
    std::error_code ec;
    fs::path fullPath = fs::canonical(path, ec);
    bool found = (!ec && fs::is_regular_file(fullPath));
    if (result != nullptr) {
        *result = found ? fullPath : path;
    }
    return found;
}

// ------------------------------
// GetProgramNameAbsolutePath
// ------------------------------
static fs::path GetProgramNameAbsolutePath(const char* progname) {
    char buf[PATH_MAX];
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32)
    int res = GetModuleFileNameA(0, buf, PATH_MAX - 1);
    const char PATH_DELIMITER = ';';
#else
    int res = readlink("/proc/self/exe", buf, PATH_MAX);
    const char PATH_DELIMITER = ':';
#endif

    if (0 > res || res >= PATH_MAX - 1) {
        return fs::path(progname);
    }
    buf[res] = '\0';
    fs::path program;
    if (getFullPath(fs::path(buf).replace_filename(progname), &program)) {
        return program;
    }  

    const char* const path = std::getenv("PATH");
    if (path != nullptr) {
        std::stringstream search_path(path);
        std::string path_element;
        while (std::getline(search_path, path_element, PATH_DELIMITER)) {
            const fs::path testpath =
                path_element / fs::path(progname);
            if (getFullPath(testpath, &program)) {
                return program;
            }
        }
    }

    return progname;  // Didn't find anything, return progname as-is.
}

bool get_packages_path(const char* progName, fs::path& packages_path) {
    packages_path = GetProgramNameAbsolutePath(progName).parent_path().parent_path();
    packages_path = packages_path / "share" / "verific" / "vhdl_packages";
    if (fs::is_directory(packages_path))
        return true;

    std::cout << "ERROR: Could not find VHDL packages in : " << packages_path << std::endl;
    return false;
}

// ------------------------------
// main
// ------------------------------
int main (int argc, char* argv[]) {

    try {
#ifdef PRODUCTION_BUILD
        License_Manager license(License_Manager::LicensedProductName::ANALYZE);
#endif
        std::string top_module;
        fs::path file_path;
        std::set<std::string> works;

        if (argc < 3) {
            std::cout << "./analyze -f <path_to_instruction_file>\n\n";
            std::cout << "The complete list of supported instructions:\n";
            std::cout << "{-vlog95|-vlog2k|-sv2005|-sv2009|-sv2012|-sv} [-D<macro>[=<value>]] <verilog-file/files>\n";
            std::cout << "{-vhdl87|-vhdl93|-vhdl2k|-vhdl2008|-vhdl} <vhdl-file/files>\n";
            std::cout << "-work <libname> {-sv|-vhdl|...} <hdl-file/files>\n";
            std::cout << "-L <libname> {-sv|-vhdl|...} <hdl-file/files>\n";
            std::cout << "-vlog-incdir <directory>\n";
            std::cout << "-vlog-libdir <directory>\n";
            std::cout << "-vlog-define <macro>[=<value>]\n";
            std::cout << "-vlog-undef <macro>\n";
            std::cout << "-top <top-module>\n";
            std::cout << "-set-error <msg_id/ids>\n";
            std::cout << "-set-warning <msg_id/ids>\n";
            std::cout << "-set-info <msg_id/ids>\n";
            std::cout << "-set-ignore <msg_id/ids>\n";
            return 1;
        }

        if (std::string(argv[1]) == "-f") {
            file_path = std::string(argv[2]);
        }

        std::ifstream in(file_path);
        if (!in) {
            std::cout << "ERROR: Could not open instruction file: " << file_path << std::endl;
            return 1;
        }

        std::vector<std::string> verific_incdirs;
        std::vector<std::string> verific_libdirs;
        std::vector<std::string> verific_libexts;

        fs::path vhdl_packages;
        if (!get_packages_path("analyze", vhdl_packages)) {
            return 1;
        }

        std::string line;
        while (std::getline(in, line)) {
            std::istringstream buffer(line);
            std::vector<std::string> args;

            std::copy(std::istream_iterator<std::string>(buffer),
                    std::istream_iterator<std::string>(),
                    std::back_inserter(args));

            int size = args.size();
            int argidx = 0;

            if (size == 0 || args[argidx][0] == '#') {
                continue;
            }

            while (argidx < size) { 

                std::string work = "work";
                unsigned analysis_mode = veri_file::UNDEFINED;

                if (args[argidx] == "-set-error" || args[argidx] == "-set-warning" ||
                        args[argidx] == "-set-info" || args[argidx] == "-set-ignore")
                {
                    msg_type_t type;

                    if (args[argidx] == "-set-error")
                        type = VERIFIC_ERROR;
                    else if (args[argidx] == "-set-warning")
                        type = VERIFIC_WARNING;
                    else if (args[argidx] == "-set-info")
                        type = VERIFIC_INFO;
                    else
                        type = VERIFIC_IGNORE;

                    while (++argidx < size)
                        Message::SetMessageType(args[argidx].c_str(), type);

                    continue;
                }

                if (args[argidx] == "-vlog-incdir") {
                    while (++argidx < size)
                        verific_incdirs.push_back(args[argidx]);

                    for (auto &dir : verific_incdirs)
                        veri_file::AddIncludeDir(dir.c_str());
                    verific_incdirs.clear();
                    continue;
                }

                if (args[argidx] == "-vlog-libdir") {
                    while (++argidx < size)
                        verific_libdirs.push_back(args[argidx]);

                    for (auto &dir : verific_libdirs)
                        veri_file::AddYDir(dir.c_str());
                    verific_libdirs.clear();
                    continue;
                }

                if (args[argidx] == "-vlog-libext") {
                    while (++argidx < size)
                        verific_libexts.push_back(args[argidx]);

                    for (auto &ext : verific_libexts)
                        veri_file::AddLibExt(ext.c_str());
                    verific_libexts.clear();
                    continue;
                }

                if (args[argidx] == "-vlog-define") {
                    while (++argidx < size) {
                        std::string name = args[argidx];
                        size_t equal = name.find('=');
                        if (equal != std::string::npos) {
                            std::string value = name.substr(equal+1);
                            name = name.substr(0, equal);
                            veri_file::DefineCmdLineMacro(name.c_str(), value.c_str());
                        } else {
                            veri_file::DefineCmdLineMacro(name.c_str());
                        }
                    }
                    continue;
                }

                if (args[argidx] == "-vlog-undef") {
                    while (++argidx < size) {
                        std::string name = args[argidx];
                        veri_file::UndefineMacro(name.c_str());
                    }
                    continue;
                }

                if (argidx + 1 < size && args[argidx] == "-top") {
                    top_module = args[++argidx];
                    argidx++;
                }

                if (argidx + 1 < size && args[argidx] == "-work") {
                    work = args[++argidx];
                    argidx++;
                }

                if (argidx + 1 < size && args[argidx] == "-L") {
                    veri_file::AddLOption(args[++argidx].c_str());
                    argidx++;
                }

                if (argidx >= size)
                    continue;

                if (args[argidx] == "-vlog95") {
                    analysis_mode = veri_file::VERILOG_95;
                } else if (args[argidx] == "-vlog2k") {
                    analysis_mode = veri_file::VERILOG_2K;
                } else if (args[argidx] == "-sv2005") {
                    analysis_mode = veri_file::SYSTEM_VERILOG_2005;
                } else if (args[argidx] == "-sv2009") {
                    analysis_mode = veri_file::SYSTEM_VERILOG_2009;
                } else if (args[argidx] == "-sv2012" || args[argidx] == "-sv" || args[argidx] == "-formal") {
                    analysis_mode = veri_file::SYSTEM_VERILOG;
                }

                if(analysis_mode != veri_file::UNDEFINED) {
                    Array file_names;
                    argidx++;

                    while(argidx < size && args[argidx].compare(0, 2, "-D") == 0) {
                        std::string name = args[argidx].substr(2);
                        size_t equal = name.find('=');
                        if (equal != std::string::npos) {
                            std::string value = name.substr(equal+1);
                            name = name.substr(0, equal);
                            veri_file::DefineMacro(name.c_str(), value.c_str());
                        } else {
                            veri_file::DefineMacro(name.c_str());
                        }
                        argidx++;
                    }

                    works.insert(work);
                    while (argidx < size) {
                        if (args[argidx].find("*.") != std::string::npos) {
                            fs::path dir = fs::current_path();
                            std::string file = args[argidx].substr(0, args[argidx].find("*."));
                            if (!file.empty())
                                dir = file;
                            for (auto const& dir_entry : fs::directory_iterator{dir}) 
                                if (dir_entry.path().extension() == ".v" || dir_entry.path().extension() == ".sv")
                                    file_names.Insert(Strings::save(dir_entry.path().c_str()));
                            argidx++;
                        } else {
                            file_names.Insert(Strings::save(args[argidx++].c_str()));
                        }
                    }

                    if (!veri_file::AnalyzeMultipleFiles(&file_names, analysis_mode, work.c_str(), veri_file::MFCU)) {
                        std::cout << "ERROR: Reading Verilog/SystemVerilog sources failed.\n";
                        return 1;
                    }

                    int i;
                    char *f ;
                    FOREACH_ARRAY_ITEM(&file_names, i, f) Strings::free(f); 
                    continue;
                }

                if (args[argidx] == "-vhdl87") {
                    analysis_mode = vhdl_file::VHDL_87;
                    vhdl_file::SetDefaultLibraryPath((vhdl_packages / "vdbs_1987").c_str());
                } else if (args[argidx] == "-vhdl93") {
                    analysis_mode = vhdl_file::VHDL_93;
                    vhdl_file::SetDefaultLibraryPath((vhdl_packages / "vdbs_1993").c_str());
                } else if (args[argidx] == "-vhdl2k") {
                    analysis_mode = vhdl_file::VHDL_2K;
                    vhdl_file::SetDefaultLibraryPath((vhdl_packages / "vdbs_1993").c_str());
                } else if (args[argidx] == "-vhdl2008" || args[argidx] == "-vhdl") {
                    analysis_mode = vhdl_file::VHDL_2008;
                    vhdl_file::SetDefaultLibraryPath((vhdl_packages / "vdbs_2008").c_str());
                } else {
                    continue;
                }

                if(analysis_mode != veri_file::UNDEFINED) {
                    argidx++;
                    works.insert(work);
                    while (argidx < size) {
                        if (args[argidx].find("*.") != std::string::npos) {
                            fs::path dir = fs::current_path();
                            std::string file = args[argidx].substr(0, args[argidx].find("*."));
                            if (!file.empty())
                                dir = file;
                            for (auto const& dir_entry : fs::directory_iterator{dir}) 
                                if (dir_entry.path().extension() == ".vhd")
                                    if (!vhdl_file::Analyze(dir_entry.path().c_str(), work.c_str(), analysis_mode)) {
                                        std::cout << "ERROR: Reading vhdl source failed:\n";
                                        return 1;
                                    }
                            argidx++;
                        } else {
                            if (!vhdl_file::Analyze(args[argidx++].c_str(), work.c_str(), analysis_mode)) {
                                std::cout << "ERROR: Reading vhdl source failed:\n";
                                return 1;
                            }
                        }
                    }
                }
            }
        }

        json port_info = json::array();

        for (auto &w : works) {
            VeriLibrary *veri_lib = veri_file::GetLibrary(w.c_str(), 1);
            VhdlLibrary *vhdl_lib = vhdl_file::GetLibrary(w.c_str(), 1);

            Array *netlists;
            if (!top_module.empty()) {
                Array veri_modules, vhdl_units;
                VeriModule *veri_module = veri_lib ? veri_lib->GetModule(top_module.c_str(), 1) : nullptr;
                if (veri_module) {
                    veri_modules.InsertLast(veri_module);
                }
                VhdlDesignUnit *vhdl_unit = vhdl_lib ? vhdl_lib->GetPrimUnit(top_module.c_str()) : nullptr;
                if (vhdl_unit) {
                    vhdl_units.InsertLast(vhdl_unit);
                }
                netlists = hier_tree::Elaborate(&veri_modules, &vhdl_units, 0);
                save_vhdl_module_ports_info(&vhdl_units, netlists, port_info);
                save_veri_module_ports_info(&veri_modules, port_info);
            } else {
                Array veri_libs, vhdl_libs;
                if (vhdl_lib) vhdl_libs.InsertLast(vhdl_lib);
                if (veri_lib) veri_libs.InsertLast(veri_lib);
                netlists = hier_tree::ElaborateAll(&veri_libs, &vhdl_libs, 0);
                Array *verilog_modules = veri_file::GetTopModules(w.c_str());
                Array *vhdl_modules = vhdl_file::GetTopDesignUnits(w.c_str());

                save_vhdl_module_ports_info(vhdl_modules, netlists, port_info);
                save_veri_module_ports_info(verilog_modules, port_info);
            }

        }

        std::ofstream o("port_info.json");
        o << std::setw(4) << port_info << std::endl;

        return 0;
    }
    catch (fs::filesystem_error const& ex) {
        std::cout << "ANALYZE: ERROR : " << ex.what() << std::endl;
        return 1;
    }
#ifdef PRODUCTION_BUILD
    catch (License_Manager::LicenseFatalException const& ex){
        std::cout << "ANALYZE: ERROR : " << ex.what() << std::endl;
        return 1;
    }
    catch (License_Manager::LicenseCorrectableException const& ex){
        std::cout << "ANALYZE: ERROR : " << ex.what() << std::endl;
        return 1;
    }
#endif
    catch (...) {
        std::cout << "ANALYZE: ERROR : Unhandled exception." << std::endl;
        return 1;
    }
}
