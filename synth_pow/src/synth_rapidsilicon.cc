/*
 *  Copyright (C) 2022 RapidSilicon
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
#include <numeric>
#include <algorithm>

#ifdef PRODUCTION_BUILD
#include "License_manager.hpp"
#endif

int DSP_COUNTER;
USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

#define XSTR(val) #val
#define STR(val) XSTR(val)

#ifndef PASS_NAME
#define PASS_NAME synth_pow
#endif

#define GENESIS_DIR genesis
#define GENESIS_2_DIR genesis2
#define GENESIS_3_DIR genesis3
#define COMMON_DIR common
#define SIM_LIB_FILE cells_sim.v
#define LLATCHES_SIM_FILE llatches_sim.v
#define DSP_SIM_LIB_FILE dsp_sim.v
#define BRAMS_SIM_LIB_FILE brams_sim.v
#define FFS_MAP_FILE ffs_map.v
#define LUTx_SIM_FILE LUT.v
#define LUT_FINAL_MAP_FILE lut_map.v
#define ARITH_MAP_FILE arith_map.v
#define DSP_MAP_FILE dsp_map.v
#define DSP_FINAL_MAP_FILE dsp_final_map.v
#define ALL_ARITH_MAP_FILE all_arith_map.v
#define BRAM_TXT brams.txt
#define BRAM_LIB brams_new.txt
#define BRAM_LIB_SWAP brams_new_swap.txt
#define BRAM_ASYNC_TXT brams_async.txt
#define BRAM_MAP_FILE brams_map.v
#define BRAM_MAP_NEW_FILE brams_map_new.v
#define BRAM_FINAL_MAP_FILE brams_final_map.v
#define BRAM_FINAL_MAP_NEW_FILE brams_final_map_new.v
#define GET_FILE_PATH(tech_dir,file) " +/rapidsilicon/" STR(tech_dir) "/" STR(file)
#define IO_cells_FILE io_cells_primitives_new.sv
#define IO_MODEL_FILE io_model_map_new.v
#define GET_FILE_PATH_RS_LUTx_PRIMITVES(tech_dir,file) " +/rapidsilicon/" STR(tech_dir) "/RS_PRIMITIVES/LUT/" STR(file)
#define GET_FILE_PATH_RS_PRIMITVES(tech_dir,file) " +/rapidsilicon/" STR(tech_dir) "/RS_PRIMITIVES/IO/" STR(file)
#define GET_TECHMAP_FILE_PATH_RS_PRIMITVES(tech_dir,file) " +/rapidsilicon/" STR(tech_dir) "/RS_PRIMITIVES/TECHMAP/" STR(file)



#define VERSION_MAJOR 0 // 0 - beta 

#define VERSION_MINOR 4
#define VERSION_PATCH 196

enum Technologies {
    GENERIC,   
    GENESIS,
    GENESIS_2,
    GENESIS_3
};



struct SynthPowerRapidSiliconPass : public ScriptPass {

    SynthPowerRapidSiliconPass() : ScriptPass(STR(PASS_NAME), "Synthesis for RapidSilicon FPGAs") {}

    void help() override
    {
        log("\n");
        log("   %s [options]\n", STR(PASS_NAME));
        log("This command runs synthesis for RapidSilicon FPGAs\n");
        log("\n");
        log("    -verilog <file>\n");
        log("        Write the design to the specified verilog file. writing of an output file\n");
        log("        is omitted if this parameter is not specified.\n");
        log("\n");
        log("\n");
    }

    string module_name; 
    Technologies tech; 
    string verilog_file;
    int lut_cnt;
    int dff_cnt;
    int latch_cnt;
    int ram_cnt;
    int dsp_cnt;
    int IO_cnt;
    int PLL_cnt;
    
    std::vector<Cell *> DSP_Blocks;
    std::vector<Cell *> LUTs;
    std::vector<Cell *> LUTs_CLK;
    std::vector<Cell *> LUTs_NCLK;
    std::vector<Cell *> DFFs;
    std::vector<Wire *> IOs;
    std::vector<RTLIL::SigSpec> clocks;

    std::map<RTLIL::SigSpec,std::vector<float>>ce_ffs;
    std::map<RTLIL::SigSpec,std::vector<SigSpec>>clk_source;
    std::map<RTLIL::SigSpec,std::vector<SigSpec>>clk_driver;
    std::map<string,std::vector<string>>lut_clk;
    std::map<string,string>Lut_clocks;

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
        string clke_strategy_str;
        clear_flags();
        int max_device_bram = -1;
        int max_device_dsp = -1;
        int max_device_carry_length = -1;
        _design = design;

        size_t argidx;
        for (argidx = 1; argidx < args.size(); argidx++) {
            if (args[argidx] == "-verilog" && argidx + 1 < args.size()) {
                verilog_file = args[++argidx];
                continue;
            }
            break;
        }
        extra_args(args, argidx, design);

        if (!design->full_selection())
            log_cmd_error("This command only operates on fully selected designs!\n");

        log_header(design, "Executing synth_pow pass: v%d.%d.%d\n", 
            VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
        log_push();

        run_script(design, run_from, run_to);

        log_pop();
    }
    void check_dff(){
        for (auto dff : DFFs){
            RTLIL::SigSpec dff_clk;
            clocks.push_back(dff->getPort(ID::C));

            if (dff->getPort(ID::E) == RTLIL::S1)
                ce_ffs[dff->getPort(ID::C)].push_back(1);
            else if (dff->getPort(ID::E) == RTLIL::S0)
                ce_ffs[dff->getPort(ID::C)].push_back(0);
            else 
                ce_ffs[dff->getPort(ID::C)].push_back(0.5);

            if (!dff->getPort(ID::D).is_fully_const())
                clk_source[dff->getPort(ID::C)].push_back(dff->getPort(ID::D));
            if (!dff->getPort(ID::E).is_fully_const())
                clk_source[dff->getPort(ID::C)].push_back(dff->getPort(ID::E));
            if (!dff->getPort(ID::R).is_fully_const())
                clk_source[dff->getPort(ID::C)].push_back(dff->getPort(ID::E));
            if (!dff->getPort(ID::Q).is_fully_const())
                clk_driver[dff->getPort(ID::C)].push_back(dff->getPort(ID::Q));
            
        }

        auto unique_clk = std::unique(clocks.begin(), clocks.end());
        clocks.erase(unique_clk, clocks.end());
        
        for (auto ce_ff: ce_ffs){
            float sum = std::accumulate(ce_ff.second.begin(), ce_ff.second.end(), 0.0);
            log("Key = %s Sum = %f Result = %f\n",log_signal(ce_ff.first),sum, sum/ce_ff.second.size());
        }
    }
    int countOnesInBinary(int n) {
        int count = 0;
        while (n > 0) {
            if (n & 1)
                count++;
            n >>= 1; // Right shift to check the next bit
        }
        return count;
    }
    
    bool check_LUT_src_drv(RTLIL::SigSpec lut_port, std::string &clk){
        for (auto clk_src : clk_source){
            for (auto src : clk_src.second){
                if (lut_port == src){
                    clk = log_signal(clk_src.first);
                    return true;
                }
            }
        }
        for (auto clk_drive : clk_driver){
            for (auto drive : clk_drive.second){
                if (lut_port == drive){
                    clk = log_signal(clk_drive.first);
                    return true;
                }
            }
        }
        return false;
    }
    
    bool LUT2_iter(string port_id, RTLIL::SigSpec chunk_A,string &lut_id){
        for (auto lut_clk:LUTs_CLK){
            RTLIL::SigSpec Port1;
            RTLIL::SigSpec Port2;
            if (port_id == "A"){
                Port1 = lut_clk->getPort(ID::A);
            }
            else{
                Port1 = lut_clk->getPort(ID::Y);
            }
            
            if (!(Port1.is_chunk())){
                std::vector<SigChunk> chunks_Y = (lut_clk->getPort(ID::Y));
                for (auto chunk_Y : chunks_Y){
                    RTLIL::SigSpec chunkY = chunk_Y;
                    if (chunk_A == chunkY){
                        lut_id = log_id(lut_clk->name);
                        return true;
                    }
                }
            }
            else{
                RTLIL::SigSpec lut_a = chunk_A;
                if (lut_a == Port1){
                    lut_id = log_id(lut_clk->name);
                    return true;  
                }
            }
        }
        return false;
    }

    bool check_LUT_niteration (RTLIL::Cell *lut,string &lut_id){
        bool nck_found = false;
        if (!(lut->getPort(ID::A).is_chunk())){
            std::vector<SigChunk> chunks_A = (lut->getPort(ID::A));
            for (auto chunk_A : chunks_A){
                RTLIL::SigSpec chunkA = chunk_A;
                if (LUT2_iter("A",chunkA,lut_id)) 
                    return true;
                else{
                    if (LUT2_iter("Y",chunkA,lut_id)) 
                        return true;
                }
            }
        }
        else{
            if (LUT2_iter("A",lut->getPort(ID::A),lut_id)) 
                return true;
            else {
                if (LUT2_iter("Y",lut->getPort(ID::A),lut_id))
                    return true;
            }
        }
        
        if (!(lut->getPort(ID::Y).is_chunk())){
            std::vector<SigChunk> chunks_Y = (lut->getPort(ID::Y));
            for (auto chunk_Y : chunks_Y){
                RTLIL::SigSpec chunkY = chunk_Y;
                if (LUT2_iter("A",chunkY,lut_id)) 
                    return true;
                else{
                    if (LUT2_iter("Y",chunkY,lut_id)) 
                        return true;
                }
            }
        }
        else{
            if (LUT2_iter("A",lut->getPort(ID::Y),lut_id)) 
                return true;
            else {
                if (LUT2_iter("Y",lut->getPort(ID::Y),lut_id))
                    return true;
            }
        }
        return false;
    }
    // SigMap sigmap1(_design->top_module());
    string check_glitch_factor(RTLIL::Cell *lut){
        RTLIL::Const lut_init = lut->getParam(RTLIL::escape_id("INIT_VALUE"));
        int one_cnt = countOnesInBinary(lut_init.as_int());
        int length = GetSize(lut_init);
        if (one_cnt > 0) {
            if ((one_cnt > length * .45) && (one_cnt < length * .55) && (length>16)) {
                return  "Very_High";
            } else if ((one_cnt > length * .35) && (one_cnt < length * .75) && (length>16)) {
                return "High";
            }
            else
                return "Typical";
        }
        else
            return "Typical";
    }

    void check_LUT(){
        int found_lut = 0;
        string clk_lut = "";
        string glitch_factor = "";
        SigMap sigmap(_design->top_module());
        for (auto lut : LUTs){
            bool found_clk_for_lut = false;
            if (!(lut->getPort(ID::A).is_chunk())){
                std::vector<SigChunk> chunks_A = sigmap(lut->getPort(ID::A));
                for (auto chunk_A : chunks_A){
                    RTLIL::SigSpec src_lut = chunk_A;
                    found_clk_for_lut = check_LUT_src_drv(src_lut,clk_lut);
                    if (found_clk_for_lut){
                        glitch_factor = check_glitch_factor(lut);
                        break;
                    }
                }
            }
            else{
                found_clk_for_lut = check_LUT_src_drv(lut->getPort(ID::A),clk_lut);
                if (found_clk_for_lut){
                    glitch_factor = check_glitch_factor(lut);
                }
            }
            if (!(lut->getPort(ID::Y).is_chunk()) && found_clk_for_lut == false){
                std::vector<SigChunk> chunks_Y = sigmap(lut->getPort(ID::Y));
                for (auto chunk_Y : chunks_Y){
                    RTLIL::SigSpec src_lut = chunk_Y;
                    found_clk_for_lut = check_LUT_src_drv(src_lut,clk_lut);
                    if (found_clk_for_lut){
                        glitch_factor = check_glitch_factor(lut);
                        break;
                    }
                }
            }
            else if (found_clk_for_lut == false){
                found_clk_for_lut = check_LUT_src_drv(lut->getPort(ID::Y),clk_lut);
                if (found_clk_for_lut){
                    glitch_factor = check_glitch_factor(lut);
                }
            }

            if (!found_clk_for_lut) LUTs_NCLK.push_back(lut);
            else {
                LUTs_CLK.push_back(lut);
                lut_clk[clk_lut].push_back(glitch_factor);
                string ck = log_id(lut->name);
                Lut_clocks[ck] = clk_lut;                
                found_lut++;
            }
        }
        for (auto lut_nclk:LUTs_NCLK){
            string lut_id = "";
            bool lut_found = check_LUT_niteration(lut_nclk,lut_id);
            if (lut_found){
                glitch_factor = check_glitch_factor(lut_nclk);
                lut_clk[clk_lut].push_back(glitch_factor);
            }
            else{
                glitch_factor = check_glitch_factor(lut_nclk);
                lut_clk["unknown"].push_back(glitch_factor);
            }
            
        }
        for (const auto& pair : lut_clk) {
            const std::vector<std::string>& glitches = pair.second;
            std::unordered_map<std::string, int> elementCounts;
            for (const std::string& str : glitches) {
                elementCounts[str]++;
            }
            log("Clock: %s\nLut Glitch:\n",pair.first.c_str());
            for (const auto& element : elementCounts) {
                std::cout << "\t"<<element.first << ": " << element.second << std::endl;
            }
            std::cout << std::endl;
        }
    }
    // void check_IOs(){
    //     for (auto wire : IOs){

    //     }
    // }

    void script() override
    {
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
                    readArgs = GET_FILE_PATH(GENESIS_3_DIR, SIM_LIB_FILE) 
                                GET_FILE_PATH(GENESIS_3_DIR, LLATCHES_SIM_FILE)
                                GET_FILE_PATH(GENESIS_3_DIR, DSP_SIM_LIB_FILE)
                                GET_FILE_PATH_RS_LUTx_PRIMITVES(GENESIS_3_DIR, LUTx_SIM_FILE)
                                GET_FILE_PATH(GENESIS_3_DIR, BRAMS_SIM_LIB_FILE);
                    break;
                }    
                // Just to make compiler happy
                case Technologies::GENERIC: {
                    break;
                }    
            }
            // run("read_verilog -lib -specify -nomem2reg" GET_FILE_PATH(COMMON_DIR, SIM_LIB_FILE) + readArgs);
            
            
            for (auto &module : _design->selected_modules()) {
                module_name = log_id(module->name);
                for (auto wire : module->wires()){
                    if (wire->port_input){
                        IO_cnt++;
                        IOs.push_back(wire);
                        // IO
                    }
                    if (wire->port_output){
                        IO_cnt++;
                        IOs.push_back(wire);
                    }
                }
                for (auto &cell : module->selected_cells()) {
                    if (cell->type == RTLIL::escape_id("RS_DSP_MULT") \
                        || cell->type == RTLIL::escape_id("RS_DSP_MULT_REGIN") \
                        || cell->type == RTLIL::escape_id("RS_DSP_MULT_REGOUT") \
                        || cell->type == RTLIL::escape_id("RS_DSP_MULT_REGIN_REGOUT") ){
                        DSP_Blocks.push_back(cell);
                        dsp_cnt++;
                        continue;
                    }
                    if (cell->type == RTLIL::escape_id("$lut") \
                        || cell->type == RTLIL::escape_id("LUT1") \
                        || cell->type == RTLIL::escape_id("LUT2") \
                        || cell->type == RTLIL::escape_id("LUT3") \
                        || cell->type == RTLIL::escape_id("LUT4") \
                        || cell->type == RTLIL::escape_id("LUT5") \
                        || cell->type == RTLIL::escape_id("LUT6") ){
                        LUTs.push_back(cell);
                        lut_cnt++;
                        continue;
                    }
                    if (cell->type == RTLIL::escape_id("DFFRE") \
                        || cell->type == RTLIL::escape_id("DFFNRE")) {
                        dff_cnt++;
                        DFFs.push_back(cell);
                        continue;
                    }
                    // log("Cell = %s  Module = %s\n",log_id(cell->name),log_id(cell->type));
                }
            }
            check_dff();
            check_LUT();
            log("No. of LUT = %d\nNo. of DSP = %d\nNo. of DFF = %d\nNo. of IOs = %d\n",lut_cnt,dsp_cnt,dff_cnt,IO_cnt);
        }

    }

} SynthPowerRapidSiliconPass;

PRIVATE_NAMESPACE_END
