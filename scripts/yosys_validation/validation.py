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
    
def plugin(arch):
    plugins =  YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/cells_sim.v " + \
                YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/dsp_sim.v "+ \
                YS_ROOT+"/../../yosys/install/share/yosys/rapidsilicon/" + arch + "/dsp_map.v "+ \
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

def yosys_parser(PROJECT_NAME,raptor_log,synth_status,test_):
    stat = False ;next_command = False; dffsre = [];Data=[];DSP = [];BRAM=[];_Luts_=0;Carry_cells=[]
    global run_synth_status
    run_synth_status = ""
    regex = re.compile(r'\b(WARNING|Warning|renaming|INFO|Info|->|.cc)\b')
    # print(raptor_log)
    with open(raptor_log,"r") as in_file:
        status_found = False
        for line in in_file:
            if regex.search(line):
                continue
            else:
                if re.search(r".*Printing statistics.*", line):
                    stat = True
                    next_command = False
                    Carry_cells.clear()
                    DSP.clear()
                    BRAM.clear()
                    dffsre.clear()
                
                if (re.search(r".*Executing.*", line) and stat == True):
                    stat = False
                    next_command = True

                try:
                    if (re.search(r".*fa_.*bit.*", line) and (stat == True) and (next_command == False)):
                        Carry_cells.append(int(line.split()[1]))
                except:
                    pass

                if (re.search(r".*\$lut.*", line) and (stat == True) and (next_command == False)):
                    # print(line)
                    _Luts_ = line.split()[1]
                
                if (re.search(r".*RS_DSP.*", line) and (stat == True) and (next_command == False)):
                    # print(line)
                    DSP.append(int(line.split()[1]))

                if (re.search(r".*TDP.*K", line) and (stat == True) and (next_command == False)):
                    # print(line)
                    BRAM.append(int(line.split()[1]))

                if ((re.search(r".*dff.*", line)) and stat == True and next_command == False):
                    # print(line)
                    dffsre.append(int(line.split()[1]))
                
                if ((re.search(r"ERROR:.*", line) or re.search(r"\[ERROR\].*", line))):
                    # print(line)
                    failure_type =  ""
                    error_msg=line
                    break
                if re.search(r"End of script.*", line):
                    # print(line)
                    synth_status="Synthesis Succeeded"
                    status_found = True

    
    if status_found == 0:
        synth_status ="Synthesis Failed"

    Data = [PROJECT_NAME,str(_Luts_),str(sum(dffsre)),str(sum(Carry_cells)),str(sum(BRAM)),str(sum(DSP)),synth_status]

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
    synth_status = True
    if (os.path.exists(YS_ROOT +"/../../" + rtl_path+"/yosys.ys")==0):
        print("Cannot open "+YS_ROOT +"/../../" + rtl_path+"/yosys.ys")
    else:
        with open (YS_ROOT +"/../../" + rtl_path+"/yosys.ys","r") as ys_file:
            for line in ys_file:
                if (re.search(r".*-tech genesis.*", line)):
                    architecture = line.split("-tech")
                    architecture = architecture[1].split()[0]

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
            synth_status = False
    
    return synth_status,architecture

def simulate(project_path,rtl_path,top_module,test_,architecture):
    vcs_cmd = "vcs -sverilog " + project_path+"/"+top_module+"_post_synth.v " + YS_ROOT +"/../../" + rtl_path +"/"+top_module+".sv  " + YS_ROOT +"/../../" + rtl_path +"/tb.sv " + plugin(architecture) + " -full64 -debug_all -kdb -lca -timescale=1ns/100ps +define+VCS_MODE=1"
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
        execute(vcs_cmd,"vcs_compile.log","err_compile.log","VCS compilation",test_)
        execute("./simv","vcs_sim.log","err_sim.log","VCS simulation",test_)
        execute("vcd2fst tb.vcd tb.fst --compress" ,"vcd_sim.log","err_vcd.log","VCD file compression",test_)
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
            project_path = os.path.join(path,test)
            rtl_path  = JSON_data["benchmarks"][test]["test_path"]
            top_module = JSON_data["benchmarks"][test]["top_module"]
            os.makedirs(project_path)
            os.chdir(project_path)
            synth, architecture = compile(project_path,rtl_path,top_module,test)
            if (synth == True):
                try:
                    Data = yosys_parser(test,"synth.log","Synthesis Succeeded",test)
                except Exception as e:
                    Data = [test,"N/A","N/A","N/A","N/A","N/A","Synthesis Failed"]
                    print(str(e))
        if ((JSON_data["benchmarks"][test]["compile_status"] == "active") and (JSON_data["benchmarks"][test]["sim_status"] == "active")) and synth == True and args.sim == True:
            simulate(project_path,rtl_path,top_module,test,architecture)
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
# try:
#     shutil.rmtree(YS_ROOT+'/logs/')
# except Exception as e: 
#     print(str(e))
path = _create_new_project_dir_()
test()
