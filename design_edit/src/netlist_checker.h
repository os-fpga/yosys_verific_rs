#ifndef NETLIST_CHECKER_H
#define NETLIST_CHECKER_H

#include "backends/rtlil/rtlil_backend.h"
#include "kernel/celltypes.h"
#include "kernel/ff.h"
#include "kernel/ffinit.h"
#include "kernel/log.h"
#include "kernel/mem.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"

USING_YOSYS_NAMESPACE
using namespace RTLIL;

struct NETLIST_CHECKER {
  std::string escaped_id(const std::string &input);
  void set_difference(const pool<SigBit>& set1, const pool<SigBit>& set2);
  void write_checker_file();
  void gather_prims_data(Module* mod);
  void gather_fabric_data(Module* mod);
  void check_idly_data_ins();
  void check_odly_data_outs();
  void check_odly_data_ins();
  void check_dly_cntrls();
  void check_ddr_cntrls();
  void check_iddr_data_outs();
  void check_iddr_data_ins();
  void check_oddr_data_ins();
  void check_oddr_data_outs();
  void check_iserdes_data_ins();
  void check_iserdes_data_outs();
  void check_oserdes_data_ins();
  void check_oserdes_data_outs();
  void check_serdes_cntrls();
  void check_buf_cntrls();
  void check_fclkbuf_conns();
  void check_clkbuf_conns();
  void check_buf_conns();
  void gather_bufs_data(Yosys::RTLIL::Module* orig_mod);
  bool check_netlist();

  int feedback_clocks = 0;
  pool<SigBit> design_inputs, design_outputs;
  std::unordered_set<std::string> prims;
  pool<SigBit> i_buf_ins, i_buf_outs, o_buf_outs, i_buf_ctrls, o_buf_ctrls;
  pool<SigBit> clk_buf_ins, dly_in_ctrls, dly_out_ctrls;
  pool<SigBit> fclk_buf_ins, fab_outs, fab_ins, o_buf_ins;
  pool<SigBit> i_dly_ins, i_dly_outs, o_dly_ins, o_dly_outs;
  pool<SigBit> i_serdes_ins, i_serdes_outs, o_serdes_ins, o_serdes_outs;
  pool<SigBit> i_ddr_ins, i_ddr_outs, o_ddr_ins, o_ddr_outs;
  pool<SigBit> i_serdes_in_ctrls, i_serdes_out_ctrls;
  pool<SigBit> o_serdes_in_ctrls, o_serdes_out_ctrls, ddr_ctrls;
  std::unordered_set<std::string> i_serdes_controls =
      {"RST", "BITSLIP_ADJ", "EN", "DATA_VALID", "DPA_LOCK", "DPA_ERROR", "PLL_LOCK"};
  std::unordered_set<std::string> o_serdes_controls =
      {"RST", "DATA_VALID", "OE_IN", "OE_OUT", "CHANNEL_BOND_SYNC_IN", "CHANNEL_BOND_SYNC_OUT", "PLL_LOCK"};
  std::unordered_set<std::string> dly_controls =
      {"DLY_LOAD", "DLY_ADJ", "DLY_INCDEC", "DLY_TAP_VALUE"};
  pool<SigBit> diff;
  std::stringstream netlist_checker;
  bool netlist_error = false;
};

#endif