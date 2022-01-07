prj_project new -name ${BENCHMARK_NAME} -impl "${BENCHMARK_NAME}" -dev "LFE5U-85F-6BG756C" -synthesis "synplify"

set file_list {}
foreach vh_file [glob -nocomplain -directory ${BENCHMARK_RUN_DIR} "*.vh"] {
  set root_name [file rootname $vh_file]
  file rename -force -- $root_name.vh $root_name.v
  lappend file_list $root_name.v
}
foreach svh_file [glob -nocomplain -directory ${BENCHMARK_RUN_DIR} "*.svh"] {
  set root_name [file rootname $svh_file]
  file rename -force -- $root_name.svh $root_name.sv
  lappend file_list $root_name.sv
} 
foreach src_file [glob -nocomplain -directory ${BENCHMARK_RUN_DIR} "*.*v"] {
  if { ( $src_file ni $file_list ) && ( [file tail $src_file] != [file tail ${TOP_MODULE} ] ) } {
    lappend file_list $src_file
  } elseif { [file tail $src_file] == [file tail ${TOP_MODULE} ] } {
    set design_top $src_file 
  }    
}

lappend file_list $design_top

foreach design $file_list {
  prj_src add $design
}

prj_project save

prj_strgy copy -from "Area" -name "Area_opt" -file "Area_opt.sty"
prj_strgy set "Area_opt"
prj_strgy set_value -strategy Area_opt syn_use_lpf_file=False
prj_run Synthesis -impl "${BENCHMARK_NAME}" -task Synplify_Synthesis
prj_run Translate -impl "${BENCHMARK_NAME}"
prj_run Map -impl "${BENCHMARK_NAME}"
prj_run Map -impl "${BENCHMARK_NAME}" -task MapTrace
