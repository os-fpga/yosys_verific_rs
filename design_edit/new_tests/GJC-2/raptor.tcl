create_design flop2flop2flop
target_device 1GE100-ES1
add_design_file flop2flop2flop.v
#add_constraint_file flop2flop2flop.sdc
ipgenerate
analyze
synth
#packing
#place
#route
#sta

