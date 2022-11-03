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

#include "port_dump.h"
#include "hier_dump.h"

#ifdef PRODUCTION_BUILD
#include "License_manager.hpp"
#endif

namespace fs = std::filesystem;
using namespace Verific ;

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

        int argidx = 1;
        bool dumpHierTree = false;

        while (argidx < argc) {
            if (std::string(argv[argidx]) == "-f") {
                file_path = std::string(argv[++argidx]);
                argidx++;
                continue;
            }

            if (std::string(argv[argidx]) == "-dump_hier_tree") {
                dumpHierTree = true;
                argidx++;
            }
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

                    for (auto &dir : verific_incdirs)
                        veri_file::AddIncludeDir(dir.c_str());
                    for (auto &dir : verific_libdirs)
                        veri_file::AddYDir(dir.c_str());
                    for (auto &ext : verific_libexts)
                        veri_file::AddLibExt(ext.c_str());

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

        verific_incdirs.clear();
        verific_libdirs.clear();
        verific_libexts.clear();

        portDump *ports = new portDump("port_info.json");
        hierDump *hierTree = new hierDump("hier_info.json");

        for (auto &w : works) {
            VeriLibrary *veri_lib = veri_file::GetLibrary(w.c_str(), 1);
            VhdlLibrary *vhdl_lib = vhdl_file::GetLibrary(w.c_str(), 1);

            Array *modules;
            Array *units;
            if (!top_module.empty()) {
                Array m;
                Array u;
                VeriModule *veri_module = veri_lib ? veri_lib->GetModule(top_module.c_str(), 1) : nullptr;
                if (veri_module) {
                    m.InsertLast(veri_module);
                }
                VhdlDesignUnit *vhdl_unit = vhdl_lib ? vhdl_lib->GetPrimUnit(top_module.c_str()) : nullptr;
                if (vhdl_unit) {
                    u.InsertLast(vhdl_unit);
                }
                hier_tree::Elaborate(&m, &u, 0);
                modules = &m;
                units = &u;
            } else {
                Array veri_libs, vhdl_libs;
                if (vhdl_lib) vhdl_libs.InsertLast(vhdl_lib);
                if (veri_lib) veri_libs.InsertLast(veri_lib);
                hier_tree::ElaborateAll(&veri_libs, &vhdl_libs, 0);
                modules = veri_file::GetTopModules(w.c_str());
                units = vhdl_file::GetTopDesignUnits(w.c_str());
            }
                ports->saveInfo(modules, units);
                if (dumpHierTree)
                    hierTree->saveInfo(modules, units);
        }

        ports->saveJson();
        if (dumpHierTree)
            hierTree->saveJson();

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
