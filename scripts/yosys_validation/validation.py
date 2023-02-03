from pathlib import Path
import argparse
import os
import re
import subprocess
import sys
import glob
import json
import csv

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

args = parser.parse_args()

if (os.path.exists(YS_ROOT+"/../../suites/yosys_validation/"+args.test)):
    with open(YS_ROOT+"/../../suites/yosys_validation/"+args.test) as file:
        JSON_data = json.load(file)
else:
    print("[ERROR]: <"+YS_ROOT+"/../../suites/yosys_validation/"+args.test+"> file does not exist")
    sys.exit()

plugins = YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/genesis2/brams_sim.v " + \
         YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/genesis2/cells_sim.v " + \
         YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/genesis2/dsp_sim.v "+ \
         YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/genesis2/dsp_map.v "+ \
         YS_ROOT+"/../../yosys/install/share/yosys/simlib.v "+ \
         YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/genesis2/TDP18K_FIFO.v "+ \
         YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/genesis2/ufifo_ctl.v "+ \
         YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/genesis2/sram1024x18.v"

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

        # Write the file out again
        with open(file_, 'w') as file:
            file.write(data)

def yosys_parser(PROJECT_NAME,raptor_log,synth_status,test_):
    stat = False ;next_command = False; dffsre = [];Data=[];DSP = [];BRAM=[];_Luts_=0;Carry_cells=0
    global run_synth_status
    run_synth_status = ""
    with open(raptor_log,"r") as in_file:
        for line in in_file:
            if re.search(r".*Printing statistics.*", line):
                stat = True
                next_command = False
                DSP.clear()
                BRAM.clear()
                dffsre.clear()

            if (re.search(r".*adder_carry.*", line) and (stat == True) and (next_command == False)):
                Carry_cells = line.split()[1]

            if (re.search(r".*\$lut.*", line) and (stat == True) and (next_command == False)):
                _Luts_ = line.split()[1]

            if (re.search(r".*RS_DSP2.*", line) and (stat == True) and (next_command == False)):
                DSP.append(int(line.split()[1]))

            if (re.search(r".*TDP.*K", line) and (stat == True) and (next_command == False)):
                BRAM.append(int(line.split()[1]))

            if ((re.search(r".*dff.*", line)) and stat == True and next_command == False):
                dffsre.append(int(line.split()[1]))

            if ((re.search(r".*yosys>.*", line) or (re.findall(r'[0-9]+\.', line) and re.search(r".*Printing statistics.*", line) == None)) and stat == True):
                stat = False
                next_command = True

        Data = [PROJECT_NAME,str(_Luts_),str(sum(dffsre)),str(Carry_cells),str(sum(BRAM)),str(sum(DSP)),synth_status]

    print(CGREEN+synth_status+" for "+test_+CEND)
    return Data

def vcs_parse(sim_file,test_,Data):
    with open (sim_file, 'r') as sim_file:
        for line in sim_file:
            if (re.search(r"Simulation Passed.*", line)):
                sim = True
                Data.append("Simulation Passed")
                print(CGREEN+"Simulation Passed for "+test_+CEND)
            if (re.search(r"Simulation Failed.*", line)):
                sim = False
                Data.append("Simulation Failed")
                print(CRED+"Simulation Failed for "+test_+CEND)


def compile(project_path,rtl_path,top_module,test_):
    synth = True
    if (os.path.exists(YS_ROOT +"/../../" + rtl_path+"/yosys.ys")==0):
        print("Cannot open "+YS_ROOT +"/../../" + rtl_path+"/yosys.ys")
    else:
        synth_cmd = YS_ROOT+"/../../"+JSON_data["yosys"]["yosys_path"]+" -s " + YS_ROOT +"/../../" + rtl_path+"/yosys.ys"
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
            synth = False

def simulate(project_path,rtl_path,top_module,test_):
    vcs_cmd = "vcs -sverilog " + project_path+"/"+top_module+"_post_synth.v " + YS_ROOT +"/../../" + rtl_path +"/"+top_module+".sv  " + YS_ROOT +"/../../" + rtl_path +"/tb.sv " + plugins + " -full64 -debug_all -kdb -lca +define+VCS_MODE=1"
    # print(vcs_cmd)
    try:
        execute(vcs_cmd,"vcs_compile.log","err_compile.log","VCS compilation",test_)
        execute("./simv","vcs_sim.log","err_sim.log","VCS simulation",test_)
    except Exception as e:
        sim = False
        print (str(e))


def test():
    header = ['Design Name', 'LUTs','DFF','Carry Logic','BRAM\'s','DSP\'s',"Synthesis Status", "Simulation Status"]
    CSV_File = open(path+"/results.csv",'w')
    writer = csv.writer(CSV_File)
    writer.writerow(header)
    CSV_File.close()
    for test in JSON_data["benchmarks"]:
        Data = []
        if JSON_data["benchmarks"][test]["compile_status"] == "active":
            synth = True
            project_path = os.path.join(path,test)
            rtl_path  = JSON_data["benchmarks"][test]["test_path"]
            top_module = JSON_data["benchmarks"][test]["top_module"]
            os.makedirs(project_path)
            os.chdir(project_path)

            compile(project_path,rtl_path,top_module,test)
            if (synth == True):
                try:
                    Data = yosys_parser(test,"synth.log","Synthesis Succeeded",test)
                except Exception as e:
                    Data = [test,"N/A","N/A","N/A","N/A","N/A","Synthesis Failed"]
                    print(str(e))
        if ((JSON_data["benchmarks"][test]["compile_status"] == "active") and (JSON_data["benchmarks"][test]["sim_status"] == "active")) and synth == True:
            simulate(project_path,rtl_path,top_module,test)
            try:
                vcs_parse("vcs_sim.log",test,Data)
            except Exception as e:
                Data.append("Cannot open ./simv file")
                print(str(e))
        if JSON_data["benchmarks"][test]["compile_status"] == "active":
            with open(path+"/results.csv",'a') as csv_file:
                writer = csv.writer(csv_file)
                writer.writerow(Data)
                print("\n================================================================================\n")

path = _create_new_project_dir_()
test()
