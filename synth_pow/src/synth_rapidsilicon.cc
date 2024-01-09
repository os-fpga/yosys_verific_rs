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
#include <numeric>
#include <algorithm>
#include <chrono>
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
#define DSP_SIM_38 DSP38.v
#define DSP_SIM_LIB_FILE dsp_sim.v
#define LLATCHES_SIM_FILE llatches_sim.v
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
#define DSP_SIM_LIB_FILE_RS_PRIMITIVE(tech_dir,file) " +/rapidsilicon/" STR(tech_dir) "/RS_PRIMITIVES/sim_models/verilog/" STR(file)
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
    
    std::vector<Cell *> DSP_Blocks;
    std::vector<Cell *> LUTs;
    std::vector<Cell *> LUTs_CLK;
    std::vector<Cell *> LUTs_NCLK;
    std::vector<Cell *> DSP_38;
    std::vector<Cell *> TDP36K;
    std::vector<Cell *> DFFs;
    std::vector<Cell *> IBUFs;
    std::vector<Cell *> OBUFs;
    std::vector<Cell *> CLK_BUF;
    std::vector<Wire *> IOs;
    std::vector<RTLIL::SigSpec> clocks;
    std::map<SigSpec, SigSpec> clk_from_buffer;
    std::map<SigSpec, string> io_clk;
    std::map<SigSpec, string> io_with_clk;
    std::vector<std::tuple<int, string, string, string, string>> ios_out;
    std::map<RTLIL::SigSpec,std::vector<float>>ce_ffs;
    std::map<RTLIL::SigSpec,std::vector<SigSpec>>clk_source;
    std::map<RTLIL::SigSpec,std::vector<SigSpec>>clk_driver;
    std::map<string,std::vector<string>>lut_clk;
    std::map<string,string>Lut_clocks;
    std::map<string,std::vector<int>>dsp_clock;
    std::map<string,std::vector<string>> bram_out;
    
    std::map<std::tuple<std::string, SigSpec, SigSpec, int, int, float, float, float, float>, int> tdp_out;
    std::map<std::tuple<IdString, string, int, int>, int> dsp_out;

    std::map<string, double> sdc_clks;
    std::map<string, double> clk_out;
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
                clk_source[dff->getPort(ID::C)].push_back(dff->getPort(ID::R));
            if (!dff->getPort(ID::Q).is_fully_const())
                clk_driver[dff->getPort(ID::C)].push_back(dff->getPort(ID::Q));
            
        }
        auto unique_clk = std::unique(clocks.begin(), clocks.end());
        clocks.erase(unique_clk, clocks.end());
    }
    
    bool check_LUT_src_drv(RTLIL::SigSpec lut_port, std::string &clk, SigSpec lut_port_A){
        for (auto clk_src : clk_source){
            for (auto src : clk_src.second){
                if (lut_port == src){
                    clk = log_signal(clk_src.first);
                    if (!(lut_port_A.is_chunk())){
                        std::vector<SigChunk> chunks_A = lut_port_A;
                        for (auto chunk_A : chunks_A){
                            RTLIL::SigSpec chunkA = chunk_A;
                            clk_source[clk_src.first].push_back(chunkA);
                        }
                    }
                    return true;
                }
            }
        }
        for (auto clk_drive : clk_driver){
            for (auto drive : clk_drive.second){
                if (lut_port == drive){
                    clk = log_signal(clk_drive.first);
                    if (!(lut_port_A.is_chunk())){
                        std::vector<SigChunk> chunks_A = lut_port_A;
                        for (auto chunk_A : chunks_A){
                            RTLIL::SigSpec chunkA = chunk_A;
                            clk_source[clk_drive.first].push_back(chunkA);
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }
    
    bool LUT2_iter(string port_id, RTLIL::SigSpec chunk_A,string &lut_id){
        for (auto lut_clk1:LUTs_CLK){
            RTLIL::SigSpec Port1;
            if (port_id == "A"){
                Port1 = lut_clk1->getPort(ID::A);
            }
            else{
                Port1 = lut_clk1->getPort(ID::Y);
            }
            
            if (!(Port1.is_chunk())){
                std::vector<SigChunk> chunks_Y = (lut_clk1->getPort(ID::Y));
                for (auto chunk_Y : chunks_Y){
                    RTLIL::SigSpec chunkY = chunk_Y;
                    if (chunk_A == chunkY){
                        lut_id = log_id(lut_clk1->name);
                        return true;
                    }
                }
            }
            else{
                RTLIL::SigSpec lut_a = chunk_A;
                if (lut_a == Port1){
                    lut_id = log_id(lut_clk1->name);
                    return true;  
                }
            }
        }
        return false;
    }

    bool check_LUT_niteration (RTLIL::Cell *lut,string &lut_id){
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
                    found_clk_for_lut = check_LUT_src_drv(src_lut,clk_lut,lut->getPort(ID::A));
                    if (found_clk_for_lut){
                        glitch_factor = check_glitch_factor(lut);
                        break;
                    }
                }
            }
            else{
                found_clk_for_lut = check_LUT_src_drv(lut->getPort(ID::A),clk_lut,lut->getPort(ID::A));
                if (found_clk_for_lut){
                    glitch_factor = check_glitch_factor(lut);
                }
            }
            if (!(lut->getPort(ID::Y).is_chunk()) && found_clk_for_lut == false){
                std::vector<SigChunk> chunks_Y = sigmap(lut->getPort(ID::Y));
                for (auto chunk_Y : chunks_Y){
                    RTLIL::SigSpec src_lut = chunk_Y;
                    found_clk_for_lut = check_LUT_src_drv(src_lut,clk_lut, lut->getPort(ID::A));
                    if (found_clk_for_lut){
                        glitch_factor = check_glitch_factor(lut);
                        break;
                    }
                }
            }
            else if (found_clk_for_lut == false){
                found_clk_for_lut = check_LUT_src_drv(lut->getPort(ID::Y),clk_lut,lut->getPort(ID::A));
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

        auto start = std::chrono::high_resolution_clock::now();
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
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration =  std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        log("Time taken by LUTs with no clock = %fs\n",duration.count());
    }
    
    string dsp_38_no_clk(SigSpec _port_, SigSpec &_clk_){
        string clk = "unknown";
        for (auto clk_src : clk_source){
            for (auto src : clk_src.second){
                if (GetSize(_port_)>1){
                    for(int i = 0 ; i<=GetSize(_port_)-1; i++){
                        if (_port_[i] == src){
                            _clk_ = clk_src.first;
                            return log_signal(clk_src.first);
                        }
                    }
                }
                else{
                    if (_port_ == src){
                        _clk_ = clk_src.first;
                        return log_signal(clk_src.first);                        
                    }
                }
            }
        }
        for (auto clk_drive : clk_driver){
            for (auto drive : clk_drive.second){
                if (GetSize(_port_)>1){
                    for(int i = 0 ; i<=GetSize(_port_)-1; i++){
                        if (_port_[i] == drive){
                            _clk_ = clk_drive.first;
                            return log_signal(clk_drive.first);
                        }
                    }
                }
                else{
                    if (_port_ == drive){
                        _clk_ = clk_drive.first;
                        return log_signal(clk_drive.first);
                    }
                }
            }
        }
        return clk;
    }

    void check_dsp38(){
        bool clk_found = false;
        string clk = "";
        SigSpec _clock_;
        for (auto dsp : DSP_38){
            string in_reg = (dsp->getParam(RTLIL::escape_id("INPUT_REG_EN"))).decode_string().c_str();
            string outreg = (dsp->getParam(RTLIL::escape_id("OUTPUT_REG_EN"))).decode_string().c_str();
            string mode = (dsp->getParam(RTLIL::escape_id("DSP_MODE"))).decode_string().c_str();
            if (in_reg == "FALSE" && outreg == "FALSE" && mode == "MULTIPLY") clk_found = false;
            else clk_found = true;
            int a_size = check_port_width(dsp->getPort(ID::A));
            int b_size = check_port_width(dsp->getPort(ID::B));
            if (clk_found){           
                if (!dsp->getPort(ID::CLK).empty()){
                    clk_found = true;
                    update_clk_src_driver(dsp->getPort(RTLIL::escape_id("Z")), false, dsp->getPort(RTLIL::escape_id("CLK")));
                    update_clk_src_driver(dsp->getPort(RTLIL::escape_id("A")), true, dsp->getPort(RTLIL::escape_id("CLK")));
                    update_clk_src_driver(dsp->getPort(RTLIL::escape_id("B")), true, dsp->getPort(RTLIL::escape_id("CLK")));
                    clk = log_signal(dsp->getPort(ID::CLK));
                    dsp_clock[clk].push_back(a_size);
                    dsp_clock[clk].push_back(b_size);
                    clocks.push_back(clk);
                }
                else{
                    clk_found = false;
                    clk = dsp_38_no_clk(dsp->getPort(RTLIL::escape_id("Z")),_clock_);
                    if (clk == "unknown")
                         clk = dsp_38_no_clk(dsp->getPort(RTLIL::escape_id("A")),_clock_);
                    if (clk == "unknown")
                         clk = dsp_38_no_clk(dsp->getPort(RTLIL::escape_id("B")),_clock_);
                    dsp_clock[clk].push_back(a_size);
                    dsp_clock[clk].push_back(b_size);
                }
            }
            else {
                clk = dsp_38_no_clk(dsp->getPort(RTLIL::escape_id("Z")),_clock_);
                if (clk == "unknown")
                        clk = dsp_38_no_clk(dsp->getPort(RTLIL::escape_id("A")),_clock_);
                if (clk == "unknown")
                        clk = dsp_38_no_clk(dsp->getPort(RTLIL::escape_id("B")),_clock_);
                dsp_clock[clk].push_back(a_size);
                dsp_clock[clk].push_back(b_size);
            }
            std::tuple<RTLIL::IdString, string, int, int> elements;
            elements = std::make_tuple(dsp->type, clk, a_size, b_size);
            if (dsp_out.find(elements) != dsp_out.end()) {
                dsp_out[elements]++;
            } else {
                dsp_out[elements] = 1;
            }
        }
        auto unique_clk = std::unique(clocks.begin(), clocks.end());
        clocks.erase(unique_clk, clocks.end());
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

    void update_clk_src_driver(SigSpec _Port_, bool update_src, SigSpec clk){
        SigMap sigmap(_design->top_module());
        if (!(_Port_.is_chunk())){
            std::vector<SigChunk> chunks = sigmap(_Port_);
            for (auto chunk : chunks){
                RTLIL::SigSpec _sigspec_chunk_ = chunk;
                if (!_sigspec_chunk_.is_fully_const()){
                    if (update_src){
                        clk_source[clk].push_back(_sigspec_chunk_);
                    }
                    else{
                        clk_driver[clk].push_back(_sigspec_chunk_);
                    }
                }
            }
        }
        else{
            if (!_Port_.is_fully_const()){
                if (update_src){
                    clk_source[clk].push_back(_Port_);
                }
                else{
                    clk_driver[clk].push_back(_Port_);
                }
            }
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

                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_A1")), true, ram->getPort(RTLIL::escape_id("CLK_A1")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_A2")), true, ram->getPort(RTLIL::escape_id("CLK_A2")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_B1")), true, ram->getPort(RTLIL::escape_id("CLK_B1")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_B2")), true, ram->getPort(RTLIL::escape_id("CLK_B2")));

                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_A1")), true, ram->getPort(RTLIL::escape_id("CLK_A1")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_A2")), true, ram->getPort(RTLIL::escape_id("CLK_A2")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_B1")), true, ram->getPort(RTLIL::escape_id("CLK_B1")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_B2")), true, ram->getPort(RTLIL::escape_id("CLK_B2")));

                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_A1")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_A2")), false, ram->getPort(RTLIL::escape_id("CLK_A2")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_B1")), false, ram->getPort(RTLIL::escape_id("CLK_B1")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_B2")), false, ram->getPort(RTLIL::escape_id("CLK_B2")));

                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_A1")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_A2")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_B1")));
                    update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_B2")));

                    clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_A1")));
                    clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_A2")));
                    clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_B1")));
                    clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_B2")));

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

                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_A1")), true, ram->getPort(RTLIL::escape_id("CLK_A1")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_A2")), true, ram->getPort(RTLIL::escape_id("CLK_A2")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_B1")), true, ram->getPort(RTLIL::escape_id("CLK_B1")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_B2")), true, ram->getPort(RTLIL::escape_id("CLK_B2")));

                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_A1")), true, ram->getPort(RTLIL::escape_id("CLK_A1")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_A2")), true, ram->getPort(RTLIL::escape_id("CLK_A2")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_B1")), true, ram->getPort(RTLIL::escape_id("CLK_B1")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_B2")), true, ram->getPort(RTLIL::escape_id("CLK_B2")));

                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_A1")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_A2")), false, ram->getPort(RTLIL::escape_id("CLK_A2")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_B1")), false, ram->getPort(RTLIL::escape_id("CLK_B1")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_B2")), false, ram->getPort(RTLIL::escape_id("CLK_B2")));

                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_A1")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_A2")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_B1")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A1")), false, ram->getPort(RTLIL::escape_id("CLK_B2")));
                        
                        clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_A1")));
                        clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_A2")));
                        clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_B1")));
                        clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_B2")));

                        ram_type = "36k ";
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

                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_A")), true, ram->getPort(RTLIL::escape_id("CLK_A")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("ADDR_B")), true, ram->getPort(RTLIL::escape_id("CLK_B")));

                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_A")), true, ram->getPort(RTLIL::escape_id("CLK_A")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("BE_B")), true, ram->getPort(RTLIL::escape_id("CLK_B")));

                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_A")), false, ram->getPort(RTLIL::escape_id("CLK_A")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("RDATA_B")), false, ram->getPort(RTLIL::escape_id("CLK_B")));

                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A")), false, ram->getPort(RTLIL::escape_id("CLK_A")));
                        update_clk_src_driver(ram->getPort(RTLIL::escape_id("WDATA_A")), false, ram->getPort(RTLIL::escape_id("CLK_B")));
                        
                        clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_A")));
                        clocks.push_back(ram->getPort(RTLIL::escape_id("CLK_B")));

                        ram_type = "18kx2 ";
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
            std::tuple<std::string, SigSpec, SigSpec, int, int, float, float, float, float> elements;
            if (tech == Technologies::GENESIS_3)
                elements = std::make_tuple(ram_type, ram->getPort(RTLIL::escape_id("CLK_A")), ram->getPort(RTLIL::escape_id("CLK_B")), arwidth, brwidth, ena, enb, wena, wenb);
            else if (tech != Technologies::GENERIC)
                elements = std::make_tuple(ram_type, ram->getPort(RTLIL::escape_id("CLK_A1")), ram->getPort(RTLIL::escape_id("CLK_B1")), arwidth, brwidth, ena, enb, wena, wenb);
            if (tdp_out.find(elements) != tdp_out.end()) {
                tdp_out[elements]++;
            } else {
                tdp_out[elements] = 1;
            }
        }
        
        auto unique_clk = std::unique(clocks.begin(), clocks.end());
        clocks.erase(unique_clk, clocks.end());
        
        log("BRAM's : %ld\n",TDP36K.size());
        for (auto &bram : tdp_out){
            log("\t%s : %s %s : %d %d : %2f %2f : %2f %2f : %d\n", std::get<0>(bram.first).c_str(), log_signal(std::get<1>(bram.first)), log_signal(std::get<2>(bram.first)), std::get<3>(bram.first), std::get<4>(bram.first), std::get<5>(bram.first), std::get<6>(bram.first), std::get<7>(bram.first), std::get<8>(bram.first), bram.second);
        }
    }
    
    void check_IOs(){

        std::vector<std::tuple<int, string, string, string, string>> filteredVector;
        if (IBUFs.size()!=0){
            for (auto i_buf : IBUFs){
                for (auto clk : clocks){
                    if (CLK_BUF.size()==0){
                        if (i_buf->getPort(RTLIL::escape_id("O")) == clk){
                            clk_from_buffer[clk] = i_buf->getPort(RTLIL::escape_id("I"));
                            IBUFs.erase(remove(IBUFs.begin(),IBUFs.end(),i_buf));
                        }
                    }
                    else{
                        for (auto clk_buf : CLK_BUF){
                            if (clk_buf->getPort(RTLIL::escape_id("O")) == clk){
                                if (clk_buf->getPort(RTLIL::escape_id("I")) == i_buf->getPort(RTLIL::escape_id("O"))){
                                    clk_from_buffer[clk] = i_buf->getPort(RTLIL::escape_id("I"));
                                }
                            }
                        }
                    }
                }
            }
            string _clk_;
            SigSpec clk;
            string io_type = "SDR";
            for (auto i_buf : IBUFs){
                _clk_ = dsp_38_no_clk(i_buf->getPort(RTLIL::escape_id("O")),clk);
                io_clk[i_buf->getPort(RTLIL::escape_id("I"))] = _clk_;
            }
            for (auto o_buf : OBUFs){
                _clk_ = dsp_38_no_clk(o_buf->getPort(RTLIL::escape_id("I")),clk);
                io_clk[o_buf->getPort(RTLIL::escape_id("O"))] = _clk_;
            }
            for (auto wire : IOs){
                SigSpec _port_ = wire;
                for (auto io_clk_pair : io_clk){
                    for (auto prt : _port_){
                        if (io_clk_pair.first == prt){
                            for (auto clk_mapping : clk_from_buffer){
                                if (clk_mapping.second == prt){
                                    io_type = "Clock";
                                    break;
                                }
                            }
                            if (wire->port_input){
                                if (io_type == "Clock")
                                    filteredVector.push_back(make_tuple(GetSize(_port_), log_signal(_port_), "Input", io_type, log_signal(io_clk_pair.first)));
                                else
                                    filteredVector.push_back(make_tuple(GetSize(_port_), log_signal(_port_), "Input", io_type, io_clk_pair.second));
                            }else
                                filteredVector.push_back(make_tuple(GetSize(_port_), log_signal(_port_), "Output", io_type, io_clk_pair.second));
                            break;
                        }
                    }
                }
            }
        }
        else{
            string io_type = "SDR";
            string _clk_;
            bool clk_found = false;
            for (auto wire : IOs){
                SigSpec _port_ = wire;
                clk_found = false;
                for (auto _prt_ : _port_){
                    SigSpec clk;
                    _clk_ = dsp_38_no_clk(_prt_,clk);
                    if (_clk_ != "unknown"){
                        clk_found = true;
                        io_clk[_port_] = _clk_;
                        break;
                    }
                }
                if (clk_found == false)
                    io_clk[_port_] = "unknown";
                for (auto clk : clocks){
                    if (_port_ == clk){
                        io_type = "Clock";
                        break;
                    }
                }
                if (wire->port_input){
                    if (io_type == "Clock")
                        filteredVector.push_back(make_tuple(GetSize(_port_), log_signal(_port_), "Input", io_type, log_signal(_port_)));
                    else
                        filteredVector.push_back(make_tuple(GetSize(_port_), log_signal(_port_), "Input", io_type, _clk_));
                }else
                    filteredVector.push_back(make_tuple(GetSize(_port_), log_signal(_port_), "Output", io_type, _clk_));
                
                io_type = "SDR";
            }
        }
        std::set<std::tuple<int, string, string, string, string>> uniqueTuples;
        for (const auto& tuple : filteredVector) {
            if (uniqueTuples.insert(tuple).second) {
                ios_out.push_back(tuple);
            }
        }
        log("IOs: %ld\n",IOs.size());
        for (auto io_out : ios_out){
            log("\t%d %s %s %s %s\n",std::get<0>(io_out),std::get<1>(io_out).c_str(),std::get<2>(io_out).c_str(), std::get<3>(io_out).c_str(), std::get<4>(io_out).c_str());
        }
    }

    int sdc_parsing (string sdc_file){
        std::ifstream sdcFile(sdc_file); // Replace "input.sdc" with your SDC file name

        if (!sdcFile.is_open()) {
            log_warning("Error opening the SDC constraint file.");
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
                    for (auto clk : clocks){
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
                    if (clocks.empty()){
                        clk_out["unknown"] = 0;
                    }
                    else{
                        for (auto clk : clocks){
                            clk_out[log_signal(clk)] = 0;
                        }
                    }
                }
            }
        }
        else{
            if (clocks.empty()){
                clk_out["unknown"] = 0;
            }
            else{
                for (auto clk : clocks){
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
                {"", std::to_string(ram.second), std::get<0>(ram.first), log_signal(std::get<1>(ram.first)), log_signal(std::get<2>(ram.first)), \
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
                    for (auto clk : clocks){
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
                    if (clocks.empty()){
                        clk_out["unknown"] = 0;
                    }
                    else{
                        for (auto clk : clocks){
                            clk_out[log_signal(clk)] = 0;
                        }
                    }
                }
            }
        }
        else{
            if (clocks.empty()){
                clk_out["unknown"] = 0;
            }
            else{
                for (auto clk : clocks){
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
                {"Enabled", std::to_string(ram.second), std::get<0>(ram.first), log_signal(std::get<1>(ram.first)), log_signal(std::get<2>(ram.first)), \
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
                    readArgs =  GET_FILE_PATH(GENESIS_3_DIR, LLATCHES_SIM_FILE)
                                DSP_SIM_LIB_FILE_RS_PRIMITIVE(GENESIS_3_DIR, DSP_SIM_38);
                                // GET_FILE_PATH(GENESIS_3_DIR, BRAMS_SIM_LIB_FILE);
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
                    if (wire->port_input){
                        IO_cnt++;
                        IOs.push_back(wire);
                    }
                    if (wire->port_output){
                        IO_cnt++;
                        IOs.push_back(wire);
                    }
                }
                for (auto &cell : module->selected_cells()) {
                    if (cell->type == RTLIL::escape_id("RS_DSP_MULT") || cell->type == RTLIL::escape_id("RS_DSP_MULT_REGIN") || cell->type == RTLIL::escape_id("RS_DSP_MULT_REGOUT") || cell->type == RTLIL::escape_id("RS_DSP_MULT_REGIN_REGOUT") ){
                        DSP_Blocks.push_back(cell);
                        continue;
                    }
                    if (cell->type == RTLIL::escape_id("$lut") || cell->type == RTLIL::escape_id("LUT1") || cell->type == RTLIL::escape_id("LUT2") || cell->type == RTLIL::escape_id("LUT3") \
                        || cell->type == RTLIL::escape_id("LUT4") || cell->type == RTLIL::escape_id("LUT5") || cell->type == RTLIL::escape_id("LUT6") ){
                        LUTs.push_back(cell);
                        continue;
                    }
                    if (cell->type == RTLIL::escape_id("DFFRE") \
                        || cell->type == RTLIL::escape_id("DFFNRE")) {
                        DFFs.push_back(cell);
                        continue;
                    }
                    if (cell->type == RTLIL::escape_id("DSP38") || cell->type == RTLIL::escape_id("DSP19x2")) {
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
            check_dff();
            check_BRAM();
            check_dsp38();
            auto start = std::chrono::high_resolution_clock::now();
            check_LUT();
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration =  std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
            log("Time taken by LUTs = %fs\n",duration.count());
            check_IOs();
            sdc_parsing(sdc_str);
            gen_csv_old();

        }
    }

} PowerExtractRapidSilicon;
PRIVATE_NAMESPACE_END