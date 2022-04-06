cmake_minimum_required(VERSION 3.12)

# Detect build type, fallback to release and throw a warning if use didn't
# specify any
if(NOT CMAKE_BUILD_TYPE)
    message(WARNING "Build type not set, falling back to Release mode.
        To specify build type use:
        -DCMAKE_BUILD_TYPE=<mode> where <mode> is Debug or Release.")
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE STRING "Choose the type of build, options are: Debug Release."
        FORCE)
endif(NOT CMAKE_BUILD_TYPE)

project(yosys_verific_rs)

add_subdirectory(logic_synthesis-rs)

option(ENABLE_VERIFIC "Enable Verific frontend" ON)
option(YOSYS_LIB "Build Yosys also as library" OFF)
option(BUILD_YOSYS_PLUGINS "Enable build of yosys-plugins" OFF)

set(ABCEXTERNAL ${CMAKE_CURRENT_SOURCE_DIR}/build/logic_synthesis-rs/bin/abc)
set(YOSYS_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/yosys/install)
set(VERIFIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/verific/verific-vJan22)

list(APPEND VERIFIC_MK_ARGS 
    "VERSION=-O3"
    "TOPFLAGS=-I/usr/include/tcl -fPIC -std=c++11"
)
list(APPEND YOSYS_MK_ARGS
    "CONFIG=gcc"
    "PREFIX=${YOSYS_PATH}"
    "ABCEXTERNAL=${ABCEXTERNAL}"
)

if (ENABLE_VERIFIC)
    list(APPEND YOSYS_MK_ARGS
        "ENABLE_VERIFIC=1"
        "DISABLE_VERIFIC_EXTENSIONS=1"
        "VERIFIC_DIR=${VERIFIC_DIR}"
    )
endif()

if (YOSYS_LIB)
    list(APPEND YOSYS_MK_ARGS
        "ENABLE_LIBYOSYS=1"
    )
endif()

list(APPEND YOSYS_RS_PLUGIN_MK_ARGS
    "YOSYS_PATH=${YOSYS_PATH}"
)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND YOSYS_MK_ARGS
        "ENABLE_DEBUG=1"
    )
    list(APPEND VERIFIC_MK_ARGS 
        "PROJECT_FLAGS=-Og -DDEBUG"
    )
    # yosys-plugins and yosys-rs-plugin inherit yosys flags.
endif()

add_custom_target(
    yosys ALL 
    COMMAND $(MAKE) install ${YOSYS_MK_ARGS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys
    COMMENT "Compile Yosys with given Makefile"
)
add_custom_target(
    yosys_clean
    COMMAND $(MAKE) clean
    COMMAND rm -rf ${YOSYS_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys
    COMMENT "Clean Yosys/Yosys+Verific with given Makefile"
)
list(APPEND CLEAN_TARGETS
    "yosys_clean"
)

if (ENABLE_VERIFIC)
    add_custom_target(
        verific ALL 
        COMMAND $(MAKE) ${VERIFIC_MK_ARGS}
        WORKING_DIRECTORY ${VERIFIC_DIR}/tclmain
        COMMENT "Compile Verific with given Makefile"
    )
    add_custom_target(
        verific_clean
        COMMAND $(MAKE) clean
        WORKING_DIRECTORY ${VERIFIC_DIR}/tclmain
        COMMENT "Clean Verific with given Makefile"
    )
    add_dependencies(yosys verific)
    list(APPEND CLEAN_TARGETS
        "verific_clean"
    )
endif()

if (BUILD_YOSYS_PLUGINS)
    list(APPEND YOSYS_PLUGINS_MK_ARGS
        "YOSYS_PATH=${YOSYS_PATH}"
        "EXTRA_FLAGS=\"-DPASS_NAME=synth_ql\""
    )
    add_custom_target(
        yosys-plugins ALL
        COMMAND $(MAKE) install_ql-qlf ${YOSYS_PLUGINS_MK_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-plugins
        COMMENT "Compile Yosys ql-qlf plugin with given Makefile"
    )
    add_custom_target(
        yosys-plugins_clean
        COMMAND $(MAKE) clean
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-plugins
        COMMENT "Clean Yosys ql-qlf plugin with given Makefile"
    )
    add_dependencies(yosys-plugins yosys)
    list(APPEND CLEAN_TARGETS
        "yosys-plugins_clean"
    )
endif()

add_custom_target(
    yosys-rs-plugin ALL 
    COMMAND $(MAKE) install ${YOSYS_RS_PLUGIN_MK_ARGS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin
    COMMENT "Compile Yosys RS plugin with given Makefile"
)
add_custom_target(
    yosys-rs-plugin_clean
    COMMAND $(MAKE) clean
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin
    COMMENT "Clean Yosys RS plugin with given Makefile"
)
add_dependencies(yosys-rs-plugin yosys)
list(APPEND CLEAN_TARGETS
    "yosys-rs-plugin_clean"
)

add_custom_target(
    clean_all
    COMMAND $(MAKE) ${CLEAN_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Clean All"
)

# Installation target
install(
    DIRECTORY ${YOSYS_PATH}/bin
    DESTINATION .
    PATTERN "*"
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                GROUP_EXECUTE GROUP_READ)
install(
    DIRECTORY ${YOSYS_PATH}/share 
    DESTINATION .
    PATTERN "*.so"
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                GROUP_EXECUTE GROUP_READ)
install(
    DIRECTORY ${YOSYS_PATH}/share 
    DESTINATION .
    PATTERN "*.so" EXCLUDE
    PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ)

# Check system 
message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
