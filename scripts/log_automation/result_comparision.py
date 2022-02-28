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
data = pd.DataFrame(columns=["Benchmarks", "Average", "Minimum", "Maximum"])
p = [i for i in df if i.startswith("PERCENTAGE Yosys")]
percentage_list = list(df[p[0]])
index = 2

def add_min_benchmarks(data, min_val):
    global index
    m = 0
    min_list = []
    for i in percentage_list:
        if i == min_val:
            min_list.append(df['Benchmarks'][m])
        m += 1
    for i in min_list:
        data.at[index, "Benchmarks"] = i
        data.at[index, "Minimum"] = min_val  
        index += 1

def add_max_benchmarks(data, max_val):
    global index
    m = 0
    max_list = []
    for i in percentage_list:
        if i == max_val:
            max_list.append(df['Benchmarks'][m])
        m += 1
    for i in max_list:
        data.at[index, "Benchmarks"] = i
        data.at[index, "Maximum"] = max_val  
        index += 1

def flag_condition(min_val, average):
    if min_val >= 0 and average > 0:
        print("The status of QoR is ", colored("GREEN.", "white", "on_green"))
    elif min_val >= -5 and average > 0:
        print("The status of QoR is ", colored("BLUE.", "white", "on_blue"))
    elif min_val >= -5 and average <= 0:
        print("The status of QoR is ", colored("YELLOW.", "black", "on_yellow"))
    elif min_val < -5 and average > 0:
        print("The status of QoR is ", colored("MAGENTA.", "white", "on_magenta"))
    elif min_val < -5 and average <= 0:
        print("The status of QoR is ", colored("RED.", "white", "on_red"))
        sys.exit(1)

def main():
     
    min_val = min(percentage_list)
    max_val = max(percentage_list) 
    average = sum(list(df[p[0]])) / len(list(df[p[0]]))
    data.at[1, "Average"] = average
    add_min_benchmarks(data, min_val)
    add_max_benchmarks(data, max_val)
    blankIndex=[''] * len(data)
    data.index=blankIndex
    print(data.fillna('-'))
    flag_condition(min_val, average)

if __name__ == "__main__":
        main()
