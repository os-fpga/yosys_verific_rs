create_design ft
target_device 1GVTC
add_design_file ft.v
add_constraint_file ft.sdc
ipgenerate
analyze
synth
#packing
#place
#route
#sta

