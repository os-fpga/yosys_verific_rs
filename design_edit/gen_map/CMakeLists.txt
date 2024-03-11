cmake_minimum_required(VERSION 3.10)
project(rs_primitive)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)  

# Define the output directory for the generated header relative to the current source directory
set(GENERATED_HEADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/" CACHE STRING "Directory to output generated header files")

# Define the directory path directly in CMakeLists.txt
set(DIRECTORY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../yosys-rs-plugin/genesis3/FPGA_PRIMITIVES_MODELS/specs_internal" CACHE STRING "Path to the directory")
set(OUT_PATH "${GENERATED_HEADER_OUTPUT_DIR}/rs_primitive.h")


# Create an executable for your program
add_executable(rs_primitive rs_primitive.cpp)

# Pass the directory path as a preprocessor definition
target_compile_definitions(rs_primitive PRIVATE DIRECTORY_PATH="${DIRECTORY_PATH}")

# Pass the output directory as a preprocessor definition to the code
target_compile_definitions(rs_primitive PRIVATE GENERATED_HEADER_OUTPUT_DIR="${GENERATED_HEADER_OUTPUT_DIR}")

add_custom_command(
    OUTPUT ${OUT_PATH}
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/rs_primitive
    DEPENDS rs_primitive
    COMMENT "Generating rs_primitive.h"
)

add_custom_target(
    GeneratePrimitive ALL
    DEPENDS ${OUT_PATH}
)
