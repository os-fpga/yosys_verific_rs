import sys
import argparse
import csv
import pandas as pd
from termcolor import colored

parser = argparse.ArgumentParser(description="This script analyzies QoR results compared with base suite.")
parser.add_argument("file",
                    help="csv file for analyzing")
args = parser.parse_args()
df = pd.read_csv( args.file )
percentage_list = [i for i in df if ("PERCENTAGE" in i) and ("LUT" in i)]
index = 2
flag = True

def add_min_benchmarks(data, min_val, values):
    global index
    m = 0
    min_list = []
    for i in values:
        if i == min_val:
            min_list.append(df['Benchmarks'][m])
        m += 1
    for i in min_list:
        data.at[index, "Benchmarks"] = i
        data.at[index, "Minimum"] = min_val  
        index += 1

def add_max_benchmarks(data, max_val, values):
    global index
    m = 0
    max_list = []
    for i in values:
        if i == max_val:
            max_list.append(df['Benchmarks'][m])
        m += 1
    for i in max_list:
        data.at[index, "Benchmarks"] = i
        data.at[index, "Maximum"] = max_val  
        index += 1

def flag_condition(min_val, average):
    global flag
    if min_val >= 0 and average > 0:
        print("The status of QoR is ", colored("GREEN.", "white", "on_green"))
    elif min_val >= -5 and average > 0:
        print("The status of QoR is ", colored("BLUE.", "white", "on_blue"))
    elif min_val >= -5 and average <= 0:
        print("The status of QoR is Yellow", colored("YELLOW.", "white", "on_yellow"))
    elif min_val < -5 and average > 0:
        print("The status of QoR is ", colored("MAGENTA.", "white", "on_magenta"))
    elif min_val < -5 and average <= 0:
        print("The status of QoR is ", colored("RED.", "white", "on_red"))
        flag = False

def check_status():
    for i in df:
        if "STATUS" in i:
            if "Fail" in list(df[i]): sys.exit("Some benchmarks have failed.")

def main():
    for i in percentage_list:
        data = pd.DataFrame(columns=["Benchmarks", "Average", "Minimum", "Maximum"])
        values = list(df[i][: -4])
        min_val = min(values)
        max_val = max(values) 
        average = sum(list(df[i][:-4])) / len(list(df[i][:-4]))
        data.at[1, "Average"] = average
        add_min_benchmarks(data, min_val, values)
        add_max_benchmarks(data, max_val, values)
        blankIndex=[''] * len(data)
        data.index=blankIndex
        print(i)
        print(data.fillna('-'))
        flag_condition(min_val, average)
    check_status()
    if flag:
        pass
    else:
        sys.exit(1)

if __name__ == "__main__":
        main()
