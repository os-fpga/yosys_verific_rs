#!/bin/bash

# Iterate over each .ys file in the Tests directory
for ys_file in Tests/**/*.ys; do
    # Extract the directory containing the .ys file
    folder=$(dirname "$ys_file")

    # Extract the folder name
    folder_name=$(basename "$folder")

    echo "Running yosys for ${folder_name}..."

    # Change directory to the folder containing the .ys file and run yosys
    (cd "$folder" && cp -f ./tmp/* ./gold/ > /dev/null && diff ./gold ./tmp)

    echo "Done with ${folder_name}."
done
