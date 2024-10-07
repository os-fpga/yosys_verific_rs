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
  void check_dly_cntrls();
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
  pool<SigBit> fclk_buf_ins, fab_outs, fab_ins;
  std::unordered_set<std::string> dly_controls =
      {"DLY_LOAD", "DLY_ADJ", "DLY_INCDEC", "DLY_TAP_VALUE"};
  pool<SigBit> diff;
  std::stringstream netlist_checker;
  bool netlist_error = false;
};

#endif