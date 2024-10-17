/**
 * @file netlist_checker.cc
 * @author Behzad Mehmood (behzadmehmood82@gmail.com)
 * @author Manadher Kharroubi (manadher@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-09
 *
 * @copyright Copyright (c) 2024
 */
#include "netlist_checker.h"

std::string NETLIST_CHECKER::escaped_id(const std::string &input) {
  std::string result;
  result.reserve(input.size());
  for (char c : input) {
    if (c != '\\') {
      result.push_back(c); 
    }
  }
  return result;
}

void NETLIST_CHECKER::set_difference(const pool<SigBit>& set1,
  const pool<SigBit>& set2)
{
  for (auto &bit : set1)
  {
    if (!set2.count(bit))
    {
      diff.insert(bit);
    }
  }
}

void NETLIST_CHECKER::write_checker_file()
{
  std::ofstream netlist_checker_file("netlist_checker.log");
  if (netlist_checker_file.is_open())
  {
    netlist_checker_file << netlist_checker.str();
    netlist_checker_file.close();
  }

  netlist_checker.str("");
  netlist_checker.clear();
}

void NETLIST_CHECKER::gather_prims_data(Module* mod)
{
  for (auto cell : mod->cells())
  {
    if (cell->type == RTLIL::escape_id("I_BUF") ||
      cell->type == RTLIL::escape_id("I_BUF_DS"))
    {
      for (auto conn : cell->connections())
      {
        IdString portName = conn.first;
        for (SigBit bit : conn.second)
        {
          if (bit.wire != nullptr)
          {
            if (escaped_id(portName.str()) == "EN")
            {
              i_buf_ctrls.insert(bit);
            }
          }
        }
      }
    } else if (cell->type == RTLIL::escape_id("O_BUFT") ||
      cell->type == RTLIL::escape_id("O_BUFT_DS"))
    {
      for (auto conn : cell->connections())
      {
        IdString portName = conn.first;
        for (SigBit bit : conn.second)
        {
          if (bit.wire != nullptr)
          {
            if (escaped_id(portName.str()) == "T") 
            {
              o_buf_ctrls.insert(bit);
            }
          }
        }
      }
    } else if (cell->type == RTLIL::escape_id("FCLK_BUF"))
    {
      feedback_clocks++;
      if(feedback_clocks > 8)
        log_error("Feedback clock count exceeded, upto 8 feedback clocks are allowed.\n");
      for (auto conn : cell->connections())
      {
        IdString portName = conn.first;
        if(cell->input(portName))
        {
          for (SigBit bit : conn.second)
          {
            if (bit.wire != nullptr)
            {
              fclk_buf_ins.insert(bit);
            }
          }
        }
      }
    } else if (cell->type == RTLIL::escape_id("I_DELAY"))
    {
      for (auto conn : cell->connections())
      {
        IdString portName = conn.first;
        if(dly_controls.find(escaped_id(portName.str())) != dly_controls.end())
        {
          if(cell->input(portName))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) dly_in_ctrls.insert(bit);
          } else if(cell->output(portName))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) dly_out_ctrls.insert(bit);
          }
        }
      }
    } else if (cell->type == RTLIL::escape_id("O_DELAY"))
    {
      for (auto conn : cell->connections())
      {
        IdString portName = conn.first;
        if(dly_controls.find(escaped_id(portName.str())) != dly_controls.end())
        {
          if(cell->input(portName))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) dly_in_ctrls.insert(bit);
          } else if(cell->output(portName))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) dly_out_ctrls.insert(bit);
          }
        }
      }
    } else if (cell->type == RTLIL::escape_id("I_SERDES"))
    {
      for (auto conn : cell->connections())
      {
        IdString portName = conn.first;
        if(i_serdes_controls.find(escaped_id(portName.str())) != i_serdes_controls.end())
        {
          if(cell->input(portName))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) i_serdes_in_ctrls.insert(bit);
          } else if(cell->output(portName))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) i_serdes_out_ctrls.insert(bit);
          }
        }
      }
    } else if (cell->type == RTLIL::escape_id("O_SERDES"))
    {
      for (auto conn : cell->connections())
      {
        IdString portName = conn.first;
        if(o_serdes_controls.find(escaped_id(portName.str())) != o_serdes_controls.end())
        {
          if(cell->input(portName))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) o_serdes_in_ctrls.insert(bit);
          } else if(cell->output(portName))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) o_serdes_out_ctrls.insert(bit);
          }
        }
      }
    } else if (cell->type == RTLIL::escape_id("I_DDR") ||
      cell->type == RTLIL::escape_id("O_DDR"))
    {
      for (auto conn : cell->connections())
      {
        IdString portName = conn.first;
        if (portName == RTLIL::escape_id("R") ||
          portName == RTLIL::escape_id("E"))
        {
          for (SigBit bit : conn.second)
            if (bit.wire != nullptr) ddr_ctrls.insert(bit);
        }
      }
    }
  }
}

void NETLIST_CHECKER::check_idly_data_ins()
{
  netlist_checker << "\nChecking I_DELAY data inputs\n";
  netlist_checker << "================================================================\n";

  for (auto &bit : i_dly_ins)
  {
    if (!i_buf_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is input data signal of I_DELAY and must be an I_BUF(DS) output\n";
      netlist_error = true;
    }
  }
}

void NETLIST_CHECKER::check_odly_data_outs()
{
  netlist_checker << "\nChecking O_DELAY data outputs\n";
  netlist_checker << "================================================================\n";

  for (auto &bit : o_dly_outs)
  {
    if (!o_buf_ins.count(bit))
    {
      netlist_checker << log_signal(bit) << " is output data signal of O_DELAY and must be an O_BUF(T/DS) input\n";
      netlist_error = true;
    }
  }
}

void NETLIST_CHECKER::check_odly_data_ins()
{
  netlist_checker << "\nChecking O_DELAY data inputss\n";
  netlist_checker << "================================================================\n";

  for (auto &bit : o_dly_ins)
  {
    if (!fab_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is input data signal of O_DELAY and must be a fabric output\n";
      netlist_error = true;
    }
  }
}

void NETLIST_CHECKER::check_dly_cntrls()
{
  netlist_checker << "\nChecking I_DELAY/O_DELAY control signals\n";
  netlist_checker << "================================================================\n";
  for (auto &bit : dly_in_ctrls)
  {
    if (!fab_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is an input control signal and must be a fabric output\n";
      netlist_error = true;
    }
  }

  for (auto &bit : dly_out_ctrls)
  {
    if (!fab_ins.count(bit))
    {
      netlist_checker << log_signal(bit) << " is an output control signal and must be a fabric input\n";
      netlist_error = true;
    }
  }
  netlist_checker << "================================================================\n";
}

void NETLIST_CHECKER::check_ddr_cntrls()
{
  netlist_checker << "\nChecking I_DDR/O_DDR control signals\n";
  netlist_checker << "================================================================\n";
  for (auto &bit : ddr_ctrls)
  {
    if (!fab_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is an input control signal and must be a fabric output\n";
      netlist_error = true;
    }
  }

  netlist_checker << "================================================================\n";
}

void NETLIST_CHECKER::check_iddr_data_outs()
{
  netlist_checker << "\nChecking I_DDR data outputs\n";
  netlist_checker << "================================================================\n";

  for (auto &bit : i_ddr_outs)
  {
    if (!fab_ins.count(bit))
    {
      netlist_checker << log_signal(bit) << " is output data signal of I_DDR and must be a fabric input\n";
      netlist_error = true;
    }
  }
}

void NETLIST_CHECKER::check_oddr_data_ins()
{
  netlist_checker << "\nChecking O_DDR data inputss\n";
  netlist_checker << "================================================================\n";

  for (auto &bit : o_ddr_ins)
  {
    if (!fab_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is input data signal of O_DDR and must be a fabric output\n";
      netlist_error = true;
    }
  }
}

void NETLIST_CHECKER::check_iserdes_data_ins()
{
  netlist_checker << "\nChecking I_SERDES data inputs\n";
  netlist_checker << "================================================================\n";

  for (auto &bit : i_serdes_ins)
  {
    if (!i_dly_outs.count(bit) && !i_buf_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is input data signal of I_SERDES and must be an I_BUF/I_DELAY output\n";
      netlist_error = true;
    }
  }
}

void NETLIST_CHECKER::check_iserdes_data_outs()
{
  netlist_checker << "\nChecking I_SERDES data outputs\n";
  netlist_checker << "================================================================\n";

  for (auto &bit : i_serdes_outs)
  {
    if (!fab_ins.count(bit))
    {
      netlist_checker << log_signal(bit) << " is output data signal of I_SERDES and must be a fabric input\n";
      netlist_error = true;
    }
  }
}

void NETLIST_CHECKER::check_oserdes_data_ins()
{
  netlist_checker << "\nChecking O_SERDES data inputss\n";
  netlist_checker << "================================================================\n";

  for (auto &bit : o_serdes_ins)
  {
    if (!fab_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is input data signal of O_SERDES and must be a fabric output\n";
      netlist_error = true;
    }
  }
}

void NETLIST_CHECKER::check_oserdes_data_outs()
{
  netlist_checker << "\nChecking O_SERDES data outputss\n";
  netlist_checker << "================================================================\n";

  for (auto &bit : o_serdes_outs)
  {
    if (!o_buf_ins.count(bit) && !o_dly_ins.count(bit))
    {
      netlist_checker << log_signal(bit) << " is output data signal of O_SERDES and must be an O_DELAY/O_BUF input\n";
      netlist_error = true;
    }
  }
}


void NETLIST_CHECKER::check_serdes_cntrls()
{
  netlist_checker << "\nChecking I_SERDES/O_SERDES control signals\n";
  netlist_checker << "================================================================\n";
  for (auto &bit : i_serdes_in_ctrls)
  {
    if (!fab_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is an input control signal and must be a fabric output\n";
      netlist_error = true;
    }
  }

  for (auto &bit : i_serdes_out_ctrls)
  {
    if (!fab_ins.count(bit))
    {
      netlist_checker << log_signal(bit) << " is an output control signal and must be a fabric input\n";
      netlist_error = true;
    }
  }

  for (auto &bit : o_serdes_in_ctrls)
  {
    if (!fab_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is an input control signal and must be a fabric output\n";
      netlist_error = true;
    }
  }

  for (auto &bit : o_serdes_out_ctrls)
  {
    if (!fab_ins.count(bit))
    {
      netlist_checker << log_signal(bit) << " is an output control signal and must be a fabric input\n";
      netlist_error = true;
    }
  }
  netlist_checker << "================================================================\n";
}

void NETLIST_CHECKER::check_buf_cntrls()
{
  netlist_checker << "\nChecking Buffer control signals\n";
  netlist_checker << "================================================================\n";
  for (auto bit : i_buf_ctrls)
  {
    if (!fab_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is an input control signal and must be an output of fabric\n";
      netlist_error = true;
    }
  }

  for (auto bit : o_buf_ctrls)
  {
    if (!fab_outs.count(bit))
    {
      netlist_checker << log_signal(bit) << " is an input control signal and must be an output of fabric\n";
      netlist_error = true;
    }
  }
  netlist_checker << "================================================================\n";
}

void NETLIST_CHECKER::check_fclkbuf_conns()
{
  netlist_checker << "\nChecking FCLK_BUF connections\n";
  netlist_checker << "================================================================\n";
  set_difference(fclk_buf_ins, fab_outs);
  if(!diff.empty())
  {
    netlist_checker << "The following fclk_buf_outputs are not fabric outputs\n";
    for (const auto &elem : diff)
    {
      netlist_checker << "FCLK_BUF_IN : " << log_signal(elem) << "\n";
    }
    netlist_error = true;
    diff.clear();
  }
  netlist_checker << "================================================================\n";
}

void NETLIST_CHECKER::check_clkbuf_conns()
{
  set_difference(clk_buf_ins, i_buf_outs);
  if(!diff.empty())
  {
    netlist_checker << "================================================================\n";
    netlist_checker << "The following CLK_BUF inputs are not connected to I_BUF outputs\n";
    for (const auto &elem : diff)
    {
      netlist_checker << "CLK_BUF Input : " << log_signal(elem) << "\n";
    }
    netlist_checker << "================================================================\n";
    netlist_error = true;
  }

  diff.clear();
}

void NETLIST_CHECKER::check_buf_conns()
{
  netlist_checker << "Checking Buffer connections\n";
  if (design_inputs == i_buf_ins && design_outputs == o_buf_outs)
  {
    netlist_checker << "All IO connections are correct.\n";
    return;
  }

  diff.clear();
  set_difference(design_inputs, i_buf_ins);

  if(!diff.empty())
  {
    netlist_checker << "================================================================\n";
    netlist_checker << "The following inputs are not connected to I_BUFs\n";
    int i=0;
    for (const auto &elem : diff)
    {
      i++;
      netlist_checker << "Input : " << log_signal(elem) << "\n";
    }
    netlist_checker << "================================================================\n";
    netlist_error = true;
  }

  diff.clear();
  set_difference(i_buf_ins, design_inputs);
  if(!diff.empty())
  {
    netlist_checker << "================================================================\n";
    netlist_checker << "The following I_BUF inputs are not connected to the design inputs\n";
    for (const auto &elem : diff)
    {
      netlist_checker << "I_BUF Input : " << log_signal(elem) << "\n";
    }
    netlist_checker << "================================================================\n";
    netlist_error = true;
  }

  diff.clear();
  set_difference(design_outputs, o_buf_outs);
  if(!diff.empty())
  {
    netlist_checker << "================================================================\n";
    netlist_checker << "The following outputs are not connected to O_BUFs\n";
    for (const auto &elem : diff)
    {
      netlist_checker << "Output : " << log_signal(elem) << "\n";
    }
    netlist_checker << "================================================================\n";
    netlist_error = true;
  }

  diff.clear();
  set_difference(o_buf_outs, design_outputs);
  if(!diff.empty())
  {
    netlist_checker << "================================================================\n";
    netlist_checker << "The following O_BUF outputs are not connected to the design outputs\n";
    for (const auto &elem : diff)
    {
      netlist_checker << "O_BUF Output : " << log_signal(elem) << "\n";
    }
    netlist_checker << "================================================================\n";
    netlist_error = true;
  }

  diff.clear();
  return;
}

void NETLIST_CHECKER::gather_bufs_data(Yosys::RTLIL::Module* orig_mod)
{
  for (auto cell : orig_mod->cells())
  {
    string module_name = escaped_id(cell->type.str());
    if (std::find(prims.begin(), prims.end(), module_name) != prims.end())
    {
      if (cell->type == RTLIL::escape_id("I_BUF") ||
        cell->type == RTLIL::escape_id("I_BUF_DS"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          for (SigBit bit : conn.second)
          {
            if (bit.wire != nullptr)
            {
              if (cell->input(portName) && (escaped_id(portName.str()) != "EN"))
                i_buf_ins.insert(bit);
              if (cell->output(portName)) i_buf_outs.insert(bit);
            }
          }
        }
      } else if (cell->type == RTLIL::escape_id("O_BUF") ||
        cell->type == RTLIL::escape_id("O_BUF_DS"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          for (SigBit bit : conn.second)
          {
            if (bit.wire != nullptr)
            {
              if(cell->output(portName)) o_buf_outs.insert(bit);
              if (cell->input(portName)) o_buf_ins.insert(bit);
            }
          }
        }
      } else if (cell->type == RTLIL::escape_id("O_BUFT") ||
        cell->type == RTLIL::escape_id("O_BUFT_DS"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          for (SigBit bit : conn.second)
          {
            if (bit.wire != nullptr)
            {
              if(cell->output(portName)) o_buf_outs.insert(bit);
              if (portName == RTLIL::escape_id("I")) o_buf_ins.insert(bit);
            }
          }
        }
      } else if (cell->type == RTLIL::escape_id("CLK_BUF"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          if(cell->input(portName))
          {
            for (SigBit bit : conn.second)
            {
              if (bit.wire != nullptr)
              {
                clk_buf_ins.insert(bit);
              }
            }
          }
        }
      } else if (cell->type == RTLIL::escape_id("I_DELAY"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          if (portName == RTLIL::escape_id("I"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) i_dly_ins.insert(bit);
          }
          if (portName == RTLIL::escape_id("O"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) i_dly_outs.insert(bit);
          }
        }
      } else if (cell->type == RTLIL::escape_id("O_DELAY"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          if (portName == RTLIL::escape_id("I"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) o_dly_ins.insert(bit);
          }
          if (portName == RTLIL::escape_id("O"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) o_dly_outs.insert(bit);
          }
        }
      } else if (cell->type == RTLIL::escape_id("I_SERDES"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          if (portName == RTLIL::escape_id("D"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) i_serdes_ins.insert(bit);
          }
          if (portName == RTLIL::escape_id("Q"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) i_serdes_outs.insert(bit);
          }
        }
      } else if (cell->type == RTLIL::escape_id("O_SERDES"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          if (portName == RTLIL::escape_id("D"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) o_serdes_ins.insert(bit);
          }
          if (portName == RTLIL::escape_id("Q"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) o_serdes_outs.insert(bit);
          }
        }
      } else if (cell->type == RTLIL::escape_id("I_DDR"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          if (portName == RTLIL::escape_id("D"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) i_ddr_ins.insert(bit);
          }
          if (portName == RTLIL::escape_id("Q"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) i_ddr_outs.insert(bit);
          }
        }
      } else if (cell->type == RTLIL::escape_id("O_DDR"))
      {
        for (auto conn : cell->connections())
        {
          IdString portName = conn.first;
          if (portName == RTLIL::escape_id("D"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) o_ddr_ins.insert(bit);
          }
          if (portName == RTLIL::escape_id("Q"))
          {
            for (SigBit bit : conn.second)
              if (bit.wire != nullptr) o_ddr_outs.insert(bit);
          }
        }
      }
    }
  }
}
