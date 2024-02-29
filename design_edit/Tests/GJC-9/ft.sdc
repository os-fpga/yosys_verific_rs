set_top_module ft
# -name is used for creating virtual clock and for actual clock -name option will not be used
create_clock -period 5 -name clk
set_input_delay 1 -clock clk [get_ports {din}]
set_output_delay 1 -clock clk [get_ports {dout}]

# pin locations
set_property mode Mode_BP_SDR_A_RX HR_2_6_3P
set_pin_loc din HR_2_6_3P

set_property mode Mode_BP_SDR_A_TX HR_3_12_6P
set_pin_loc dout HR_3_12_6P
