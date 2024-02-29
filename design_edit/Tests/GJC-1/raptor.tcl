create_design flop2flop
target_device GEMINI_COMPACT_10x8
add_design_file flop2flop.v
#add_constraint_file flop2flop.sdc
ipgenerate
analyze
synth_options -inferred_io
synth
#packing
#place
#route
#sta

