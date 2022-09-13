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
    {VERI_WIRE, "LOGIC"},
    {VERI_REG, "REG"}
};

void save_veri_module_ports_info(Array *verilog_modules, json& port_info) {
    int p;
    VeriModule* veri_mod;
    FOREACH_ARRAY_ITEM(verilog_modules, p, veri_mod) {
        json module;
        module["topModule"] = veri_mod->Name();
        Array* ports = veri_mod->GetPorts();
        int j;
        VeriIdDef* port;
        FOREACH_ARRAY_ITEM(ports, j, port) {
            json range;
            range["msb"] = port->LeftRangeBound();
            range["lsb"] = port->RightRangeBound();
            std::string type = types.find(port->Type()) != types.end() ? types[port->Type()] : "Unknown";
            module["ports"].push_back({{"name", port->GetName()}, {"direction", directions[port->Dir()]}, {"range", range}, {"type", type}});
        }
        port_info.push_back(module);
    }
}

void parse_vhdl_range(VhdlDiscreteRange *pDiscreteRange, int& msb, int& lsb) {
    if (pDiscreteRange && pDiscreteRange->GetClassId() == ID_VHDLRANGE) {
        VhdlRange *pRange = static_cast<VhdlRange*>(pDiscreteRange) ;
        VhdlExpression *pLHS = pRange->GetLeftExpression() ;
        VhdlExpression *pRHS = pRange->GetRightExpression() ;
        if (pRange->GetDir() == VHDL_downto) {
            msb = static_cast<VhdlInteger*>(pLHS)->GetValue();
            lsb = static_cast<VhdlInteger*>(pRHS)->GetValue();
        } else {
            msb = static_cast<VhdlInteger*>(pRHS)->GetValue();
            lsb = static_cast<VhdlInteger*>(pLHS)->GetValue();
        }
    }
}

void save_vhdl_module_ports_info(Array *vhdl_modules, Array *netlists, json& port_info) {
    int k;
    VhdlPrimaryUnit* mod;
    FOREACH_ARRAY_ITEM(vhdl_modules, k, mod) {
        json module;
        module["topModule"] = mod->Name();
        Array* ports = mod->GetPortClause();
        int j;
        VhdlInterfaceDecl* port;
        FOREACH_ARRAY_ITEM(ports, j, port) {
            int msb = 0;
            int lsb = 0;
            std::string port_type = "Unknown";
            VhdlSubtypeIndication* type = port->GetSubtypeIndication();
            switch(type->GetClassId()) {
                case ID_VHDLINDEXEDNAME:
                    port_type = type->GetPrefix()->OrigName();
                    unsigned i ;
                    VhdlDiscreteRange *pDiscreteRange ;
                    FOREACH_ARRAY_ITEM(type->GetAssocList(), i, pDiscreteRange) {
                        parse_vhdl_range(pDiscreteRange, msb, lsb);
                    }
                    break; 
                case ID_VHDLIDREF:
                    port_type = static_cast<VhdlIdRef*>(type)->GetSingleId()->Name();
                    break; 
                case ID_VHDLEXPLICITSUBTYPEINDICATION:
                    port_type = type->GetTypeMark()->Name();
                    pDiscreteRange = static_cast<VhdlExplicitSubtypeIndication*>(type)->GetRangeConstraint();
                    parse_vhdl_range(pDiscreteRange, msb, lsb);
                    break; 
                default:
                    std::cout << "Unknown type: " << type->GetClassId() << std::endl;
                    break; 
            }
            Array *ppp = port->GetIds();
            int ii;
            VhdlInterfaceId* p;
            FOREACH_ARRAY_ITEM(ppp, ii, p) {
                json range;
                range["msb"] = msb;
                range["lsb"] = lsb;
                module["ports"].push_back({{"name", p->Name()}, {"direction", vhdl_directions[p->Mode()]}, {"range", range}, {"type", port_type}});
            }
        }
        port_info.push_back(module);
    }
}

bool get_packages_path(fs::path program_path, fs::path& packages_path) {
    std::error_code ec;
    packages_path = fs::canonical(program_path.remove_filename(), ec).parent_path();
    packages_path = packages_path / "share" / "verific" / "vhdl_packages";
    if (!ec && fs::is_directory(packages_path))
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
        if (!get_packages_path(argv[0], vhdl_packages)) {
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
            std::string work = "work";
            unsigned analysis_mode = veri_file::UNDEFINED;
            int argidx = 0;

            if (size == 0 || args[argidx][0] == '#') {
                continue;
            }

            if (args[argidx] == "-vlog-incdir") {
                while (++argidx < size)
                    verific_incdirs.push_back(args[argidx]);
                continue;
            }

            if (args[argidx] == "-vlog-libdir") {
                while (++argidx < size)
                    verific_libdirs.push_back(args[argidx]);
                continue;
            }

            if (args[argidx] == "-vlog-libext") {
                while (++argidx < size)
                    verific_libexts.push_back(args[argidx]);
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

            if (args[argidx] == "-top" && argidx + 1 < size) {
                top_module = args[++argidx];
                continue;
            }

            if (args[argidx] == "-work" && argidx + 1 < size) {
                work = args[++argidx];
                argidx++;
            }

            if (args[argidx] == "-L" && argidx + 1 < size) {
                veri_file::AddLOption(args[++argidx].c_str());
                argidx++;
            }

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
                        veri_file::DefineCmdLineMacro(name.c_str(), value.c_str());
                    } else {
                        veri_file::DefineCmdLineMacro(name.c_str());
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
                std::cout << "ERROR: Unknown instruction is specified: " << line << std::endl;
                return 1;
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

            for (auto &dir : verific_incdirs)
                veri_file::AddIncludeDir(dir.c_str());
            for (auto &dir : verific_libdirs)
                veri_file::AddYDir(dir.c_str());
            for (auto &ext : verific_libexts)
                veri_file::AddLibExt(ext.c_str());

            verific_incdirs.clear();
            verific_libdirs.clear();
            verific_libexts.clear();
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
