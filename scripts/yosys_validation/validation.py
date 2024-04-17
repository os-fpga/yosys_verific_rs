from pathlib import Path
import argparse
import os
import re
import subprocess
import sys
import glob
import json
import csv
import shutil
import string

CRED = '\033[91m'
CGREEN = '\033[92m'
CEND = '\033[0m'

try:
    YS_ROOT = os.getcwd()
except:
    print ("YS_ROOT is not defined, Please export YS_ROOT=<path to the Yosys_Validation directory>")
    sys.exit()


parser = argparse.ArgumentParser()
parser.add_argument(
    "--test",
    type=str,
    help="Path for tools Input file",
    required=True
)
parser.add_argument(
    "--tech",
    type=str,
    choices=["genesis", "genesis2", "genesis3"],
    help="Specify technology for the run genesis|genesis2|genesis3",
    required=True
)
parser.add_argument(
    "--rtl",
    type=bool,
    help="Path for tools Input file",
    default=False
)
parser.add_argument(
    "--sim",
    type=bool,
    help="Path for tools Input file",
    default=False
)
args = parser.parse_args()
plugins = ""
flow = ""
if (args.rtl):
    flow = "rtl"
else:
    flow = "sim"
if (os.path.exists(YS_ROOT+"/../../suites/yosys_validation/"+args.test)):
    with open(YS_ROOT+"/../../suites/yosys_validation/"+args.test) as file:
        JSON_data = json.load(file)
else:
    print("[ERROR]: <"+YS_ROOT+"/../../suites/yosys_validation/"+args.test+"> file does not exist")
    sys.exit()
def get_template(template_file):
  with open(template_file) as read_job:
    template = string.Template(read_job.read())
    read_job.close()
  return template

def generate_task_conf(gen_file, final_output):
  with open(gen_file, 'w') as write_job:
    write_job.write(final_output)
    write_job.close()

def plugin(arch):
    if (arch == "genesis3"):
        plugins =   YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/LUT1.v " + \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/LUT2.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/LUT3.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/LUT4.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/LUT5.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/LUT6.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/DFFRE.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/DFFNRE.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/CARRY.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/DSP38.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/DSP19X2.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/TDP_RAM18KX2.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/FPGA_PRIMITIVES_MODELS/sim_models/verilog/TDP_RAM36K.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/simlib.v "
    else:
        plugins =  YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/cells_sim.v " + \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/dsp_sim.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/dsp_map.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/sram1024x18.v"+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/simlib.v "

        if flow == "rtl":
            plugins = plugins + "/nfs_scratch/scratch/FV/awais/Synthesis/v1/yosys_verific_rs/scripts/yosys_validation/file_list.sv"
        else:
            plugins = plugins + YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/brams_sim.v " + \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/TDP18K_FIFO.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/ufifo_ctl.v "+ \
                    YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/sram1024x18.v"
    return plugins

def _create_new_project_dir_():
    run_list = []
    if (os.path.exists(YS_ROOT+"/logs/")==0):
        os.makedirs(YS_ROOT+"/logs/run1")
        path = YS_ROOT+"/logs/run1"
    else:
        runid = glob.glob(YS_ROOT+'/logs/run*')
        if (len(runid)>0):
            for dir in runid:
                dir_splt=(dir.split("/"))[-1]
                run_list.append(int((dir_splt.split("n"))[-1]))
            new_dir = max(run_list)+1
            path = YS_ROOT+"/logs/run"+str(new_dir)
            os.makedirs(path)
        else:
            path = YS_ROOT+"/logs/run1"
            os.makedirs(path)

    return path

def execute(cmd,ofname,er_file,pt,test_):
    with open(ofname,'wb') as outpfile,open(ofname,'wb') as er_file:
        p=subprocess.Popen(cmd.split(),stdout=outpfile,stderr=er_file)
        try:
            print("Running "+pt+" for "+test_)
            os.waitpid(p.pid,0)
        except KeyboardInterrupt:
            print("Exception is taken placed")
            # print(os.kill(p.pid,signal.SIGKILL))
            os.wait()
            sys.exit()

def replace_string(file_,search_,replace_):
    # Read in the file
    with open(file_, 'r') as file :
        data = file.read()
        # Replace the target string
        data = data.replace(search_, replace_)

        # print(data)
        # Write the file out again
        with open(file_, 'w') as file:
            file.write(data)

def parse_log_backward(log_file):
    found_start = False
    result_lines = []
    synth_stat = ""

    with open(log_file, 'rb') as file:
        file.seek(0, 2)  # Seek to the end of the file
        file_size = file.tell()
        pointer = file_size - 1

        while pointer >= 0:
            file.seek(pointer)
            current_char = file.read(1)
            if current_char == b'\n':
                line = file.readline().decode('utf-8').strip()
                if "End of script." in line:
                    synth_stat = "Synthesis Succeeded"
                if "Number of LUTs:" in line:
                    found_start = True
                elif found_start and "Printing statistics." in line:
                    break  # We've found the end of the section
                elif found_start:
                    result_lines.insert(0, line)

            pointer -= 1

    return result_lines, synth_stat

def yosys_parser(PROJECT_NAME,raptor_log,synth_status,test_):
    stat = False ;next_command = False; dffsre = [];Data=[];DSP38 = [];DSP19X2 = []; BRAM36K=[];BRAM18K=[];_Luts_=[];Carry_cells=[]
    RS_DSP_MULT = 0
    RS_DSP_MULT_REGIN = 0
    RS_DSP_MULT_REGOUT = 0
    RS_DSP_MULT_REGIN_REGOUT = 0
    RS_DSP_MULTACC = 0
    RS_DSP_MULTACC_REGIN = 0
    RS_DSP_MULTACC_REGOUT = 0
    RS_DSP_MULTACC_REGIN_REGOUT = 0
    RS_DSP_MULTADD_REGIN = 0
    RS_DSP_MULTADD_REGIN_REGOUT = 0
    RS_DSP_MULTADD_REGOUT = 0
    RS_DSP_MULTADD = 0

    global run_synth_status
    run_synth_status = ""
    # print(raptor_log)
    with open(raptor_log,"r") as in_file:
        result, synth_status = parse_log_backward(raptor_log)
        # result = "\n".join(result)
        # print(result)
        for line in result:
            # print(line)
            try:
                if (re.search(r".*CARRY.*", line)):
                    Carry_cells.append(int(line.split()[1]))
            except:
                pass
            if ((re.search(r".*\$lut.*", line)) or (re.search(r".*LUT.*", line))):
                # print(line)
                _Luts_.append(int(line.split()[1]))
            
            if (re.search(r".*DSP38.*", line)):
                # print(line)
                DSP38.append(int(line.split()[1]))
            if (re.search(r".*DSP19X2.*", line)):
                # print(line)
                DSP19X2.append(int(line.split()[1]))

            if (re.search(r".*RS_DSP_MULT ", line)):
                # print(line)
                RS_DSP_MULT = int(line.split()[1])

            if (re.search(r".*RS_DSP_MULT_REGIN ", line)):
                # print(line)
                RS_DSP_MULT_REGIN = int(line.split()[1])

            if (re.search(r".*RS_DSP_MULT_REGOUT ", line)):
                # print(line)
                RS_DSP_MULT_REGOUT = int(line.split()[1])

            if (re.search(r".*RS_DSP_MULT_REGIN_REGOUT ", line)):
                # print(line)
                RS_DSP_MULT_REGIN_REGOUT = int(line.split()[1])

            if (re.search(r".*RS_DSP_MULTACC ", line)):
                # print(line)
                RS_DSP_MULTACC = int(line.split()[1])

            if (re.search(r".*RS_DSP_MULTACC_REGIN ", line)):
                # print(line)
                RS_DSP_MULTACC_REGIN = int(line.split()[1])
            
            if (re.search(r".*RS_DSP_MULTACC_REGOUT ", line)):
                # print(line)
                RS_DSP_MULTACC_REGOUT = int(line.split()[1])

            if (re.search(r".*RS_DSP_MULTACC_REGIN_REGOUT ", line)):
                # print(line)
                RS_DSP_MULTACC_REGIN_REGOUT = int(line.split()[1])

            if (re.search(r".*RS_DSP_MULTADD ", line)):
                # print(line)
                RS_DSP_MULTADD = int(line.split()[1])

            if (re.search(r".*RS_DSP_MULTADD_REGIN ", line)):
                # print(line)
                RS_DSP_MULTADD_REGIN = int(line.split()[1])
            
            if (re.search(r".*RS_DSP_MULTADD_REGOUT ", line)):
                # print(line)
                RS_DSP_MULTADD = int(line.split()[1])

            if (re.search(r".*RS_DSP_MULTADD_REGIN_REGOUT ", line)):
                # print(line)
                RS_DSP_MULTADD_REGIN = int(line.split()[1])

            if (re.search(r".*TDP_RAM18KX2", line)):
                # print(line)
                BRAM18K.append(int(line.split()[1]))
                
            if (re.search(r".*TDP_RAM36K", line)):
                # print(line)
                BRAM36K.append(int(line.split()[1]))

            if (re.search(r".*DFF.*", line)):
                # print(line)
                dffsre.append(int(line.split()[1]))
            

            # print(line__)

    if (args.tech == "genesis3"):
        Data = [PROJECT_NAME,str(sum(_Luts_)),str(sum(dffsre)),str(sum(Carry_cells)),str(sum(BRAM36K)),str(sum(BRAM18K)),str(sum(DSP38)),str(sum(DSP19X2)), synth_status]
    else:
        Data = [PROJECT_NAME,str(sum(_Luts_)),str(sum(dffsre)),str(sum(Carry_cells)),str(sum(BRAM36K)),str(sum(BRAM18K)),str(sum(DSP38)),str(sum(DSP19X2)),str(RS_DSP_MULT),str(RS_DSP_MULT_REGIN),str(RS_DSP_MULT_REGOUT), str(RS_DSP_MULT_REGIN_REGOUT), str(RS_DSP_MULTACC), str(RS_DSP_MULTACC_REGIN), str(RS_DSP_MULTACC_REGOUT), str(RS_DSP_MULTACC_REGIN_REGOUT), str(RS_DSP_MULTADD), str(RS_DSP_MULTADD_REGIN), str(RS_DSP_MULTADD_REGOUT), str(RS_DSP_MULTADD_REGIN_REGOUT), synth_status]
    
    print(CGREEN+synth_status+" for "+test_+CEND)
    return Data

def vcs_parse(sim_file,test_,Data):
    with open (sim_file, 'r') as sim_file:
        for line in sim_file:
            if (re.search(r"Simulation Passed.*", line) or re.search(r".*All Comparison Matched.*", line)):
                sim = True
                Data.append("Simulation Passed")
                print(CGREEN+"Simulation Passed for "+test_+CEND)
            if (re.search(r"Simulation Failed.*", line)):
                sim = False
                Data.append("Simulation Failed")
                print(CRED+"Simulation Failed for "+test_+CEND)

def compile(project_path,rtl_path,top_module,test_):
    synth_status = True
    if (os.path.exists(project_path+"/yosys.ys")==0):
        print("Cannot open "+project_path+"/yosys.ys")
    else:
        synth_cmd = YS_ROOT+"/../../"+JSON_data["yosys"]["yosys_path"]+" -s " + project_path+"/yosys.ys"
        try:
            execute(synth_cmd,"synth.log","err_synth.log","synthesis",test_)
        except Exception as _error_:
            print(str(_error_))
        netlist = project_path+"/"+top_module+"_post_synth.v"
        search_text = "module "+top_module
        replace_text = "module " + top_module+"_post_synth"
        if (os.path.exists(netlist)):
            replace_string(netlist,search_text,replace_text)
        else:
            print(CRED+"Synthesis Failed for "+test_+CEND)
            synth_status = False
    
    return synth_status

def simulate(project_path,rtl_path,top_module,test_):
    vcs_cmd = "vcs -sverilog " + project_path+"/"+top_module+"_post_synth.v " + YS_ROOT +"/../../" + rtl_path +"/"+top_module+".sv  " + YS_ROOT +"/../../" + rtl_path +"/tb.sv " + plugin(args.tech) + " -full64 -debug_all -kdb -lca -timescale=1ns/100ps +define+VCS_MODE=1"
    iverilog_cmd = "iverilog -g2012 -o " + top_module+"_ " + plugin(args.tech) + " " +YS_ROOT +"/../../" + rtl_path +"/"+top_module+".sv  " + project_path+"/"+top_module+"_post_synth.v " + YS_ROOT +"/../../" + rtl_path +"/tb.sv "
    #print(iverilog_cmd)
    if (glob.glob(YS_ROOT +"/../../" + rtl_path+"/*.mem")):
        mem_init = glob.glob(YS_ROOT +"/../../" + rtl_path+"/*.mem")
        for _file_ in mem_init:
            shutil.copy(_file_,project_path+"/")

    if (glob.glob(YS_ROOT +"/../../" + rtl_path+"/*.init")):
        mem_init = glob.glob(YS_ROOT +"/../../" + rtl_path+"/*.init")
        for _file_ in mem_init:
            shutil.copy(_file_,project_path+"/")
    if flow == "rtl":
        shutil.copy(YS_ROOT + "/bram.svh",project_path+"/")
    # print(vcs_cmd)
    try:
        execute(iverilog_cmd,"compile.log","err_compile.log","compilation",test_)
        execute("vvp ./"+top_module+"_" ,"sim.log","err_sim.log","simulation",test_)
        # execute("./simv","sim.log","err_sim.log","simulation",test_)
        execute("vcd2fst tb.vcd tb.fst --compress" ,"vcd_sim.log","err_vcd.log","VCD file compression",test_)
    except Exception as e:
        sim = False
        print (str(e))

def test():
    if (args.tech == "genesis3"):
        header = ['Design Name', 'LUTs','DFF','Carry Logic','TDP_RAM36K','TDP_RAM18KX2','DSP38', 'DSP19X2', "Synthesis Status", "Simulation Status"]
    else:
        header = ['Design Name', 'LUTs','DFF','Carry Logic','TDP_RAM36K','TDP_RAM18KX2','DSP38', 'DSP19X2','RS_DSP_MULT', 'RS_DSP_MULT_REGIN' ,'RS_DSP_MULT_REGOUT', 'RS_DSP_MULT_REGIN_REGOUT', 'RS_DSP_MULTACC', 'RS_DSP_MULTACC_REGIN', 'RS_DSP_MULTACC_REGOUT', 'RS_DSP_MULTACC_REGIN_REGOUT', 'RS_DSP_MULTADD', 'RS_DSP_MULTADD_REGIN', 'RS_DSP_MULTADD_REGOUT', 'RS_DSP_MULTADD_REGIN_REGOUT', "Synthesis Status", "Simulation Status"]
    CSV_File = open(path+"/results.csv",'w')
    writer = csv.writer(CSV_File)
    writer.writerow(header)
    CSV_File.close()
    Tool_settings_gen = "-de -goal delay -max_device_dsp 176 -max_device_bram 176 -max_device_carry_length 528 -max_dsp 176 -max_bram 176 -max_carry_length 528 -de_max_threads -1"
    Tool_settings_gen2 = "-de -goal delay -max_device_dsp 176 -max_device_bram 176 -max_device_carry_length 528 -max_dsp 176 -max_bram 176 -max_carry_length 528 -de_max_threads -1"
    Tool_settings_gen3 = "-de -goal delay -no_iobuf -new_tdp36k -new_dsp19x2 -max_device_dsp 500 -max_device_bram 500 -max_device_carry_length 528 -max_dsp 500 -max_bram 500 -max_carry_length 528 -de_max_threads -1"
    Tool_seting = ""
    for test in JSON_data["benchmarks"]:
        Data = []
        os.chdir(YS_ROOT)
        try:
            if JSON_data["benchmarks"][test]["compile_status"] == "active":
                project_path = os.path.join(path,test)
                rtl_path  = JSON_data["benchmarks"][test]["test_path"]
                top_module = JSON_data["benchmarks"][test]["top_module"]
                # os.makedirs(project_path)
                shutil.copytree("../../"+rtl_path,project_path)
                sim_model_blackbox = args.tech
                TASK_template = get_template(project_path+"/yosys.ys")
                if (args.tech == "genesis"):
                    Tool_seting = Tool_settings_gen
                if (args.tech == "genesis2"):
                    Tool_seting = Tool_settings_gen2
                    device = args.tech
                if (args.tech == "genesis3"):
                    Tool_seting = Tool_settings_gen3
                    device = args.tech+"/FPGA_PRIMITIVES_MODELS/blackbox_models"

                Gen_Template = TASK_template.substitute(ROOT_PATH = YS_ROOT+"/../../",RTL_PATH = rtl_path, ARCHITECTURE=device, TOP = top_module, SYNTH_SETTING = args.tech + " " + Tool_seting)
                generate_task_conf(project_path+"/yosys.ys", Gen_Template)
                os.chdir(project_path)
                synth = compile(project_path,rtl_path,top_module,test)
                if (synth == True):
                    try:
                        Data = yosys_parser(test,"synth.log","Synthesis Succeeded",test)
                    except Exception as e:
                        Data = [test,"N/A","N/A","N/A","N/A","N/A","Synthesis Failed"]
                        print(str(e))
            if ((JSON_data["benchmarks"][test]["compile_status"] == "active") and (JSON_data["benchmarks"][test]["sim_status"] == "active")) and synth == True and args.sim == True:
                simulate(project_path,rtl_path,top_module,test)
                try:
                    vcs_parse("sim.log",test,Data)
                except Exception as e:
                    Data.append("Cannot open ./simv file")
                    print(str(e))
            if JSON_data["benchmarks"][test]["compile_status"] == "active":
                with open(path+"/results.csv",'a') as csv_file:
                    writer = csv.writer(csv_file)
                    writer.writerow(Data)
                    print("\n================================================================================\n")
        except Exception as error:
            print(error)
# try:
#     shutil.rmtree(YS_ROOT+'/logs/')
# except Exception as e: 
#     print(str(e))
path = _create_new_project_dir_()
try:
    test()
except Exception as error:
    print(error)
