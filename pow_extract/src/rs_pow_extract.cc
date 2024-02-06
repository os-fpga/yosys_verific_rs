/*
 *  Copyright (C) 2023 RapidSilicon
 *
 */
#include "kernel/celltypes.h"
#include "backends/rtlil/rtlil_backend.h"
#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/mem.h"
#include "kernel/ffinit.h"
#include "kernel/ff.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <map>
#include <unordered_map>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <unistd.h>
#ifdef PRODUCTION_BUILD
#include "License_manager.hpp"
#endif

int DSP_COUNTER;
USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

#define XSTR(val) #val
#define STR(val) XSTR(val)

#ifndef PASS_NAME
#define PASS_NAME pow_extract
#endif

#define GENESIS_DIR genesis
#define GENESIS_2_DIR genesis2
#define GENESIS_3_DIR genesis3
#define COMMON_DIR common
#define SIM_LIB_FILE cells_sim.v
#define SIM_LIB_CARRY_FILE CARRY.v
#define LLATCHES_SIM_FILE llatches_sim.v
#define DSP_SIM_LIB_FILE dsp_sim.v
#define BRAMS_SIM_LIB_FILE brams_sim.v
#define BRAMS_SIM_NEW_LIB_FILE1 bram_map_rs.v
#define BRAMS_SIM_NEW_LIB_FILE2 TDP_RAM36K.v
#define FFS_MAP_FILE ffs_map.v
#define DFFRE_SIM_FILE DFFRE.v
#define DFFNRE_SIM_FILE DFFNRE.v
#define LUT1_SIM_FILE LUT1.v
#define LUT2_SIM_FILE LUT2.v
#define LUT3_SIM_FILE LUT3.v
#define LUT4_SIM_FILE LUT4.v
#define LUT5_SIM_FILE LUT5.v
#define LUT6_SIM_FILE LUT6.v
#define CLK_BUF_SIM_FILE CLK_BUF.v
#define I_BUF_SIM_FILE I_BUF.v
#define O_BUF_SIM_FILE O_BUF.v
#define LUT_FINAL_MAP_FILE lut_map.v
#define DSP_38_MAP_FILE dsp38_map.v
#define DSP_38_SIM_FILE DSP38.v
#define DSP19X2_MAP_FILE dsp19x2_map.v
#define DSP19X2_SIM_FILE DSP19X2.v
#define ARITH_MAP_FILE arith_map.v
#define DSP_MAP_FILE dsp_map.v
#define DSP_FINAL_MAP_FILE dsp_final_map.v
#define ALL_ARITH_MAP_FILE all_arith_map.v
#define BRAM_TXT brams.txt
#define BRAM_LIB brams_new.txt
#define BRAM_LIB_SWAP brams_new_swap.txt
#define BRAM_ASYNC_TXT brams_async.txt
#define BRAM_MAP_NEW_VERSION_FILE brams_map_new_version.v // New version techmap files for TDP36K
#define BRAM_FINAL_MAP_NEW_VERSION_FILE brams_final_map_new_version.v // New version techmap files for TDP36K
#define BRAM_MAP_FILE brams_map.v
#define BRAM_MAP_NEW_FILE brams_map_new.v
#define BRAM_FINAL_MAP_FILE brams_final_map.v
#define BRAM_FINAL_MAP_NEW_FILE brams_final_map_new.v
#define IO_cells_FILE io_cells_map1.v
#define IO_CELLs_final_map io_cell_final_map.v
#define GET_FILE_PATH(tech_dir,file) " +/rapidsilicon/" STR(tech_dir) "/" STR(file)
#define GET_FILE_PATH_RS_FPGA_SIM(tech_dir,file) " +/rapidsilicon/" STR(tech_dir) "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/" STR(file)
#define GET_TECHMAP_FILE_PATH(tech_dir,file) " +/rapidsilicon/" STR(tech_dir) "/" STR(file)
#define VERSION_MAJOR 0 // 0 - beta 
#define VERSION_MINOR 4
#define VERSION_PATCH 196

enum Technologies {
    GENERIC,   
    GENESIS,
    GENESIS_2,
    GENESIS_3
};

struct PowerExtractRapidSilicon : public ScriptPass {

    PowerExtractRapidSilicon() : ScriptPass(STR(PASS_NAME), "Power data extraction for RapidSilicon FPGAs") {}

    void help() override
    {
        log("\n");
        log("   %s [options]\n", STR(PASS_NAME));
        log("This command runs power extraction for RapidSilicon FPGAs\n");
        log("\n");
        log("    -verilog <file>\n");
        log("        Write the design to the specified verilog file. writing of an output file\n");
        log("        is omitted if this parameter is not specified.\n");
        log("\n");
        log("\n");
    }

    string module_name; 
    Technologies tech; 
    string sdc_str;
    string verilog_file;
    int lut_cnt;
    int dff_cnt;
    int latch_cnt;
    int ram_cnt;
    int dsp_cnt;
    int IO_cnt;
    int PLL_cnt;
    
    std::vector<Cell *> LUTs;
    std::vector<Cell *> DSP_38;
    std::vector<Cell *> TDP36K;
    std::vector<Cell *> DFFs;
    std::vector<Cell *> IBUFs;
    std::vector<Cell *> OBUFs;
    std::vector<Cell *> CLK_BUF;
    std::vector<RTLIL::SigSpec> _clocks_;
    std::map<SigSpec, SigSpec> clk_from_buffer;
    std::vector<std::tuple<int, string, string, string, string>> ios_out;

    dict<RTLIL::SigSpec, std::set<string>*> ios_clk;
    std::map<RTLIL::SigSpec,std::vector<float>>ce_ffs;
    std::map<string,std::vector<string>>lut_clk;    
    std::map<std::tuple<std::string, string, string, int, int, float, float, float, float>, int> tdp_out;
    std::map<std::tuple<IdString, string, int, int>, int> dsp_out;

    std::map<string, double> sdc_clks;
    std::map<string, double> clk_out; 
    dict<Cell*, std::set<RTLIL::SigSpec>*> cell2clkDomainsMerged;
    dict<RTLIL::SigSpec, std::set<Cell*>*> sig2CellsInFanout;
    dict<RTLIL::SigSpec, std::set<Cell*>*> sig2CellsInFanin;
    std::set<std::tuple<string, string, string, int>>IO_dict;
    dict <RTLIL::Cell* , std::set<RTLIL::SigSpec>*> LUTs_Cells;
    dict <RTLIL::Cell* , std::set<RTLIL::SigSpec>*> DSP_Cells;
    dict <RTLIL::Cell* , std::set<RTLIL::SigSpec>*> Carry_Cells;
    
    bool preserve_ip;
    
    RTLIL::Design *_design;

    void clear_flags() override
    {
        tech = Technologies::GENESIS_3;
        verilog_file = "";
        preserve_ip = false;
        lut_cnt = 0;
        dff_cnt = 0;
        latch_cnt = 0;
        ram_cnt = 0;
        dsp_cnt = 0;
        IO_cnt = 0;
        PLL_cnt = 0;
        module_name = "";
    }

    void execute(std::vector<std::string> args, RTLIL::Design *design) override
    {
#ifdef PRODUCTION_BUILD
        License_Manager license(License_Manager::LicensedProductName::YOSYS_RS_PLUGIN);
#endif
        string run_from; 
        string run_to;
        string tech_str;
        string goal_str;
        string encoding_str;
        string effort_str;
        string carry_str;
        clear_flags();
        _design = design;

        size_t argidx;
        for (argidx = 1; argidx < args.size(); argidx++) {
            if (args[argidx] == "-verilog" && argidx + 1 < args.size()) {
                verilog_file = args[++argidx];
                continue;
            }
            if (args[argidx] == "-tech" && argidx + 1 < args.size()) {
                tech_str = args[++argidx];
                continue;
            }
             if (args[argidx] == "-sdc" && argidx + 1 < args.size()) {
                sdc_str = args[++argidx];
                continue;
            }
            break;
        }
        extra_args(args, argidx, design);

        if (tech_str == "generic")
            tech = Technologies::GENERIC;
        else if (tech_str == "genesis"){
            tech = Technologies::GENESIS;
        }
        else if (tech_str == "genesis2") {
            tech = Technologies::GENESIS_2;
        }
        else if (tech_str == "genesis3") {
            tech = Technologies::GENESIS_3;
        }
        else if (tech_str != "")
            log_cmd_error("Invalid tech specified: '%s'\n", tech_str.c_str());

        if (!design->full_selection())
            log_cmd_error("This command only operates on fully selected designs!\n");

        log_header(design, "Executing power extraction pass: v%d.%d.%d\n", 
            VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
        log_push();

        run_script(design, run_from, run_to);

        log_pop();
    }


    std::string id(RTLIL::IdString internal_id)
    {
            const char *str = internal_id.c_str();
            return std::string(str);
    }

    void dump_const(const RTLIL::Const &data, int width = -1, int offset = 0, bool no_decimal = false, bool escape_comment = false)
    {
            bool set_signed = (data.flags & RTLIL::CONST_FLAG_SIGNED) != 0;
            if (width < 0)
                    width = data.bits.size() - offset;
            if (width == 0) {
                    log("{0{1'b0}}");
                    return;
            }
    #if 0
            if (nostr)
                    goto dump_hex;
    #endif
            if ((data.flags & RTLIL::CONST_FLAG_STRING) == 0 || width != (int)data.bits.size()) {
                    //if (width == 32 && !no_decimal && !nodec) {
                    if (width == 32 && !no_decimal && 1) {
                            int32_t val = 0;
                            for (int i = offset+width-1; i >= offset; i--) {
                                    log_assert(i < (int)data.bits.size());
                                    if (data.bits[i] != State::S0 && data.bits[i] != State::S1)
                                            goto dump_hex;
                                    if (data.bits[i] == State::S1)
                                            val |= 1 << (i - offset);
                            }
                            //if (decimal)
                            if (1)
                                    log("%d", val);
                            else if (set_signed && val < 0)
                                    log("-32'sd%u", -val);
                            else
                                    log("32'%sd%u", set_signed ? "s" : "", val);
                    } else {
            dump_hex:
                    //        if (nohex)
                            if (1)
                                    goto dump_bin;
                            vector<char> bin_digits, hex_digits;
                            for (int i = offset; i < offset+width; i++) {
                                    log_assert(i < (int)data.bits.size());
                                    switch (data.bits[i]) {
                                    case State::S0: bin_digits.push_back('0'); break;
                                    case State::S1: bin_digits.push_back('1'); break;
                                    case RTLIL::Sx: bin_digits.push_back('x'); break;
                                    case RTLIL::Sz: bin_digits.push_back('z'); break;
                                    case RTLIL::Sa: bin_digits.push_back('?'); break;
                                    case RTLIL::Sm: log_error("Found marker state in final netlist.");
                                    }
                            }
                            if (GetSize(bin_digits) == 0)
                                    goto dump_bin;
                            while (GetSize(bin_digits) % 4 != 0)
                                    if (bin_digits.back() == '1')
                                            bin_digits.push_back('0');
                                    else
                                            bin_digits.push_back(bin_digits.back());
                            for (int i = 0; i < GetSize(bin_digits); i += 4)
                            {
                                    char bit_3 = bin_digits[i+3];
                                    char bit_2 = bin_digits[i+2];
                                    char bit_1 = bin_digits[i+1];
                                    char bit_0 = bin_digits[i+0];
                                    if (bit_3 == 'x' || bit_2 == 'x' || bit_1 == 'x' || bit_0 == 'x') {
                                            if (bit_3 != 'x' || bit_2 != 'x' || bit_1 != 'x' || bit_0 != 'x')
                                                    goto dump_bin;
                                            hex_digits.push_back('x');
                                            continue;
                                    }
                                    if (bit_3 == 'z' || bit_2 == 'z' || bit_1 == 'z' || bit_0 == 'z') {
                                            if (bit_3 != 'z' || bit_2 != 'z' || bit_1 != 'z' || bit_0 != 'z')
                                                    goto dump_bin;
                                            hex_digits.push_back('z');
                                            continue;
                                    }
                                    if (bit_3 == '?' || bit_2 == '?' || bit_1 == '?' || bit_0 == '?') {
                                            if (bit_3 != '?' || bit_2 != '?' || bit_1 != '?' || bit_0 != '?')
                                                    goto dump_bin;
                                            hex_digits.push_back('?');
                                            continue;
                                    }
                                    int val = 8*(bit_3 - '0') + 4*(bit_2 - '0') + 2*(bit_1 - '0') + (bit_0 - '0');
                                    hex_digits.push_back(val < 10 ? '0' + val : 'a' + val - 10);
                            }
                            log("%d'%sh", width, set_signed ? "s" : "");
                            for (int i = GetSize(hex_digits)-1; i >= 0; i--)
                                    log(hex_digits[i]);
                    }
                    if (0) {
            dump_bin:
                            log("%d'%sb", width, set_signed ? "s" : "");
                            if (width == 0)
                                    log("0");
                            for (int i = offset+width-1; i >= offset; i--) {
                                    log_assert(i < (int)data.bits.size());
                                    switch (data.bits[i]) {
                                    case State::S0: log("0"); break;
                                    case State::S1: log("1"); break;
                                    case RTLIL::Sx: log("x"); break;
                                    case RTLIL::Sz: log("z"); break;
                                    case RTLIL::Sa: log("?"); break;
                                    case RTLIL::Sm: log_error("Found marker state in final netlist.");
                                    }
                            }
                    }
            } else {
                    if ((data.flags & RTLIL::CONST_FLAG_REAL) == 0)
                            log("\"");
                    std::string str = data.decode_string();
                    for (size_t i = 0; i < str.size(); i++) {
                            if (str[i] == '\n')
                                    log("\\n");
                            else if (str[i] == '\t')
                                    log("\\t");
                            else if (str[i] < 32)
                                    log("\\%03o", str[i]);
                            else if (str[i] == '"')
                                    log("\\\"");
                            else if (str[i] == '\\')
                                    log("\\\\");
                            else if (str[i] == '/' && escape_comment && i > 0 && str[i-1] == '*')
                                    log("\\/");
                            else
                                    log(str[i]);
                    }
                    if ((data.flags & RTLIL::CONST_FLAG_REAL) == 0)
                            log("\"");
            }
    }

    void dump_sigchunk(const RTLIL::SigChunk &chunk, bool no_decimal = false)
    {
        if (chunk.wire == NULL) {
            dump_const(chunk.data, chunk.width, chunk.offset, no_decimal);
            return;
        }

        if (chunk.width == chunk.wire->width && chunk.offset == 0) {

            log("%s", id(chunk.wire->name).c_str());

        } else if (chunk.width == 1) {

            if (chunk.wire->upto)
                log("%s[%d]", id(chunk.wire->name).c_str(), 
                    (chunk.wire->width - chunk.offset - 1) + chunk.wire->start_offset);
            else
                log("%s[%d]", id(chunk.wire->name).c_str(), chunk.offset + chunk.wire->start_offset);

        } else {

            if (chunk.wire->upto)
                log("%s[%d:%d]", id(chunk.wire->name).c_str(),
                    (chunk.wire->width - (chunk.offset + chunk.width - 1) - 1) + chunk.wire->start_offset,
                    (chunk.wire->width - chunk.offset - 1) + chunk.wire->start_offset);
            else
                log("%s[%d:%d]", id(chunk.wire->name).c_str(),
                    (chunk.offset + chunk.width - 1) + chunk.wire->start_offset,
                    chunk.offset + chunk.wire->start_offset);
        }
    }

    void show_sig(const RTLIL::SigSpec &sig)
    {
            if (GetSize(sig) == 0) {
            log("{0{1'b0}}");
            return;
            }

            if (sig.is_chunk()) {

                dump_sigchunk(sig.as_chunk());

            } else {

                log("{ ");

                for (auto it = sig.chunks().rbegin(); it != sig.chunks().rend(); ++it) {

                    if (it != sig.chunks().rbegin())
                        log(", ");

                    dump_sigchunk(*it, true);
                }
                log(" }");
            }
    }


    void sig2cells (dict<RTLIL::SigSpec, std::set<Cell*>*>& sig2CellsInFanout,
                    dict<RTLIL::SigSpec, std::set<Cell*>*>& sig2CellsInFanin) {

        int nbCells = 0;
        int nbInputs = 0;
        int nbOutputs = 0;

        log("Building Sig2cells ... ");

        auto startTime = std::chrono::high_resolution_clock::now();

        for (auto cell : _design->top_module()->cells()) {

            nbCells++;

            for (auto &conn : cell->connections()) {

                IdString portName = conn.first;
                RTLIL::SigSpec actual = conn.second;
                std::set<Cell*>* newSet;

                // For each input port of the cell stores its corresponding actual sig net 
                // that drives this cell through this port.
                // So for each sig net we have the corresponding set of cells it is driving.
                //
                if (cell->input(portName)) {

                    nbInputs++;

                    if (!actual.is_chunk()) {

                    for (auto it = actual.chunks().rbegin(); 
                            it != actual.chunks().rend(); ++it) {

                        RTLIL::SigSpec sub_actual = *it;

                        if (sig2CellsInFanout.count(sub_actual)) {

                            newSet = sig2CellsInFanout[sub_actual];

                        } else {

                            newSet = new std::set<Cell*>;
                            sig2CellsInFanout[sub_actual] = newSet;
                        }

                        newSet->insert(cell);

                    } // end for 

                    } else {

    #if 0
                    if (1 || (cell->name == "_091_")) {
                    log("Processing input %s of cell %s\n", portName.c_str(), 
                        (cell->name).c_str());
                    show_sig(actual);
                    log("\n");
                    }
    #endif

                    if (sig2CellsInFanout.count(actual)) {

                        newSet = sig2CellsInFanout[actual];

                    } else {

                        newSet = new std::set<Cell*>;
                        sig2CellsInFanout[actual] = newSet;
                    }

                    newSet->insert(cell);
                    }

                } else { // Cell output port case
                        // For each output port of the cell stores its corresponding actual sig net that is
                        // driven by this cell through this output port.
                        // So for each sig net we have the corresponding set of cells that drives it.
                        // Theoritically this set has only 1 Cell. If it has multiple cells then this means this
                        // sig net is multiply driven (which is not good !).

                    nbOutputs++;

                    //log("Processing output %s of cell %s\n", portName.c_str(), (cell->name).c_str());

                    if (sig2CellsInFanin.count(actual)) { // should not happen ! Multiply driven case

                    newSet = sig2CellsInFanin[actual];

                    } else {

                    newSet = new std::set<Cell*>;
                    sig2CellsInFanin[actual] = newSet;
                    }

                    newSet->insert(cell);
                }
            }
        }       

        #if 0
            log("Processed %d cells\n", nbCells);
            log("Processed %d inputs\n", nbInputs);
            log("Processed %d outputs\n", nbOutputs);
        #endif

        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        float totalTime = elapsed.count() * 1e-9;

        log(" [%.2f sec.]\n", totalTime);
    }


    void print_sig2cells(dict<RTLIL::SigSpec, std::set<Cell*>*> sig2CellsInFanout)
    {
        for (auto &s2c : sig2CellsInFanout) {

            RTLIL::SigSpec sig = s2c.first;

            show_sig(sig);
            log(" drives ");

            std::set<Cell*>* set_cells = s2c.second;

            for (std::set<Cell*>::iterator it = set_cells->begin(); 
                it != set_cells->end(); it++) {

            Cell* cell = *it;
            log("%s ", (cell->name).c_str());
            }
            log("\n\n");
        }
    }

    int sigIsConstant(RTLIL::SigSpec sig) 
    {
    if (sig.is_chunk()) {
        if ((sig.as_chunk()).wire == NULL) {
        return 1;
        }
    }

    return 0;
    }

    // Return 1 if the name "pName" corresponds to a PORT name of the 'cell'
    // and then store in 'actual' the corresponding actual sigSpec.
    // Return 0 if the name "pName" does not correspond to any PORT name in the 
    // 'cell'. In this case 'actual' is garbage and should not be considered.
    //
    int getPortActual(IdString pName, Cell* cell, RTLIL::SigSpec& actual)
    {
    for (auto &conn : cell->connections()) {

        IdString portName = conn.first;
        actual = conn.second;

        if (pName == portName) {
            return 1;
        }
    }

    return 0;
    }

    int getPortActual_noref(IdString pName, Cell* cell)
    {
    for (auto &conn : cell->connections()) {

        IdString portName = conn.first;

        if (pName == portName) {
            return 1;
        }
    }

    return 0;
    }

    bool sequentialCell(Cell* cell) 
    {
        if (cell->type == RTLIL::escape_id("DFFRE")) {
        return true;
        }

        if (cell->type == RTLIL::escape_id("TDP_RAM36K")) {
        return true;
        }

        if (cell->type == RTLIL::escape_id("TDP_RAM18KX2")) {
        return true;
        }

        if (cell->type == RTLIL::escape_id("DSP38")) {
        // Need to check if there is a CLOCK pin defined or not in this 
        // DSP cell.
        // If there is a clock pin then the DSP is a sequential cell
        // otherwise it is not.
        //
        RTLIL::SigSpec clock;

        int found = getPortActual(RTLIL::escape_id("CLK"), cell, clock);

        return found;
        }

        return false;
    }

    void collectForwardRec(RTLIL::SigSpec& sig, 
                        RTLIL::SigSpec& clock, 
                        dict<RTLIL::SigSpec, std::set<Cell*>*>& sig2CellsInFanout,
                        dict<RTLIL::SigSpec, std::set<RTLIL::SigSpec>*>& rhsSig2LhsSig,
                        dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomains){

    if (sigIsConstant(sig)) {
        return;
    }

        #if 0
            log("Forward Visit ");
            show_sig(sig);
            log("\n");
        #endif

    if (sig2CellsInFanout.count(sig)) {

        // Get the cells driven by 'sig'
        //
        std::set<Cell*>* sigFanout = sig2CellsInFanout[sig];

        std::vector<Cell*> cells2recurse;

        // For all the cells driven by 'sig' 
        //
        for (std::set<Cell*>::iterator it = sigFanout->begin(); it != sigFanout->end(); it++) {

            Cell* cell = *it;

            // We stop recursion as soon as we hit a sequential cell
            //
            if (sequentialCell(cell)) {
            continue;
            }

    #if 0
            log("Forward through cell <%s> with clk = ", (cell->name).c_str());
            show_sig(clock);
            log("\n\n");

            getchar();
    #endif

            std::set<RTLIL::SigSpec>* clkDomains;

            if (cell2clkDomains.count(cell)) { // we already went through this cell

            clkDomains = cell2clkDomains[cell];
            
            if (clkDomains->count(clock) == 0) { // if we did not go through this cell with this 'clock'
                                                // then this cell is a recurse forward candidate.
                cells2recurse.push_back(cell);
            }

            } else { // did not go through this cell yet

            clkDomains = new std::set<RTLIL::SigSpec>;

            cells2recurse.push_back(cell); // 'cell' is a recurse forward candidate since this is the
                                            // first time we process it.

            cell2clkDomains[cell] = clkDomains;
            }

            clkDomains->insert(clock);
        }
        
        // Now recurse forward on the previous selected cells through their outputs. 
        //
        for (std::vector<Cell*>::iterator it = cells2recurse.begin(); it != cells2recurse.end(); it++) {

            Cell* cell = *it;

            for (auto &conn : cell->connections()) {

                IdString portName = conn.first;
                RTLIL::SigSpec actual = conn.second;
            
                if (cell->output(portName)) {
                    collectForwardRec(actual, clock, sig2CellsInFanout, rhsSig2LhsSig, 
                                    cell2clkDomains); 
                }
            }
        }
    }

    // Handle the simple assignments case : assign lhs1 = sig; / assign lhs2 = sig; ...
    //
    // Propagate forward on all the signals in the fanout of 'sig'
    //
    if (rhsSig2LhsSig.count(sig)) { // does 'sig' get any fanout ?

        // Get the set of sig fanout
        //
        std::set<RTLIL::SigSpec>* sigFanout = rhsSig2LhsSig[sig];

        // Propagate forward all over the signals in 'sigFanout'.
        //
        for (std::set<RTLIL::SigSpec>::iterator it = sigFanout->begin(); it != sigFanout->end(); it++) {

            RTLIL::SigSpec lhs = *it;

            collectForwardRec(lhs, clock, sig2CellsInFanout, rhsSig2LhsSig, cell2clkDomains);
        }
    }
    }

    void collectBackwardRec(RTLIL::SigSpec& sig, 
                            RTLIL::SigSpec& clock, 
                            dict<RTLIL::SigSpec, std::set<Cell*>*>& sig2CellsInFanin,
                            dict<RTLIL::SigSpec, RTLIL::SigSpec>& lhsSig2RhsSig,
                            dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomains)
    {

    if (sigIsConstant(sig)) {
        return;
    }

    #if 0
    log("Backward Visit ");
    show_sig(sig);
    log("\n");
    #endif

    // If some cells are driving this 'sig'
    //
    if (sig2CellsInFanin.count(sig)) {

        // Get the cells that are driving this 'sig'
        //
        std::set<Cell*>* sigFanin = sig2CellsInFanin[sig];

        std::vector<Cell*> cells2recurse;

        // For all the cells driving this 'sig' process them.
        //
        // **WARNING ** : theoritically we should have only 1 driver cell, not more !
        // otherwise this "sig" is multi-driven
        //
        for (std::set<Cell*>::iterator it = sigFanin->begin(); it != sigFanin->end(); it++) {

            Cell* cell = *it;

            RTLIL::Wire *wire = sig[0].wire;
            std::string sig_name = wire->name.str();

            // We stop recursion as soon as we hit a sequential cell
            //
            if (sequentialCell(cell)) {
            continue;
            }

    #if 0
            log("Backward through cell <%s> with clk = ", (cell->name).c_str());
            show_sig(clock);
            log("\n\n");

            getchar();
    #endif

            std::set<RTLIL::SigSpec>* clkDomains;

            if (cell2clkDomains.count(cell)) { // we already went through this cell

            clkDomains = cell2clkDomains[cell];
            
            if (clkDomains->count(clock) == 0) { // if we did not go through this cell with this 'clock'
                                                // then this cell is a recurse backward candidate.
                cells2recurse.push_back(cell);
            }

            } else { // case of very first visit of this cell.

            clkDomains = new std::set<RTLIL::SigSpec>;

            cells2recurse.push_back(cell); // 'cell' is a recurse backward candidate since this is the
                                            // first time we process it.

            cell2clkDomains[cell] = clkDomains;
            }

            clkDomains->insert(clock);
        }
        

        // Now recurse backward on the previous selected cells through their inputs. 
        //
        for (std::vector<Cell*>::iterator it = cells2recurse.begin(); it != cells2recurse.end(); it++) {

            Cell* cell = *it;

            for (auto &conn : cell->connections()) {

                IdString portName = conn.first;
                RTLIL::SigSpec actual = conn.second;
            
                if (cell->input(portName)) {

                if (!actual.is_chunk()) { // if the 'actual' is a concat of sub actuals then
                                            // propagate backward on each sub actual.

                    for (auto it = actual.chunks().rbegin(); 
                        it != actual.chunks().rend(); ++it) {

                        RTLIL::SigSpec sub_actual = *it;

                        collectBackwardRec(sub_actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                            cell2clkDomains); 
                    }

                } else { // otherwise propagate backward on the single actual

                        collectBackwardRec(actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                            cell2clkDomains); 
                }
                }
            }
        } // end of backward recursion on selected cells

    }

    // Handle the simple assignment case : assign sig = rhs;
    //
    if (lhsSig2RhsSig.count(sig)) {

    #if 0
        log("backward through assign\n");
        getchar();
    #endif

        RTLIL::SigSpec rhs = lhsSig2RhsSig[sig];

        collectBackwardRec(rhs, clock, sig2CellsInFanin, lhsSig2RhsSig, cell2clkDomains);
    }
    }

    // Process the internal assignment of the form : assign lhs = rhs;
    //
    // Handle case where LHS is complex and is a concat of several signals like
    //
    // assign {lhsSig1, lhsSig2, lhsSig3, lhsSig4} = {rhsSig1, rhsSig2, rhsSig3, rhsSig4}.
    //
    // We will translate it into :
    //    assign lhsSig1 = rhsSig1;
    //    assign lhsSig2 = rhsSig2;
    //    assign lhsSig3 = rhsSig3;
    //    assign lhsSig4 = rhsSig4;
    //
    // Handle also case where we have constants in the RHS like :
    //
    // assign {lhsSig1, lhsSig2, lhsSig3, lhsSig4} = {2'b00, rhsSig3, rhsSig4}.
    //
    // We need to look at the constant width to make sure that there is a one to one
    // correspondance between the lhsSig_i and rhsSig_i (called sub actuals).
    // In this case we will generate only assignments for "lhsSig3", "lhsSig4" like : 
    //
    //    assign lhsSig3 = rhsSig3;
    //    assign lhsSig4 = rhsSig4;
    //
    // The RHS constant assignments are not usefull for traversals.
    //
    void sig2sig(dict<RTLIL::SigSpec, std::set<RTLIL::SigSpec>*>& rhsSig2LhsSig,
                dict<RTLIL::SigSpec, RTLIL::SigSpec>& lhsSig2rhsSig)
    {
        log("Building Sig2sig ...   ");

        auto startTime = std::chrono::high_resolution_clock::now();

        for (auto it = _design->top_module()->connections().begin(); 
            it != _design->top_module()->connections().end(); ++it) {

            RTLIL::SigSpec lhs = it->first;
            RTLIL::SigSpec rhs = it->second;

            if (sigIsConstant(rhs)) {
            continue;
            }

    #if 0
            log("ASSIGN ");
            show_sig(lhs);
            log(" = ");
            show_sig(rhs);
            log("\n\n");
    #endif

            if (!lhs.is_chunk()) { // if the assign is an assignment of bus/slice

            int misMatchCase = 0;

            // This case needs to be handled. It is generally when we have a constant
            // on several bits on RHS. If the constant is for instance 6'b000000 it counts 
            // for 1 (element) instead of 6 (bits). So we would need to split the constant
            // as well.
            //
            if (lhs.chunks().size() != rhs.chunks().size()) {

                misMatchCase = 1;

                auto rit = rhs.chunks().rbegin();
        
                long unsigned rhsSize = 0;

                // Check if size mismatch is due to constant slices on the RHS
                //
                while (rit != rhs.chunks().rend()) {

                    RTLIL::SigSpec sub_rhs = *rit;

                    if (sigIsConstant(sub_rhs)) {

                    rhsSize += (sub_rhs.as_chunk()).width;

                    } else {

                    rhsSize++;
                    }

                    rit++;
                }

                if (lhs.chunks().size() != rhsSize) {

                log("WARNING: Ignoring Assignment with LHS and RHS of different sizes (%ld vs %ld)\n",
                    lhs.chunks().size(), rhsSize);

                log("         assign ");
                show_sig(lhs);
                log(" = ");
                show_sig(rhs);
                log("\n");

                // There is a true mismatch : we would need to investigate this case if this happens
                continue;
                }

    #if 0
                log("         assign ");
                show_sig(lhs);
                log(" = ");
                show_sig(rhs);
                log("\n");

                log("Computed LHS and RHS exact sizes : %ld vs %ld\n", lhs.chunks().size(), rhsSize);
                getchar();
    #endif
            }

            // Lhs and Rhs have exactly the same size !
            //
            auto lit = lhs.chunks().rbegin();
            auto rit = rhs.chunks().rbegin();
        
            // RHS should have always less (misMatchCase) or equal sub actuals than LHS so 
            // we check iteration exit on RHS.
            //
            while (rit != rhs.chunks().rend()) {

                RTLIL::SigSpec sub_lhs = *lit;
                RTLIL::SigSpec sub_rhs = *rit;

                std::set<RTLIL::SigSpec>* fanout;

                // If the 'sub_rhs" is a constant pass over it and iterate on the 'lhs' 
                // the "constant width" number of time.
                //
                if (sigIsConstant(sub_rhs)) {

                    int constSize = (sub_rhs.as_chunk()).width;

                    while (constSize--) {
                        lit++;
                    }
                    rit++;

                    continue;
                }

                if (rhsSig2LhsSig.count(sub_rhs) == 0) {

                    fanout = new std::set<RTLIL::SigSpec>;
                    rhsSig2LhsSig[sub_rhs] = fanout;
                }

                fanout = rhsSig2LhsSig[sub_rhs];
            
                fanout->insert(sub_lhs); // add 'sub_lhs' in the fanout of 'sub_rhs'.

                lhsSig2rhsSig[sub_lhs] = sub_rhs; // 'sub_lhs" has only one 'sub_rhs'

                if (misMatchCase) {
    #if 0
                    log("SUB_ASSIGN ");
                    show_sig(sub_lhs);
                    log(" = ");
                    show_sig(sub_rhs);
                    log("\n\n");
                    getchar();
    #endif
                }

                lit++;
                rit++;
            }

            } else { 

            std::set<RTLIL::SigSpec>* fanout;

            if (rhsSig2LhsSig.count(rhs) == 0) {

                fanout = new std::set<RTLIL::SigSpec>;
                rhsSig2LhsSig[rhs] = fanout;
            }

            fanout = rhsSig2LhsSig[rhs];
            
            fanout->insert(lhs); // add 'lhs' in the fanout of 'rhs'.
                
            lhsSig2rhsSig[lhs] = rhs; // 'lhs" has only one 'rhs'
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        float totalTime = elapsed.count() * 1e-9;

        log(" [%.2f sec.]\n", totalTime);
    }

    void collectForwardStart (Cell* cell, RTLIL::SigSpec &clock, 
                            dict<RTLIL::SigSpec, std::set<Cell*>*>& sig2CellsInFanout,
                            dict<RTLIL::SigSpec, std::set<RTLIL::SigSpec>*>& rhsSig2LhsSig,
                            dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomains)
    {

    // Loop over the cell outputs and propagate forward
    //
    for (auto &conn : cell->connections()) {

        IdString portName = conn.first;
        RTLIL::SigSpec actual = conn.second;
        // log("Port = %s output = %d\n",log_id(portName), cell->output(conn.first));
        if (cell->output(portName)) {

    #if 0
            log("Launch collect forward on %s %s, out = %s\n", 
                (cell->type).c_str(), (cell->name).c_str(), portName.c_str());
    #endif

            collectForwardRec(actual, clock, sig2CellsInFanout, rhsSig2LhsSig, cell2clkDomains); 
        }
    }
    }

    // Build the 'cell2clkDomains' using the dictionaries 'sig2CellsInFanout' and
    // 'rhsSig2LhsSig'.
    // The build is done through a forward traversal starting from sequential cells
    // and propagating recursively forward through their outputs.
    //
    void collectForward(dict<RTLIL::SigSpec, std::set<Cell*>*>& sig2CellsInFanout,
                        dict<RTLIL::SigSpec, std::set<RTLIL::SigSpec>*>& rhsSig2LhsSig,
                        dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomains)
    {
        log("Forward traversal ...  ");

        auto startTime = std::chrono::high_resolution_clock::now();

        // recursive forward propagation of clocks attached to each sequential cells
        //
        for (auto cell : _design->top_module()->cells()) {

            if (cell->type == RTLIL::escape_id("DFFRE")) {

            RTLIL::SigSpec clock = cell->getPort(ID::C);
                // log("Cell = DFFRE\n");
            collectForwardStart(cell, clock, sig2CellsInFanout, rhsSig2LhsSig, cell2clkDomains);

            continue;
            }

            if (cell->type == RTLIL::escape_id("TDP_RAM36K")) {
            
            RTLIL::SigSpec clock;
            int found = getPortActual(RTLIL::escape_id("CLK_A"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                collectForwardStart(cell, clock, sig2CellsInFanout, rhsSig2LhsSig, 
                                    cell2clkDomains);
            }

            found = getPortActual(RTLIL::escape_id("CLK_B"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                collectForwardStart(cell, clock, sig2CellsInFanout, rhsSig2LhsSig, 
                                    cell2clkDomains);
            }

            continue;
            }

            if (cell->type == RTLIL::escape_id("TDP_RAM18KX2")) {
            
            RTLIL::SigSpec clock;
            int found = getPortActual(RTLIL::escape_id("CLK_A1"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                collectForwardStart(cell, clock, sig2CellsInFanout, rhsSig2LhsSig, 
                                    cell2clkDomains);
            }

            found = getPortActual(RTLIL::escape_id("CLK_A2"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                collectForwardStart(cell, clock, sig2CellsInFanout, rhsSig2LhsSig, 
                                    cell2clkDomains);
            }

            found = getPortActual(RTLIL::escape_id("CLK_B1"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                collectForwardStart(cell, clock, sig2CellsInFanout, rhsSig2LhsSig, 
                                    cell2clkDomains);
            }

            found = getPortActual(RTLIL::escape_id("CLK_B2"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                collectForwardStart(cell, clock, sig2CellsInFanout, rhsSig2LhsSig, 
                                    cell2clkDomains);
            }

            continue;
            }

            if (cell->type == RTLIL::escape_id("DSP38")) {
            
            RTLIL::SigSpec clock;
            int found = getPortActual(RTLIL::escape_id("CLK"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                collectForwardStart(cell, clock, sig2CellsInFanout, rhsSig2LhsSig, 
                                    cell2clkDomains);
            }

            continue;
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        float totalTime = elapsed.count() * 1e-9;

        log(" [%.2f sec.]\n", totalTime);
    }

    // Build the 'cell2clkDomains' using the dictionaries 'sig2CellsInFanin' and
    // 'lhsSig2RhsSig'.
    // The build is done through a backward traversal starting from sequential cells
    // and propagating recursively backward through their inputs.
    //

    void collectBackward(dict<RTLIL::SigSpec, std::set<Cell*>*>& sig2CellsInFanin,
                        dict<RTLIL::SigSpec, RTLIL::SigSpec>& lhsSig2RhsSig,
                        dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomains)
    {
        log("Backward traversal ... ");

        auto startTime = std::chrono::high_resolution_clock::now();

        // recursive backward propagation of clocks attached to each sequential cells
        //
        for (auto cell : _design->top_module()->cells()) {

            // 1. DFFRE
            //
            if (cell->type == RTLIL::escape_id("DFFRE")) {

            // Assume clock is not constant/empty !
            //
            RTLIL::SigSpec clock = cell->getPort(ID::C);

            // Go backward through the inputs of the DFFRE
            //
            for (auto &conn : cell->connections()) {

                IdString portName = conn.first;

                // Do not backward of CLOCK pin !
                //
                if (portName == RTLIL::escape_id("C")) {
                    continue;
                }

                RTLIL::SigSpec actual = conn.second;
            
                if (cell->input(portName)) {

            #if 0
                            log("Launch collect backward on %s %s, out = %s\n", 
                                    (cell->type).c_str(), (cell->name).c_str(), portName.c_str());
            #endif

                    collectBackwardRec(actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                        cell2clkDomains); 
                }
            }

            continue;

        } // End of DFFRE processing


        // 2. TDP_RAM36K
        //
        if (cell->type == RTLIL::escape_id("TDP_RAM36K")) {
            
            RTLIL::SigSpec clock;

            int found = getPortActual(RTLIL::escape_id("CLK_A"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                //
                // Go backward through the inputs of the TDP_RAM36K
                //
                for (auto &conn : cell->connections()) {

                    IdString portName = conn.first;

                    // Do not backward of CLOCK pins !
                    //
                    if (portName == RTLIL::escape_id("CLK_A")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B")) {
                    continue;
                    }

                    RTLIL::SigSpec actual = conn.second;
            
                    if (cell->input(portName)) {

                    collectBackwardRec(actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                        cell2clkDomains); 
                    }
                }
            }

            found = getPortActual(RTLIL::escape_id("CLK_B"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                //
                // Go backward through the inputs of the TDP_RAM36K
                //
                for (auto &conn : cell->connections()) {

                    IdString portName = conn.first;

                    // Do not backward of CLOCK pins !
                    //
                    if (portName == RTLIL::escape_id("CLK_A")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B")) {
                    continue;
                    }

                    RTLIL::SigSpec actual = conn.second;
            
                    if (cell->input(portName)) {

                    collectBackwardRec(actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                        cell2clkDomains); 
                    }
                }
            }

            continue;

        } // End of processing TDP_RAM36K


        // 3. TDP_RAM18KX2
        //
        if (cell->type == RTLIL::escape_id("TDP_RAM18KX2")) {
            
            RTLIL::SigSpec clock;

            int found = getPortActual(RTLIL::escape_id("CLK_A1"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                //
                // Go backward through the inputs of the TDP_RAM18KX2
                //
                for (auto &conn : cell->connections()) {

                    IdString portName = conn.first;

                    // Do not backward of CLOCK pins !
                    //
                    if (portName == RTLIL::escape_id("CLK_A1")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_A2")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B1")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B2")) {
                    continue;
                    }

                    RTLIL::SigSpec actual = conn.second;
            
                    if (cell->input(portName)) {

                    collectBackwardRec(actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                        cell2clkDomains); 
                    }
                }
            }

            found = getPortActual(RTLIL::escape_id("CLK_A2"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                //
                // Go backward through the inputs of the TDP_RAM18KX2
                //
                for (auto &conn : cell->connections()) {

                    IdString portName = conn.first;

                    // Do not backward of CLOCK pins !
                    //
                    if (portName == RTLIL::escape_id("CLK_A1")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_A2")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B1")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B2")) {
                    continue;
                    }

                    RTLIL::SigSpec actual = conn.second;
            
                    if (cell->input(portName)) {

                    collectBackwardRec(actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                        cell2clkDomains); 
                    }
                }
            }

            found = getPortActual(RTLIL::escape_id("CLK_B1"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                //
                // Go backward through the inputs of the TDP_RAM18KX2
                //
                for (auto &conn : cell->connections()) {

                    IdString portName = conn.first;

                    // Do not backward of CLOCK pins !
                    //
                    if (portName == RTLIL::escape_id("CLK_A1")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_A2")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B1")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B2")) {
                    continue;
                    }

                    RTLIL::SigSpec actual = conn.second;
            
                    if (cell->input(portName)) {

                    collectBackwardRec(actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                        cell2clkDomains); 
                    }
                }
            }

            found = getPortActual(RTLIL::escape_id("CLK_B2"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                //
                // Go backward through the inputs of the TDP_RAM18KX2
                //
                for (auto &conn : cell->connections()) {

                    IdString portName = conn.first;

                    // Do not backward of CLOCK pins !
                    //
                    if (portName == RTLIL::escape_id("CLK_A1")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_A2")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B1")) {
                    continue;
                    }
                    if (portName == RTLIL::escape_id("CLK_B2")) {
                    continue;
                    }

                    RTLIL::SigSpec actual = conn.second;
            
                    if (cell->input(portName)) {

                    collectBackwardRec(actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                        cell2clkDomains); 
                    }
                }
            }

            continue;

        } // End of processing TDP_RAM18KX2


        // 4. DSP38
        //
        if (cell->type == RTLIL::escape_id("DSP38")) {
            
            RTLIL::SigSpec clock;

            int found = getPortActual(RTLIL::escape_id("CLK"), cell, clock);

            if (found && !sigIsConstant(clock)) {
                //
                // Go backward through the inputs of the DSP38
                //
                for (auto &conn : cell->connections()) {

                    IdString portName = conn.first;

                    // Do not backward of CLOCK pin !
                    //
                    if (portName == RTLIL::escape_id("CLK")) {
                    continue;
                    }

                    RTLIL::SigSpec actual = conn.second;
            
                    if (cell->input(portName)) {

                    collectBackwardRec(actual, clock, sig2CellsInFanin, lhsSig2RhsSig, 
                                        cell2clkDomains); 
                    }
                }
            }

            continue;

        } // End of processing DSP38

        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        float totalTime = elapsed.count() * 1e-9;

        log(" [%.2f sec.]\n", totalTime);
    }

    // Build 'cell2clkDomainsMerged' which is the merge of 'cell2clkDomains1' and
    // 'cell2clkDomains2'.
    //
    void mergeClockDomains(dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomains1,
                        dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomains2,
                        dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomainsMerged)
    {

    log("Merging clk domains ... ");

    auto startTime = std::chrono::high_resolution_clock::now();

    for (auto &cell_clkDomain : cell2clkDomains1) {

        Cell* cell = cell_clkDomain.first;

        std::set<RTLIL::SigSpec>* clkDomain = cell_clkDomain.second;

        std::set<RTLIL::SigSpec>* clkDomainsMerged;

        if (cell2clkDomainsMerged.count(cell)) {

            clkDomainsMerged = cell2clkDomainsMerged[cell];
            
        } else {

            clkDomainsMerged = new std::set<RTLIL::SigSpec>;

            cell2clkDomainsMerged[cell] = clkDomainsMerged;
        }

        for (std::set<RTLIL::SigSpec>::iterator it = clkDomain->begin(); 
                it != clkDomain->end(); it++) {

            RTLIL::SigSpec clock = *it;
            clkDomainsMerged->insert(clock);
        }
    }

    for (auto &cell_clkDomain : cell2clkDomains2) {

        Cell* cell = cell_clkDomain.first;

        std::set<RTLIL::SigSpec>* clkDomain = cell_clkDomain.second;

        std::set<RTLIL::SigSpec>* clkDomainsMerged;

        if (cell2clkDomainsMerged.count(cell)) {

            clkDomainsMerged = cell2clkDomainsMerged[cell];
            
        } else {

            clkDomainsMerged = new std::set<RTLIL::SigSpec>;

            cell2clkDomainsMerged[cell] = clkDomainsMerged;
        }

        for (std::set<RTLIL::SigSpec>::iterator it = clkDomain->begin(); 
                it != clkDomain->end(); it++) {

            RTLIL::SigSpec clock = *it;
            clkDomainsMerged->insert(clock);
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

    float totalTime = elapsed.count() * 1e-9;

    log("[%.2f sec.]\n\n", totalTime);
    }

    // Collect the cells with no associated clocks, e;G the ones between no sequential
    // cells (typically the ones between primary inputs and primary outputs).
    void collectNotTraversedCells(dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomains)
    {

    for (auto cell : _design->top_module()->cells()) {

        if (sequentialCell(cell)) {
        continue;
        } 

        // If cell not traversed, e.g is not associated to any clock then add it 
        // in the 'cell2clkDomains' dictionary with an empty Clk domain set.
        //
        if (cell2clkDomains.count(cell) == 0) {

            std::set<RTLIL::SigSpec>* noClocks = new std::set<RTLIL::SigSpec>;
            cell2clkDomains[cell] = noClocks; // empty set
        }
    }
    }

    void reportCellsClockDomains(dict<Cell*, std::set<RTLIL::SigSpec>*>& cell2clkDomains)
    {
    dict<RTLIL::SigSpec, std::set<Cell*>*> clk2Cells;
    std::set<RTLIL::SigSpec> clocks;

    int nbCells = 0;
    int nbMultiClockCells = 0;

    for (auto &cell_clkDomain : cell2clkDomains) {

        std::set<RTLIL::SigSpec>* clkDomain = cell_clkDomain.second;

        nbCells++;

        Cell* cell = cell_clkDomain.first;
        if ((cell->type == RTLIL::escape_id("DSP38") \
                || cell->type == RTLIL::escape_id("DSP19x2"))){
                if(getPortActual_noref(RTLIL::escape_id("CLK"), cell) == 0)
                    DSP_Cells[cell] = cell_clkDomain.second;
        }
        if ((cell->type == RTLIL::escape_id("LUT1") \
            || cell->type == RTLIL::escape_id("LUT2") \
            || cell->type == RTLIL::escape_id("LUT3") \
            || cell->type == RTLIL::escape_id("LUT4") \
            || cell->type == RTLIL::escape_id("LUT5") \
            || cell->type == RTLIL::escape_id("LUT6"))){
            
            LUTs_Cells[cell] = cell_clkDomain.second;
        }

    #if 0

        log("Cell = %s <%s>\n", (cell->type).c_str(), (cell->name).c_str());

        log("    -> Clock Domain : ");
    #endif

        for (std::set<RTLIL::SigSpec>::iterator it = clkDomain->begin(); it != clkDomain->end(); it++) {

            RTLIL::SigSpec clk = *it;

            clocks.insert(clk);

            std::set<Cell*>* clkDmnCells;

            if (clk2Cells.count(clk) == 0) {
                clkDmnCells = new std::set<Cell*>;
                clk2Cells[clk] = clkDmnCells;
            }

            clkDmnCells = clk2Cells[clk];
            clkDmnCells->insert(cell);
        }

        // Does the cell associated with several clocks ?
        //
        if (clkDomain->size() > 1) {
            nbMultiClockCells++;
        }

    #if 0
        log("\n");
    #endif
    }

    log(" -------------------------------------------------\n");
    log("  Number of cells processed    : %d\n", nbCells);
    log("  Number of Multi Clocks cells : %d\n", nbMultiClockCells++);
    log("  Extracted clocks[Nb cells]   : ");

    // List all the extracted clocks 
    //
    for (std::set<RTLIL::SigSpec>::iterator it = clocks.begin(); it != clocks.end(); it++) {

        RTLIL::SigSpec clk = *it;
        show_sig(clk);
        std::set<Cell*>* clkDmnCells = clk2Cells[clk];
        log("[%ld] ", clkDmnCells->size());
    }
    log("\n");

    int nbCellsWithNoClock = 0;

    //log("  List of cells with no clock  :\n");

    // A not traversed cell is either:
    //    1. A cell which is not in the 'cell2clkDomains' dictionary OR
    //    2. A cell with an associated empty set of clocks
    //
    for (auto cell : _design->top_module()->cells()) {

        if (sequentialCell(cell)) {
        continue;
        } 

        if (cell2clkDomains.count(cell) == 0) {

        //log("    NOT FOUND:    %s <%s>\n", (cell->type).c_str(), (cell->name).c_str());
        nbCellsWithNoClock++;

        } else {

        std::set<RTLIL::SigSpec>* clocks = cell2clkDomains[cell];

        if (clocks->size() == 0) {
            //log("    EMPTY CLOCK DOMAIN:  %s <%s>\n", (cell->type).c_str(), (cell->name).c_str());
            nbCellsWithNoClock++;
        }
        }
    }

    log("  Number of cells with no clock: %d\n", nbCellsWithNoClock);
    log(" -------------------------------------------------\n");
    }

    void splitBus(){
        // we need to slit busses in ihe netlsi to make sure the forward and  bacward
        // traversals will work correctly. Indeed when we have mixed up of single bit 
        // signals (ex: s[5]) and busses (ex: s[6:4]) it is extremely difficult to 
        // handle some clean traversals. By having a single bit to single bit
        // correspondance the traversals will behave correctly
        //
        log("Split bus into bits ...\n");
        run("splitnets -ports");

        // Use "write_verilog -simple_lhs" to deal with complex LHS. It is costly
        // in term of runtime so it would be better to fix it in "sig2sig" and avoid
        // to call this part of the code.
        // For '\LU32PEEng' we waste 150 seconds in this code and only 3 seconds to 
        // extract clock domains. Fixing it in "sig2sig" will then reduce to 3 seconds
        // the total extraction.

        //#define USE_WRITE_VERILOG_SIMPLE_LHS

        #ifdef USE_WRITE_VERILOG_SIMPLE_LHS

            // "-simple-lhs" to have single bits lhs assignments 
            //
            run("write_verilog -noexpr -simple-lhs splitnets.v");
            run("design -reset");

            string readArgs = GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, SIM_LIB_CARRY_FILE) 
                                        GET_FILE_PATH(GENESIS_3_DIR, LLATCHES_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, DFFRE_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, DFFNRE_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT1_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT2_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT3_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT4_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT5_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT6_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, CLK_BUF_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, O_BUF_SIM_FILE)
                                        GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, DSP_38_SIM_FILE)
                                        GET_FILE_PATH(GENESIS_3_DIR, BRAMS_SIM_NEW_LIB_FILE1)
                                        GET_FILE_PATH(GENESIS_3_DIR, BRAMS_SIM_LIB_FILE);
            run("read_verilog -lib -specify -nomem2reg" GET_FILE_PATH(COMMON_DIR, SIM_LIB_FILE) + readArgs);

            run("read_verilog splitnets.v");
        #endif

    }

    void extractAllClockDomains()
    {
        dict<RTLIL::SigSpec, std::set<RTLIL::SigSpec>*> rhsSig2LhsSig;
        dict<RTLIL::SigSpec, RTLIL::SigSpec> lhsSig2rhsSig;


        auto startTime = std::chrono::high_resolution_clock::now();

        // we need to slit busses in ihe netlsi to make sure the forward and  bacward
        // traversals will work correctly. Indeed when we have mixed up of single bit 
        // signals (ex: s[5]) and busses (ex: s[6:4]) it is extremely difficult to 
        // handle some clean traversals. By having a single bit to single bit
        // correspondance the traversals will behave correctly
        //
        splitBus();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        float totalTime = elapsed.count() * 1e-9;

        log("      [ Splitnet runTime = %.2f sec.]\n\n", totalTime);


        // Building connections dictionaries
        //
        // On cells connections
        //
        sig2cells(sig2CellsInFanout, sig2CellsInFanin);

        // on simple assignments (assign lhs = rhs)
        //
        sig2sig(rhsSig2LhsSig, lhsSig2rhsSig);

        // Buidling clock domains
        //
        dict<Cell*, std::set<RTLIL::SigSpec>*> cell2clkDomainsFwd;
        dict<Cell*, std::set<RTLIL::SigSpec>*> cell2clkDomainsBwd;

        // Forward propagation from sequential cells that will fill up 
        // 'cell2clkDomains'.
        //
        collectForward(sig2CellsInFanout, rhsSig2LhsSig, cell2clkDomainsFwd); 

        // Backward propagation from sequential cells that will fill up 
        // 'cell2clkDomains'.
        //
        collectBackward(sig2CellsInFanin, lhsSig2rhsSig, cell2clkDomainsBwd); 

        mergeClockDomains(cell2clkDomainsFwd, cell2clkDomainsBwd, cell2clkDomainsMerged);

        // Collect the cells with no associated clocks, e;G the ones between no sequential
        // cells (typically the ones between primary inputs and primary outputs).
        //
        collectNotTraversedCells(cell2clkDomainsMerged);

        reportCellsClockDomains(cell2clkDomainsMerged);

        endTime = std::chrono::high_resolution_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        totalTime = elapsed.count() * 1e-9;

        log("[Total clock domains extraction runTime = %.2f sec.]\n", totalTime);
    }

    void update_clkDomains(RTLIL::Cell *cell, RTLIL::SigSpec actual_clk){
        std::set<RTLIL::SigSpec>* clkDomainsMerged;

        if (cell2clkDomainsMerged.count(cell)) {
            clkDomainsMerged = cell2clkDomainsMerged[cell];
        } else {
            clkDomainsMerged = new std::set<RTLIL::SigSpec>;
            cell2clkDomainsMerged[cell] = clkDomainsMerged;
        }
        clkDomainsMerged->insert(actual_clk);
    }
    
    void check_dff(){
        for (auto dff : DFFs){
            if (dff->getPort(ID::E) == RTLIL::S1)
                ce_ffs[dff->getPort(ID::C)].push_back(1);
            else if (dff->getPort(ID::E) == RTLIL::S0)
                ce_ffs[dff->getPort(ID::C)].push_back(0);
            else 
                ce_ffs[dff->getPort(ID::C)].push_back(0.5);
            _clocks_.push_back(dff->getPort(ID::C));
            update_clkDomains(dff, dff->getPort(ID::C));
        }

        auto unique_clk = std::unique(_clocks_.begin(), _clocks_.end());
        _clocks_.erase(unique_clk, _clocks_.end());
        log("DFFs: %ld\n", DFFs.size());
        for (auto ce_ff: ce_ffs){
            float sum = std::accumulate(ce_ff.second.begin(), ce_ff.second.end(), 0.0);
            log("\tEnabled %ld %s 0.125 Typical %f\n", ce_ff.second.size(), log_signal(ce_ff.first), sum/ce_ff.second.size());
        }
    }
    
    string check_glitch_factor(RTLIL::Cell *lut){
        RTLIL::Const lut_init = lut->getParam(RTLIL::escape_id("INIT_VALUE"));
        int one_cnt = 0;
        string init_lut = lut->getParam(RTLIL::escape_id("INIT_VALUE")).as_string().c_str();
        for (char num : lut->getParam(RTLIL::escape_id("INIT_VALUE")).as_string()){
            if (num == '1') one_cnt++;
        }
        int length = GetSize(lut_init);
        if (one_cnt > 0) {
            if ((one_cnt > (length * .45)) && (one_cnt < (length * .55)) && (length>16)) {
                return  "Very_High";
            } else if ((one_cnt > (length * .35)) && (one_cnt < (length * .75)) && (length>16)) {
                return "High";
            }
            else
                return "Typical";
        }
        else
            return "Typical";
    }

    void check_LUT(){
        string clk_lut = "";
        string glitch_factor = "";
        RTLIL::Cell *lut = nullptr;
        for (auto &lut_cell : LUTs_Cells){
            lut = lut_cell.first;
            glitch_factor = check_glitch_factor(lut);
            if (LUTs_Cells.count(lut) == 0) {
                clk_lut = "unknown";
            } else {
                std::set<RTLIL::SigSpec> *clocks = lut_cell.second;
                for (std::set<RTLIL::SigSpec>::iterator it = clocks->begin(); it != clocks->end(); it++) {
                    RTLIL::SigSpec _lut_clk_ = *it;
                    if(_lut_clk_.size()>0)
                        clk_lut = log_signal(_lut_clk_);
                    else
                        clk_lut = "unknown";
                    break;
                }
            } 
            lut_clk[clk_lut].push_back(glitch_factor);          
        }
        log("LUTs: %ld\n",LUTs_Cells.size());
        for (const auto& pair : lut_clk) {
            const std::vector<std::string>& glitches = pair.second;
            std::unordered_map<std::string, int> elementCounts;
            for (const std::string& str : glitches) {
                elementCounts[str]++;
            }
            // log("Clock: %s\nLut Glitch:\n",pair.first.c_str());
            for (const auto& element : elementCounts) {
                if (pair.first != "unknown")
                    log("\tEnabled : %d : %s : 0.125 : %s\n",element.second, pair.first.c_str(),element.first.c_str());
                else
                    log("\tDisabled : %d : %s : 0.125 : %s\n",element.second, pair.first.c_str(),element.first.c_str());
                
            }
            // std::cout << std::endl;
        }
    }

    void check_dsp38(){
        string clk = "";
        SigSpec _clock_;
        std::tuple<RTLIL::IdString, string, int, int> elements;
        for (auto dsp : DSP_38){ 
            int a_size = check_port_width(dsp->getPort(ID::A));
            int b_size = check_port_width(dsp->getPort(ID::B));
            clk = log_signal(dsp->getPort(ID::CLK));
            update_clkDomains(dsp, dsp->getPort(ID::CLK));
            _clocks_.push_back(dsp->getPort(ID::CLK));
            
            elements = std::make_tuple(dsp->type, clk, a_size, b_size);
            if (dsp_out.find(elements) != dsp_out.end()) {
                dsp_out[elements]++;
            } else {
                dsp_out[elements] = 1;
            }
        }
        clk = "";
        for (auto &dsp_no_clk_cell : DSP_Cells){
            int a_size = check_port_width(dsp_no_clk_cell.first->getPort(ID::A));
            int b_size = check_port_width(dsp_no_clk_cell.first->getPort(ID::B));
            if (DSP_Cells.count(dsp_no_clk_cell.first) == 0) {
                clk = "unknown";
            } else {
                std::set<RTLIL::SigSpec> *clocks = dsp_no_clk_cell.second;
                for (std::set<RTLIL::SigSpec>::iterator it = clocks->begin(); it != clocks->end(); it++) {
                    RTLIL::SigSpec dsp_clk = *it;
                    if(dsp_clk.size()>0)
                        clk = log_signal(dsp_clk);
                    else
                        clk = "unknown";
                    break;
                }
            }
            elements = std::make_tuple(dsp_no_clk_cell.first->type, clk, a_size, b_size);
            if (dsp_out.find(elements) != dsp_out.end()) {
                dsp_out[elements]++;
            } else {
                dsp_out[elements] = 1;
            }
        }

        auto unique_clk = std::unique(_clocks_.begin(), _clocks_.end());
        _clocks_.erase(unique_clk, _clocks_.end());

        log("DSP's : %ld\n",DSP_38.size());
        for (auto &dsp : dsp_out){
            log("\t%s : %s : %d %d : %d\n", log_id(std::get<0>(dsp.first)), (std::get<1>(dsp.first)).c_str(), std::get<2>(dsp.first), std::get<3>(dsp.first), dsp.second);
        }

    }

    int check_port_width (SigSpec _Port_){
        int width = 0;
        for (auto port_bit : _Port_){
            if(!(port_bit == State::Sx || port_bit == State::S0 || port_bit == State::S1))
                width++;
        }
        return width;
    }
    
    void check_port_const (SigSpec _Port_,float &value){
        if (_Port_.is_fully_const()){
            if (_Port_ == State::S0) value = 0;
            else value = 1;
        }
    }

    void check_BRAM (){
        for (auto ram : TDP36K){
            int arwidth = 0;
            int brwidth = 0;
            int awwidth = 0;
            int bwwidth = 0;
            float ena = 0.5;
            float wena = 0.5;
            float enb = 0.5;
            float wenb = 0.5;
            string ram_type = "";
            switch (tech) {
                case Technologies::GENESIS: 
                case Technologies::GENESIS_2: {
                    arwidth = check_port_width(ram->getPort(RTLIL::escape_id("RDATA_A1"))) + check_port_width(ram->getPort(RTLIL::escape_id("RDATA_A2")));
                    brwidth = check_port_width(ram->getPort(RTLIL::escape_id("RDATA_B1"))) + check_port_width(ram->getPort(RTLIL::escape_id("RDATA_B2")));
                    awwidth = check_port_width(ram->getPort(RTLIL::escape_id("WDATA_A1"))) + check_port_width(ram->getPort(RTLIL::escape_id("WDATA_A2")));
                    bwwidth = check_port_width(ram->getPort(RTLIL::escape_id("WDATA_B1"))) + check_port_width(ram->getPort(RTLIL::escape_id("WDATA_B2")));

                    check_port_const(ram->getPort(RTLIL::escape_id("REN_A1")),ena);
                    check_port_const(ram->getPort(RTLIL::escape_id("REN_A2")),ena);
                    check_port_const(ram->getPort(RTLIL::escape_id("WEN_A1")),wena);
                    check_port_const(ram->getPort(RTLIL::escape_id("WEN_A2")),wena);

                    check_port_const(ram->getPort(RTLIL::escape_id("REN_B1")),enb);
                    check_port_const(ram->getPort(RTLIL::escape_id("REN_B2")),enb);
                    check_port_const(ram->getPort(RTLIL::escape_id("WEN_B1")),wenb);
                    check_port_const(ram->getPort(RTLIL::escape_id("WEN_B2")),wenb);

                    ram_type = "36k ";
                    break;
                }
                case Technologies::GENESIS_3:{
                    if (ram->type == RTLIL::escape_id("TDP_RAM18KX2")){
                        arwidth = check_port_width(ram->getPort(RTLIL::escape_id("RDATA_A1"))) + check_port_width(ram->getPort(RTLIL::escape_id("RDATA_A2")));
                        brwidth = check_port_width(ram->getPort(RTLIL::escape_id("RDATA_B1"))) + check_port_width(ram->getPort(RTLIL::escape_id("RDATA_B2")));
                        awwidth = check_port_width(ram->getPort(RTLIL::escape_id("WDATA_A1"))) + check_port_width(ram->getPort(RTLIL::escape_id("WDATA_A2")));
                        bwwidth = check_port_width(ram->getPort(RTLIL::escape_id("WDATA_B1"))) + check_port_width(ram->getPort(RTLIL::escape_id("WDATA_B2")));

                        check_port_const(ram->getPort(RTLIL::escape_id("REN_A1")),ena);
                        check_port_const(ram->getPort(RTLIL::escape_id("REN_A2")),ena);
                        check_port_const(ram->getPort(RTLIL::escape_id("WEN_A1")),wena);
                        check_port_const(ram->getPort(RTLIL::escape_id("WEN_A2")),wena);

                        check_port_const(ram->getPort(RTLIL::escape_id("REN_B1")),enb);
                        check_port_const(ram->getPort(RTLIL::escape_id("REN_B2")),enb);
                        check_port_const(ram->getPort(RTLIL::escape_id("WEN_B1")),wenb);
                        check_port_const(ram->getPort(RTLIL::escape_id("WEN_B2")),wenb);


                        ram_type = "18kx2 ";
                    }
                    if(ram->type == RTLIL::escape_id("TDP_RAM36K")){
                        arwidth = check_port_width(ram->getPort(RTLIL::escape_id("RDATA_A"))) + check_port_width(ram->getPort(RTLIL::escape_id("RPARITY_A")));
                        awwidth = check_port_width(ram->getPort(RTLIL::escape_id("WDATA_A"))) + check_port_width(ram->getPort(RTLIL::escape_id("WPARITY_A")));
                        brwidth = check_port_width(ram->getPort(RTLIL::escape_id("RDATA_B"))) + check_port_width(ram->getPort(RTLIL::escape_id("RPARITY_B")));
                        bwwidth = check_port_width(ram->getPort(RTLIL::escape_id("WDATA_B"))) + check_port_width(ram->getPort(RTLIL::escape_id("WPARITY_B")));

                        check_port_const(ram->getPort(RTLIL::escape_id("REN_A")),ena);
                        check_port_const(ram->getPort(RTLIL::escape_id("WEN_A")),wena);

                        check_port_const(ram->getPort(RTLIL::escape_id("REN_B")),enb);
                        check_port_const(ram->getPort(RTLIL::escape_id("WEN_B")),wenb);

                        ram_type = "36k ";
                    }
                    break;
                }
                case Technologies::GENERIC:{
                    break;
                }    
            }
            if (arwidth < awwidth) arwidth = awwidth;
            if (brwidth < bwwidth) brwidth = bwwidth;
            if (((enb==0 && wenb!=0) && (ena!=0 && wena==0)) || ((ena==0 && wena!=0) && (enb!=0 && wenb==0))){
                ram_type +=  "SDP";
            }
            else {
                ram_type += "TDP";
            }
            std::tuple<std::string, string, string, int, int, float, float, float, float> elements;
            string ClkA = "unknown";
            string ClkB = "unknown";
            if ((tech == Technologies::GENESIS_3) && (ram->type == RTLIL::escape_id("TDP_RAM36K"))){
                if (!((ram->getPort(RTLIL::escape_id("CLK_A")).as_chunk()).wire == NULL)){
                    ClkA = log_signal(ram->getPort(RTLIL::escape_id("CLK_A")));
                    _clocks_.push_back(ram->getPort(RTLIL::escape_id("CLK_A")));
                    update_clkDomains(ram, ram->getPort(RTLIL::escape_id("CLK_A")));
                }
                if (!((ram->getPort(RTLIL::escape_id("CLK_B")).as_chunk()).wire == NULL)){
                    ClkB = log_signal(ram->getPort(RTLIL::escape_id("CLK_B")));
                    _clocks_.push_back(ram->getPort(RTLIL::escape_id("CLK_B")));
                    update_clkDomains(ram, ram->getPort(RTLIL::escape_id("CLK_B")));
                }
                elements = std::make_tuple(ram_type, ClkA, ClkB, arwidth, brwidth, ena, enb, wena, wenb);
            }
            else if (tech != Technologies::GENERIC){
                if (!((ram->getPort(RTLIL::escape_id("CLK_A1")).as_chunk()).wire == NULL)){
                    ClkA = log_signal(ram->getPort(RTLIL::escape_id("CLK_A1")));
                    _clocks_.push_back(ram->getPort(RTLIL::escape_id("CLK_A1")));
                    update_clkDomains(ram, ram->getPort(RTLIL::escape_id("CLK_A1")));
                }
                if (!((ram->getPort(RTLIL::escape_id("CLK_B1")).as_chunk()).wire == NULL)){
                    ClkB = log_signal(ram->getPort(RTLIL::escape_id("CLK_B1")));
                    _clocks_.push_back(ram->getPort(RTLIL::escape_id("CLK_B1")));
                    update_clkDomains(ram, ram->getPort(RTLIL::escape_id("CLK_B1")));
                }
                
                elements = std::make_tuple(ram_type, ClkA, ClkB, arwidth, brwidth, ena, enb, wena, wenb);
            }
            if (tdp_out.find(elements) != tdp_out.end()) {
                tdp_out[elements]++;
            } else {
                tdp_out[elements] = 1;
            }
        }
        
        auto unique_clk = std::unique(_clocks_.begin(), _clocks_.end());
        _clocks_.erase(unique_clk, _clocks_.end());
        
        log("BRAM's : %ld\n",TDP36K.size());
        for (auto &bram : tdp_out){
            log("\t%s : %s %s : %d %d : %2f %2f : %2f %2f : %d\n", std::get<0>(bram.first).c_str(), std::get<1>(bram.first).c_str(), std::get<2>(bram.first).c_str(), std::get<3>(bram.first), std::get<4>(bram.first), std::get<5>(bram.first), std::get<6>(bram.first), std::get<7>(bram.first), std::get<8>(bram.first), bram.second);
        }
    }
    
    bool find_io_clk (RTLIL::Cell * cell, RTLIL::SigSpec sig_port){
        std::set<RTLIL::SigSpec> *clocks;
        std::set<string> *cdc_clk;
        bool clk_matched = false;
        if (cell2clkDomainsMerged[cell] != NULL){
            clocks = cell2clkDomainsMerged[cell];
            if (ios_clk.count(sig_port)) {
                cdc_clk = ios_clk[sig_port];
            } else {
                cdc_clk = new std::set<string>;
                ios_clk[sig_port] =cdc_clk;
            }
            for (std::set<RTLIL::SigSpec>::iterator it = clocks->begin(); it != clocks->end(); it++) {
                RTLIL::SigSpec cell_clk = *it;
                if(GetSize(cell_clk)>0){ 
                    if (cdc_clk->count(log_signal(cell_clk)))
                        continue;
                    
                    cdc_clk->insert(log_signal(cell_clk));                    
                    // ios_clk[sig_port] = log_signal(cell_clk);
                    clk_matched =  true;
                }
            }
            if (clk_matched){
                return true;
            }
        }
        return false;
    }
    
    void check_ios(){
        if (IBUFs.size()!=0){
            for (auto i_buf : IBUFs){
                for (auto clk : _clocks_){
                    if (CLK_BUF.size()==0){
                        if (i_buf->getPort(RTLIL::escape_id("O")) == clk){
                            clk_from_buffer[clk] = i_buf->getPort(RTLIL::escape_id("I"));
                            IBUFs.erase(remove(IBUFs.begin(),IBUFs.end(),i_buf));
                        }
                    }
                    else{
                        for (auto clk_buf : CLK_BUF){
                            if (clk_buf->getPort(RTLIL::escape_id("O")) != clk){
                                continue;
                            }
                            if (clk_buf->getPort(RTLIL::escape_id("I")) == i_buf->getPort(RTLIL::escape_id("O"))){
                                clk_from_buffer[clk] = i_buf->getPort(RTLIL::escape_id("I"));
                            }
                            
                        }
                    }
                }
            }
        }

        std::set<std::tuple<int, string, string, string, string>> filteredVector;
        bool clk_found = false;
        std::set<Cell*>* Cells_Set;
        RTLIL::Cell *cell;
        for (auto &module : _design->selected_modules()) {
            for (auto wire : module->wires()){
                clk_found = false;
                RTLIL::SigSpec sig_port = wire;
                if(!(wire->port_input || wire->port_output)){
                    continue;
                }
                if (sig2CellsInFanout[sig_port] != NULL){
                    Cells_Set = sig2CellsInFanout[sig_port];
                    for (std::set<RTLIL::Cell*>::iterator it = Cells_Set->begin(); it != Cells_Set->end(); it++) {
                        cell = *it;
                        find_io_clk(cell, sig_port);
                    }
                }
                
                if (sig2CellsInFanin[sig_port] != NULL){
                    Cells_Set = sig2CellsInFanin[sig_port];
                    for (std::set<RTLIL::Cell*>::iterator it = Cells_Set->begin(); it != Cells_Set->end(); it++) {
                        cell = *it;      
                        find_io_clk(cell, sig_port);
                    }
                }
            }
        }
        
        string io_type = "SDR";
        for (auto orig_io : IO_dict){
            clk_found = false;
            for (auto ind_clk : ios_clk){
                RTLIL::SigSpec io_port = ind_clk.first;
                std::set<string> *clks;
                if (!(ios_clk.count(io_port))){
                    continue;
                }
                clks = ios_clk[io_port];
                for (std::set<string>::iterator it = clks->begin(); it != clks->end(); it++){ 
                    string clk = *it;
                    if (!(clk.size()))
                        continue;
                    string ind_clk_first = log_signal(ind_clk.first);
                    std::get<0>(orig_io).erase(std::remove_if(std::get<0>(orig_io).begin(), std::get<0>(orig_io).end(), ::isspace), std::get<0>(orig_io).end());
                    // check if actual IO is part of io, clock table or not
                    if (ind_clk_first == std::get<0>(orig_io)){
                        clk_found = true;
                        // check if IO is a clock or SDR
                        if ((std::get<1>(orig_io) == clk) || (log_signal(clk_from_buffer[clk]) == std::get<1>(orig_io)))
                            io_type = "Clock";
                        else
                            io_type = "SDR";

                        filteredVector.insert(make_tuple(std::get<3>(orig_io), std::get<1>(orig_io).c_str(), std::get<2>(orig_io).c_str(), io_type.c_str(),clk.c_str()));
                        // Break the loop if first instance of an associated clock is found for IO in the cell.
                        break;
                        
                    }
                }
                if (clk_found) break;
            }
            if (clk_found == false){
                filteredVector.insert(make_tuple(std::get<3>(orig_io), std::get<1>(orig_io).c_str(), std::get<2>(orig_io).c_str(), io_type.c_str(), "unknown"));
            }
        }
        std::set<std::tuple<int, string, string, string, string>> uniqueTuples;
        for (const auto& tuple : filteredVector) {
            if (uniqueTuples.insert(tuple).second) {
                ios_out.push_back(tuple);
            }
        }
        log("IOs: %ld\n",ios_out.size());
        for (auto io_rpt : ios_out){
            log("\t%d %s %s %s  %s\n",std::get<0>(io_rpt), std::get<1>(io_rpt).c_str(), std::get<2>(io_rpt).c_str(), std::get<3>(io_rpt).c_str(), std::get<4>(io_rpt).c_str());
        }
        
    }

    int sdc_parsing (string sdc_file){
        std::ifstream sdcFile(sdc_file); // Replace "input.sdc" with your SDC file name

        if (!sdcFile.is_open()) {
            log_warning("Error opening the SDC constraint file.\n");
            return 0;
        }

        std::regex regexPattern("(?:-name\\s+)?(\\w+)\\s*-period\\s+([\\d.]+)|(-period\\s+([\\d.]+)\\s+(\\w+))");
        std::string line;
        while (std::getline(sdcFile, line)) {
            std::smatch match;
            if (std::regex_search(line, match, regexPattern)) {
                std::string clockName = match[1].str();
                double period = (match[2].str() != "") ? std::stod(match[2].str()) : std::stod(match[4].str());

                // Store clock details (clock name and period) in the map
                sdc_clks[clockName] = period;
            }
        }
        sdcFile.close();
        
        // Printing extracted clocks (both virtual and actual)
        log("All Clocks:\n");
        for (const auto& pair : sdc_clks) {
            log("\tClock Name: %s Period %f\n" ,pair.first.c_str() , pair.second);
        }
        return 0;
    }

    void writeCSV(const std::string& filename, const std::vector<std::vector<std::string>>& data) {
        std::ofstream file(filename, std::ios_base::app); // Open file in append mode

        if (!file.is_open()) {
            std::cerr << "Error opening file!" << std::endl;
            return;
        }

        for (const auto& row : data) {
            for (size_t i = 0; i < row.size(); ++i) {
                file << row[i];
                if (i != row.size() - 1) {
                    file << ",";
                }
            }
                file << "\n";
        }

        file.close();
    }

    void gen_csv (){
        if (!sdc_clks.empty()){
            for (auto sdc_clk : sdc_clks){
                if (clk_from_buffer.empty()){
                    for (auto clk : _clocks_){
                        if ("\\" + sdc_clk.first == log_signal(clk)){
                            clk_out[log_signal(clk)] = sdc_clk.second;
                        }
                    }
                }
                else{
                    for (auto clk : clk_from_buffer){
                        if ("\\" + sdc_clk.first == log_signal(clk.second)){
                            clk_out[log_signal(clk.second)] = sdc_clk.second;
                        }
                    }
                }
                if (clk_out.empty()){
                    if (_clocks_.empty()){
                        clk_out["unknown"] = 0;
                    }
                    else{
                        for (auto clk : _clocks_){
                            clk_out[log_signal(clk)] = 0;
                        }
                    }
                }
            }
        }
        else{
            if (_clocks_.empty()){
                clk_out["unknown"] = 0;
            }
            else{
                for (auto clk : _clocks_){
                    clk_out[log_signal(clk)] = 0;
                }
            }
        }
        std::vector<std::vector<std::string>> clock_data_header = {
            {},
            {"Clocks"}
        };
        writeCSV("power.csv", clock_data_header);
        string  clk_type = ""; 
        for (auto clk : clk_out){
            
            if (clk.first == "unknown") clk_type = "Internal";
            else clk_type = "I/O";
            std::vector<std::vector<std::string>> clock_data = {
                {"", "Name", "Type", "Frequency" },
                {"", clk.first, clk_type, std::to_string((1/clk.second)*1000)}
            };
            writeCSV("power.csv", clock_data);
        }

        std::vector<std::vector<std::string>> fabric = {
            {},
            {"Fabric Logic Element"},
            {},
            {"LUTs"},
            {"", "LUT Count", "Associated Clock", "Toggle Rate", "Glitch Factor"}
        };
        writeCSV("power.csv", fabric);
        std::vector<std::vector<std::string>> lut;

        if (lut_clk.empty()){
            lut = {{"", "0", "", "", ""}};
            writeCSV("power.csv", lut);
        }
        for (const auto& pair : lut_clk) {
            const std::vector<std::string>& glitches = pair.second;
            std::unordered_map<std::string, int> elementCounts;
            for (const std::string& str : glitches) {
                elementCounts[str]++;
            }
            // log("Clock: %s\nLut Glitch:\n",pair.first.c_str());
            for (const auto& element : elementCounts) { 
                lut = {{"", std::to_string(element.second), pair.first, "0.125", element.first}};
            writeCSV("power.csv", lut);
                // std::cout << "\t"<<element.first << ": " << element.second << std::endl;
            }
            // std::cout << std::endl;
        }

        std::vector<std::vector<std::string>> DFF = {
            {},
            {"DFFs"},
            {"", "DFF Count", "Clock", "Toggle Rate", "Glitch Factor", "Clock Enable"}
        };
        writeCSV("power.csv", DFF);
        std::vector<std::vector<std::string>> dff_data;
        if (ce_ffs.empty()){
            dff_data = {{"", "0", "", "", "", ""}};
            writeCSV("power.csv", dff_data);
        }
        for (auto ce_ff: ce_ffs){
            float sum = std::accumulate(ce_ff.second.begin(), ce_ff.second.end(), 0.0);
            dff_data = {{"", std::to_string(sum), log_signal(ce_ff.first), "0.125", "Typical", std::to_string(sum/ce_ff.second.size())}};
            writeCSV("power.csv", dff_data);
            // log("Key = %s Sum = %f Result = %f\n",log_signal(ce_ff.first),sum, sum/ce_ff.second.size());
        }

        std::vector<std::vector<std::string>> bram;
        bram = {
            {},
            {"BRAMs"},
            {"", "Bram Count", "Bram Type", "Clock A", "Clock B", "Width A", "Width B", "Ren A", "Ren B", "Wen A", "Wen B", "Toggle Rate A", "Toggle Rate B"}
        };
        writeCSV("power.csv", bram);
        std::vector<std::vector<std::string>> bram_data;
        if (tdp_out.empty()){
            bram_data = {{"", "0", "", "", "", "", "", "", "", "", "", ""}};
            writeCSV("power.csv", bram_data);
        }
        for (auto &ram : tdp_out){
            bram_data = {
                {"", std::to_string(ram.second), std::get<0>(ram.first), std::get<1>(ram.first), std::get<2>(ram.first), \
                std::to_string(std::get<3>(ram.first)), std::to_string(std::get<4>(ram.first)), std::to_string(std::get<5>(ram.first)), \
                std::to_string(std::get<6>(ram.first)), std::to_string(std::get<7>(ram.first)), std::to_string(std::get<8>(ram.first)), "0.125", "0.125"}
            };
            writeCSV("power.csv", bram_data);
            // log("\t%s : %s %s : %d %d : %2f %2f : %2f %2f : %d\n", std::get<0>(bram.first).c_str(), log_signal(std::get<1>(bram.first)), log_signal(std::get<2>(bram.first)), std::get<3>(bram.first), std::get<4>(bram.first), std::get<5>(bram.first), std::get<6>(bram.first), std::get<7>(bram.first), std::get<8>(bram.first), bram.second);
        }
        std::vector<std::vector<std::string>> dsp;
        dsp = {
            {},
            {"DSPs"},
            {"", "DSP Count", "DSP Type", "Clock", "Width A", "Width B", "Toggle Rate"}
        };
        writeCSV("power.csv", dsp);
        std::vector<std::vector<std::string>> dsp_data;
        for (auto &dsp : dsp_out){
            dsp_data = {
                {"", std::to_string(dsp.second), log_id(std::get<0>(dsp.first)), std::get<1>(dsp.first), std::to_string(std::get<2>(dsp.first)), \
                std::to_string(std::get<3>(dsp.first)), "0.125"}
            };
            writeCSV("power.csv", dsp_data);
            // log("\t%s : %s : %d %d : %d\n", log_id(std::get<0>(dsp.first)), (std::get<1>(dsp.first)).c_str(), std::get<2>(dsp.first), std::get<3>(dsp.first), dsp.second);
        }
        std::vector<std::vector<std::string>> _io_;
        _io_ = {
            {},
            {"IOs"},
            {"", "IO Name", "Bus Size", "Direction", "Voltage Rating", "Current Rating", "Operating Condition", "Data Type", "Associated Clock"}
        };
        writeCSV("power.csv", _io_);
        std::vector<std::vector<std::string>> _io_data_;
        for (auto io_out : ios_out){
            // GetSize(_port_), log_signal(_port_), "Output", io_type, io_clk_pair.second
            _io_data_ = {
            {"", std::get<1>(io_out), std::to_string(std::get<0>(io_out)), std::get<2>(io_out), "LVCMOS 1.8V (HR)", "2 mA", "Slow", std::get<3>(io_out).c_str(), std::get<4>(io_out)}
        };
        writeCSV("power.csv", _io_data_);
            // log("\t%d %s %s %s %s\n",std::get<0>(io_out),std::get<1>(io_out).c_str(),std::get<2>(io_out).c_str(), std::get<3>(io_out).c_str(), std::get<4>(io_out).c_str());
        }

    }

    void gen_csv_old (){
        if (!sdc_clks.empty()){
            for (auto sdc_clk : sdc_clks){
                if (clk_from_buffer.empty()){
                    for (auto clk : _clocks_){
                        if ("\\" + sdc_clk.first == log_signal(clk)){
                            clk_out[log_signal(clk)] = sdc_clk.second;
                        }
                    }
                }
                else{
                    for (auto clk : clk_from_buffer){
                        if ("\\" + sdc_clk.first == log_signal(clk.second)){
                            clk_out[log_signal(clk.second)] = sdc_clk.second;
                        }
                    }
                }
                if (clk_out.empty()){
                    if (_clocks_.empty()){
                        clk_out["unknown"] = 0;
                    }
                    else{
                        for (auto clk : _clocks_){
                            clk_out[log_signal(clk)] = 0;
                        }
                    }
                }
            }
        }
        else{
            if (_clocks_.empty()){
                clk_out["unknown"] = 0;
            }
            else{
                for (auto clk : _clocks_){
                    clk_out[log_signal(clk)] = 0;
                }
            }
        }
        std::vector<std::vector<std::string>> clock_data_header = {
            {},
            {"Clocks"}
        };
        writeCSV("power.csv", clock_data_header);
        string enabled = "";
        string  clk_type = ""; 
        for (auto clk : clk_out){
            if (clk.first == "unknown") {
                clk_type = "Internal";
            }
            else {
                clk_type = "I/O";
                enabled = "Enabled";
            }
            std::vector<std::vector<std::string>> clock_data = {
                {enabled, "",  clk.first, clk_type, std::to_string((1/clk.second)*1000) }
            };
            writeCSV("power.csv", clock_data);
        }

        std::vector<std::vector<std::string>> fabric = {
            {},
            {"Fabric Logic Element"},
        };
        writeCSV("power.csv", fabric);
        std::vector<std::vector<std::string>> lut;

        if (lut_clk.empty()){
            lut = {{"", "0", "", "", ""}};
            writeCSV("power.csv", lut);
        }
        for (const auto& pair : lut_clk) {
            const std::vector<std::string>& glitches = pair.second;
            std::unordered_map<std::string, int> elementCounts;
            for (const std::string& str : glitches) {
                elementCounts[str]++;
            }
            for (const auto& element : elementCounts) { 
                lut = {{"Enabled", std::to_string(element.second), "", pair.first, "0.125", element.first}};
            writeCSV("power.csv", lut);
            }
        }

        std::vector<std::vector<std::string>> dff_data;
        if (ce_ffs.empty()){
            dff_data = {{"", "", "0", "", "", ""}};
            writeCSV("power.csv", dff_data);
        }
        for (auto ce_ff: ce_ffs){
            float sum = std::accumulate(ce_ff.second.begin(), ce_ff.second.end(), 0.0);
            dff_data = {{"Enabled", "", std::to_string(DFFs.size()), log_signal(ce_ff.first), "0.125", "Typical", std::to_string(sum/ce_ff.second.size())}};
            writeCSV("power.csv", dff_data);
        }

        std::vector<std::vector<std::string>> bram;
        bram = {
            {},
            {"BRAM"}
        };
        writeCSV("power.csv", bram);
        std::vector<std::vector<std::string>> bram_data;
        if (tdp_out.empty()){
            bram_data = {{"", "0", "", "", "", "", "", "", "", "", "", ""}};
            writeCSV("power.csv", bram_data);
        }
        for (auto &ram : tdp_out){
            bram_data = {
                {"Enabled", std::to_string(ram.second), std::get<0>(ram.first), std::get<1>(ram.first), std::get<2>(ram.first), \
                std::to_string(std::get<3>(ram.first)), std::to_string(std::get<4>(ram.first)), std::to_string(std::get<5>(ram.first)), \
                std::to_string(std::get<6>(ram.first)), std::to_string(std::get<7>(ram.first)), std::to_string(std::get<8>(ram.first)), "0.125", "0.125"}
            };
            writeCSV("power.csv", bram_data);
        }
        std::vector<std::vector<std::string>> dsp;
        dsp = {
            {},
            {"DSP"}
        };
        writeCSV("power.csv", dsp);
        std::vector<std::vector<std::string>> dsp_data;
        for (auto &dsp : dsp_out){
            dsp_data = {
                {"Enabeld", std::to_string(dsp.second), log_id(std::get<0>(dsp.first)), std::get<1>(dsp.first), std::to_string(std::get<2>(dsp.first)), \
                std::to_string(std::get<3>(dsp.first)), "0.125"}
            };
            writeCSV("power.csv", dsp_data);
        }
        std::vector<std::vector<std::string>> _io_;
        _io_ = {
            {},
            {"I/O"}
        };
        writeCSV("power.csv", _io_);
        std::vector<std::vector<std::string>> _io_data_;
        for (auto io_out : ios_out){
            if (std::get<4>(io_out) == "unknown") enabled = "Disabled";
            else enabled = "Enabled";
            _io_data_ = {
            {enabled, std::get<1>(io_out), std::to_string(std::get<0>(io_out)), std::get<2>(io_out), "LVCMOS 1.8V (HR)", "2 mA", "Slow", std::get<3>(io_out).c_str(), std::get<4>(io_out)}
            };
            writeCSV("power.csv", _io_data_);
        }

    }

    void script() override
    {
        
        auto start = std::chrono::high_resolution_clock::now();
        string readArgs;
        if (preserve_ip){
            RTLIL::IdString protectId("$rs_protected");
            for (auto &module : _design->selected_modules()) {
                if (module->get_bool_attribute(protectId)) {
                    run(stringf("blackbox %s", module->name.c_str()));
                    _design->unset_protcted_rtl();
                }
            }
        }

        if (check_label("begin") && tech != Technologies::GENERIC) {
            switch (tech) {
                case Technologies::GENESIS: {
                    readArgs = GET_FILE_PATH(GENESIS_DIR, SIM_LIB_FILE) 
                                GET_FILE_PATH(GENESIS_DIR, DSP_SIM_LIB_FILE);
                    break;
                }    
                case Technologies::GENESIS_2: {
                    readArgs = GET_FILE_PATH(GENESIS_2_DIR, SIM_LIB_FILE) 
                                GET_FILE_PATH(GENESIS_2_DIR, DSP_SIM_LIB_FILE) 
                                GET_FILE_PATH(GENESIS_2_DIR, BRAMS_SIM_LIB_FILE);
                    break;
                }    
                case Technologies::GENESIS_3: {
                    readArgs = GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, SIM_LIB_CARRY_FILE) 
                                GET_FILE_PATH(GENESIS_3_DIR, LLATCHES_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, DFFRE_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, DFFNRE_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT1_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT2_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT3_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT4_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT5_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, LUT6_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, CLK_BUF_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, O_BUF_SIM_FILE)
                                GET_FILE_PATH_RS_FPGA_SIM(GENESIS_3_DIR, DSP_38_SIM_FILE)
                                GET_FILE_PATH(GENESIS_3_DIR, BRAMS_SIM_NEW_LIB_FILE1)
                                GET_FILE_PATH(GENESIS_3_DIR, BRAMS_SIM_LIB_FILE);
                    break;
                }    
                // Just to make compiler happy
                case Technologies::GENERIC: {
                    break;
                } 
            }
            run("read_verilog -lib -specify -nomem2reg" GET_FILE_PATH(COMMON_DIR, SIM_LIB_FILE) + readArgs);

            for (auto &module : _design->selected_modules()) {
                module_name = log_id(module->name);
                for (auto wire : module->wires()){
                    RTLIL::SigSpec io_port_ = wire;
                    for(auto ind_wire : io_port_){
                        if (wire->port_input){
                            IO_dict.insert(make_tuple(log_signal(ind_wire), log_signal(io_port_), "Input", GetSize(io_port_)));
                        }
                        if (wire->port_output){
                            IO_dict.insert(make_tuple(log_signal(ind_wire), log_signal(io_port_), "Output", GetSize(io_port_)));
                        }
                    }
                }
                for (auto &cell : module->selected_cells()) {
               
                    if (cell->type == RTLIL::escape_id("DFFRE") \
                        || cell->type == RTLIL::escape_id("DFFNRE")) {
                        DFFs.push_back(cell);
                        continue;
                    }
                    if (cell->type == RTLIL::escape_id("DSP38") || cell->type == RTLIL::escape_id("DSP19x2")) {
                        if(getPortActual_noref(RTLIL::escape_id("CLK"), cell))
                            DSP_38.push_back(cell);
                        continue;
                    }
                     if (cell->type == RTLIL::escape_id("RS_TDP36K") || cell->type == RTLIL::escape_id("TDP_RAM18KX2") || cell->type == RTLIL::escape_id("TDP_RAM36K")) {
                        TDP36K.push_back(cell);
                        continue;
                    }
                    if (cell->type == RTLIL::escape_id("I_BUF")) {
                        IBUFs.push_back(cell);
                        continue;
                    }
                    if (cell->type == RTLIL::escape_id("O_BUF")) {
                        OBUFs.push_back(cell);
                        continue;
                    }
                    if (cell->type == RTLIL::escape_id("CLK_BUF")) {
                        CLK_BUF.push_back(cell);
                        continue;
                    }
                
                }
            }
            
            std::ifstream file("power.csv");
            if (file.is_open()) {
                file.close();
                if (std::remove("power.csv") != 0) {
                    std::cerr << "Error deleting file!" << std::endl;
                }
            }
            std::vector<std::vector<std::string>> summary = {
                {"Summary"},
                {"Top-level Name", module_name }
            };
            writeCSV("power.csv", summary);

            extractAllClockDomains();
            check_dff();
            check_BRAM();
            check_dsp38();
            check_LUT();
            check_ios();    
            sdc_parsing(sdc_str);
            gen_csv_old();
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration =  std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
            char csv_path[FILENAME_MAX];
            if (getcwd(csv_path, FILENAME_MAX) == nullptr) {
                log_warning("Error getting path for power.csv");
            } else {
                strcat(csv_path, "/power.csv");
                log("\nINFO: PWR: Created %s\n",csv_path);
            }
            log("\nTime taken by power data extraction tool = %fs\n",duration.count());
            

        }
    }

} PowerExtractRapidSilicon;
PRIVATE_NAMESPACE_END