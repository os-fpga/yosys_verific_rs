set_top_module flop2flop
# -name is used for creating virtual clock and for actual clock -name option will not be used
create_clock -period 5 clk
# set_clock_uncertainty 0.005 clk
set_input_delay 1 -clock clk [get_ports {din}]
set_output_delay 1 -clock clk [get_ports {dout}]

# pin locations
set_property mode Mode_BP_SDR_A_RX HR_3_0_0P
set_pin_loc din HR_3_0_0P

set_property mode Mode_BP_SDR_A_RX HR_2_0_0P
set_pin_loc clk HR_2_0_0P

set_property mode Mode_BP_SDR_A_TX HR_5_0_0P
set_pin_loc dout  HR_5_0_0P

