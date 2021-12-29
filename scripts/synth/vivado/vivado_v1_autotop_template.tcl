set_param general.maxThreads 1
add_files -norecurse ${BENCHMARK_RUN_DIR}

if {[glob -nocomplain -directory "${BENCHMARK_RUN_DIR}" "*.vh"] != ""} {
  set_property is_global_include true [get_files [glob -nocomplain -directory "${BENCHMARK_RUN_DIR}" "*.vh"]] 
}
if {[glob -nocomplain -directory "${BENCHMARK_RUN_DIR}" "*.svh"] != ""} {
  set_property is_global_include true [get_files [glob -nocomplain -directory "${BENCHMARK_RUN_DIR}" "*.svh"]] 
}

set topVar [lindex [find_top] 0]

synth_design -top [lindex [find_top] 0] \
    -part xc7a100tfgg676-1 \
    -flatten_hierarchy rebuilt \
    -gated_clock_conversion off \
    -bufg 12 \
    -directive AreaOptimized_high \
    -fanout_limit 400 \
    -no_lc \
    -fsm_extraction auto \
    -keep_equivalent_registers \
    -resource_sharing off \
    -cascade_dsp auto \
    -control_set_opt_threshold auto \
    -max_bram 0 \
    -max_uram 0 \
    -max_dsp 0 \
    -max_bram_cascade_height 0 \
    -max_uram_cascade_height 0 \
    -shreg_min_size 5

set opt_att "Default"
set place_att "Default"
set route_att "Default"

report_utilization -file "${BENCHMARK_RUN_DIR}/util_temp_[subst $topVar]_vivado_synth.log"
report_timing_summary -file "${BENCHMARK_RUN_DIR}/timing_temp_[subst $topVar]_vivado_synth.log" -delay_type min_max -report_unconstrained -check_timing_verbose -max_paths 10000 -input_pins -routable_nets
report_power -file "${BENCHMARK_RUN_DIR}/power_temp_[subst $topVar]_vivado_synth.log"
