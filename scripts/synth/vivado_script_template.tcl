set_param general.maxThreads 1

add_files -norecurse ${BENCHMARK_RUN_DIR}
#read_xdc $::env(VIVADO_SDC_GEN)

#if {[glob -nocomplain -directory "$::env(CGA_ROOT)/$::env(DESIGN_DIR)" "*.vh"] != ""} {
#  set_property is_global_include true [get_files [glob -nocomplain -directory "$::env(CGA_ROOT)/$::env(DESIGN_DIR)" "*.vh"]] 
#}
#if {[glob -nocomplain -directory "$::env(CGA_ROOT)/$::env(DESIGN_DIR)" "*.svh"] != ""} {
#  set_property is_global_include true [get_files [glob -nocomplain -directory "$::env(CGA_ROOT)/$::env(DESIGN_DIR)" "*.svh"]] 
#}

set VIVADO_PART "xc7a100tfgg676-1"

######################################################
#               STAGE-1 Synthesis
######################################################

#if {$::env(STRATEGY)=="performance"} {
  synth_design -top ${TOP_MODULE} -part xc7a100tfgg676-1 \
          -flatten_hierarchy rebuilt \
          -gated_clock_conversion off \
          -bufg 12 \
          -directive default \
          -fanout_limit 400 \
          -no_lc \
          -fsm_extraction one_hot \
          -keep_equivalent_registers\
          -resource_sharing off \
          -cascade_dsp auto \
          -control_set_opt_threshold auto \
          -max_bram -1 \
          -max_uram -1 \
          -max_dsp -1 \
          -max_bram_cascade_height -1 \
          -max_uram_cascade_height -1 \
          -shreg_min_size 5
    set opt_att "Explore"
    set place_att "Explore"
    set route_att "Explore"
#} elseif {$::env(STRATEGY)=="area"} {
#  synth_design -top $::env(DESIGN_TOP) -part $::env(VIVADO_PART) \
#        -flatten_hierarchy rebuilt \
#        -gated_clock_conversion off \
#        -bufg 12 \
#        -directive AreaOptimized_high \
#        -fanout_limit 10000 \
#        -fsm_extraction auto \
#        -resource_sharing auto \
#        -cascade_dsp auto \
#        -control_set_opt_threshold 1 \
#        -max_bram -1 \
#        -max_uram -1 \
#        -max_dsp -1 \
#        -max_bram_cascade_height -1 \
#        -max_uram_cascade_height -1 \
#        -shreg_min_size 3
#    set opt_att "ExploreArea"
#    set place_att "default"
#    set route_att "default"
#} else {
#  synth_design -top $::env(DESIGN_TOP) -part $::env(VIVADO_PART) \
#        -flatten_hierarchy rebuilt \
#        -gated_clock_conversion off \
#        -bufg 12 \
#        -directive default \
#        -fanout_limit 10000 \
#        -fsm_extraction auto \
#        -resource_sharing auto \
#        -cascade_dsp auto \
#        -control_set_opt_threshold auto \
#        -max_bram -1 \
#        -max_uram -1 \
#        -max_dsp -1 \
#        -max_bram_cascade_height -1 \
#        -max_uram_cascade_height -1 \
#        -shreg_min_size 3
#    set opt_att "default"
#    set place_att "default"
#    set route_att "default"
#}

#   Reports files after synth_design    #

report_utilization -file ${BENCHMARK_RUN_DIR}/util_temp_${TOP_MODULE}_vivado_synth.log
report_timing_summary -file ${BENCHMARK_RUN_DIR}/timing_temp_${TOP_MODULE}_vivado_synth.log -delay_type min_max -report_unconstrained -check_timing_verbose -max_paths 10000 -input_pins -routable_nets
report_power -file ${BENCHMARK_RUN_DIR}/power_temp_${TOP_MODULE}_vivado_synth.log
#write_checkpoint -force $::env(VIVADO_GEN_LOG_PATH)/post_synth


