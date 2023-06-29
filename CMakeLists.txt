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

set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 209)

project(yosys_verific_rs)

set(YOSYS_VERIFIC ON)

if (APPLE)
    message(WARNING "MacOs build is temporarily disabled: EDA-799")
else()

list(APPEND YOSYS_VERIFIC_RS_RECURSIVE_SUBSYTEMS
    yosys
    yosys-plugins
    yosys-rs-plugin
    )

if (NOT RAPTOR)
    list(APPEND YOSYS_VERIFIC_RS_RECURSIVE_SUBSYTEMS
        Raptor_Tools
        )
endif()

list(APPEND YOSYS_VERIFIC_RS_NON_RECURSIVE_SUBSYTEMS
    logic_synthesis-rs
    )

find_package(Git QUIET)

function(checkout_subsystems SUBSYSTEM_LIST WORK_DIR RECURSIVE UPDATE)
    foreach(SUBMODULE IN LISTS ${SUBSYSTEM_LIST})
        file(GLOB README ${WORK_DIR}/${SUBMODULE}/README*)
        if (NOT EXISTS "${README}" OR ${UPDATE})
            message("Updating ${SUBMODULE} submodule...")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init ${RECURSIVE} ${SUBMODULE}
                WORKING_DIRECTORY ${WORK_DIR}
                RESULT_VARIABLE GIT_SUBMOD_RESULT
                )
            if  (NOT GIT_SUBMOD_RESULT EQUAL "0")
                message(FATAL_ERROR "\"${GIT_EXECUTABLE} submodule update --init ${RECURSIVE} ${SUBMODULE}\" failed with ${GIT_SUBMOD_RESULT}, please checkout submodule manually")
            endif()
        endif()
    endforeach()
endfunction(checkout_subsystems)

if (NOT UPDATE_SUBMODULES)
    set(UPDATE_SUBMODULES OFF)
endif()

checkout_subsystems(YOSYS_VERIFIC_RS_RECURSIVE_SUBSYTEMS ${CMAKE_CURRENT_SOURCE_DIR} "--recursive" ${UPDATE_SUBMODULES})
checkout_subsystems(YOSYS_VERIFIC_RS_NON_RECURSIVE_SUBSYTEMS ${CMAKE_CURRENT_SOURCE_DIR} "" ${UPDATE_SUBMODULES})

if (PRODUCTION_BUILD)
    message("Production Build type set to ON")
    if (NOT RAPTOR)
        get_filename_component(FLEX_LM_SRC_DIR "Raptor_Tools/Flex_LM"
            REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
        add_subdirectory(${FLEX_LM_SRC_DIR})
        # No need for else as FLEX_LM_SRC_DIR will be set by Raptor
    endif()
    message("FLEX: "  ${FLEX_LM_SRC_DIR})
endif()

option(ENABLE_VERIFIC "Enable Verific frontend" ON)
option(YOSYS_LIB "Build Yosys also as library" OFF)
if (APPLE)
    option(BUILD_YOSYS_PLUGINS "Enable build of yosys-plugins" OFF)
else()
    option(BUILD_YOSYS_PLUGINS "Enable build of yosys-plugins" ON)
endif()
option(ENABLE_VALGRIND_TESTS "Enable Valgrind tests" OFF)

get_filename_component(VERIFIC_HOME "Raptor_Tools/verific_rs"
    REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
message("VERIFIC_HOME: ${VERIFIC_HOME}")

get_filename_component(READ_VERILOG_SRC_DIR "Raptor_Tools/gatelevel_readers/read_verilog"
REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
message("READ_VERILOG_SRC_DIR: ${READ_VERILOG_SRC_DIR}")

if (NOT RAPTOR)
    add_subdirectory(${READ_VERILOG_SRC_DIR})
endif()

set(LIB_READ_VERILOG ${CMAKE_BINARY_DIR}/Raptor_Tools/gatelevel_readers/read_verilog/libreadver.a)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(YOSYS_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/yosys/debug-install)
else()
    set(YOSYS_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/yosys/install)
endif()

list(APPEND YOSYS_MK_ARGS
    "CONFIG=gcc"
    "PREFIX=${YOSYS_PATH}"
    "ABCEXTERNAL=abc"
    "YOSYS_VERIFIC=1"
    "YOSYS_VERIFIC_VER_STR=Yosys-Verific ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
)

if (ENABLE_VERIFIC)
    list(APPEND YOSYS_MK_ARGS
        "ENABLE_VERIFIC=1"
        "DISABLE_VERIFIC_EXTENSIONS=1"
        "VERIFIC_DIR=${VERIFIC_HOME}"
    )
endif()

if (PRODUCTION_BUILD)
    string(CONCAT FLEX_LM_LDLIBS
        "${FLEX_LM_SRC_DIR}/x64_lsb/liblmgr_trl.a "
        "${FLEX_LM_SRC_DIR}/x64_lsb/libcrvs.a "
        "${FLEX_LM_SRC_DIR}/x64_lsb/libsb.a "
        "${FLEX_LM_SRC_DIR}/x64_lsb/activation/lib/libnoact.a "
        "${FLEX_LM_SRC_DIR}/x64_lsb/liblmgr_dongle.a "
        "${CMAKE_BINARY_DIR}/Raptor_Tools/Flex_LM/rs_licenseManager.a "
    )
    list(APPEND YOSYS_MK_ARGS
        "PRODUCTION_BUILD=1"
        "EXTRA_LDLIBS=${FLEX_LM_LDLIBS} -lssl -lcrypto ${LIB_READ_VERILOG} -lstdc++"
        "EXTRA_CXX_FLAGS=-I${FLEX_LM_SRC_DIR} -I${FLEX_LM_SRC_DIR}/machind -DPRODUCTION_BUILD=1 -I${READ_VERILOG_SRC_DIR}/src/read_nl/"
        "EXTRA_OBJS=${FLEX_LM_SRC_DIR}/x64_lsb/lm_new_pic.o"
        "EXTRA_LD_FLAGS=-fPIC -fvisibility=hidden -no-pie -lrt -Wl,--no-as-needed -ldl -pthread"
    )
else()
    list(APPEND YOSYS_MK_ARGS
    "EXTRA_LDLIBS=${LIB_READ_VERILOG} -lssl -lcrypto -lstdc++"
    "EXTRA_CXX_FLAGS=-I${READ_VERILOG_SRC_DIR}/src/read_nl/"
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

if (NOT PRODUCTION_BUILD)
    list(APPEND YOSYS_RS_PLUGIN_MK_ARGS
        "EXTRA_FLAGS=-DDEV_BUILD"
    )
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND YOSYS_MK_ARGS
        "ENABLE_DEBUG=1"
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
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${YOSYS_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys
    COMMENT "Clean Yosys/Yosys+Verific with given Makefile"
)

add_dependencies(yosys libreadver)

if (ENABLE_VERIFIC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (NOT RAPTOR)
        execute_process(
            COMMAND ${CMAKE_MAKE_PROGRAM} clean
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Raptor_Tools/verific_rs
            COMMAND_ECHO STDOUT
            RESULT_VARIABLE GIT_SUBMOD_RESULT
        )
        add_subdirectory(${VERIFIC_HOME})
    endif()
    add_dependencies(yosys verific)
endif()

add_subdirectory(logic_synthesis-rs)
# TODO: Fix Mac OS build.
if (NOT APPLE)
    # There are linking issues with Verific libraries on Mac OS.
    # We need Mac OS environment to debug and fix the issue.
    add_subdirectory(analyze)
endif()

if (PRODUCTION_BUILD)
    add_dependencies(yosys rs_licenseManager)
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
        COMMAND $(MAKE) clean ${YOSYS_PLUGINS_MK_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-plugins
        COMMENT "Clean Yosys ql-qlf plugin with given Makefile"
    )
    add_dependencies(yosys-plugins yosys)
    add_dependencies(yosys_clean yosys-plugins_clean)
endif()

add_custom_target(
    yosys-rs-plugin ALL
    COMMAND $(MAKE) install ${YOSYS_RS_PLUGIN_MK_ARGS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin
    COMMENT "Compile Yosys RS plugin with given Makefile"
)
add_custom_target(
    yosys-rs-plugin_clean
    COMMAND $(MAKE) clean ${YOSYS_RS_PLUGIN_MK_ARGS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin
    COMMENT "Clean Yosys RS plugin with given Makefile"
)
add_dependencies(yosys-rs-plugin yosys)
add_dependencies(yosys_clean yosys-rs-plugin_clean)

add_custom_target(
    clean_yosys_verific_rs
    COMMAND $(MAKE) yosys_clean
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Clean All"
)

add_custom_command(
    OUTPUT ${YOSYS_PATH}/bin/abc
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/logic_synthesis-rs/bin/abc ${YOSYS_PATH}/bin/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Copy abc executable into bin"
    DEPENDS abc yosys
)

add_custom_command(
    OUTPUT ${YOSYS_PATH}/bin/de
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/logic_synthesis-rs/bin/de ${YOSYS_PATH}/bin/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Copy de executable into bin"
    DEPENDS de yosys
)

add_custom_target(
    copy_files ALL
    DEPENDS ${YOSYS_PATH}/bin/abc ${YOSYS_PATH}/bin/de)

# Installation target
install(
    PROGRAMS ${YOSYS_PATH}/bin/yosys
             ${YOSYS_PATH}/bin/yosys-config
             ${YOSYS_PATH}/bin/yosys-filterlib
             ${YOSYS_PATH}/bin/yosys-smtbmc
    DESTINATION bin)
install(
    DIRECTORY ${YOSYS_PATH}/share/yosys
    DESTINATION share
    USE_SOURCE_PERMISSIONS)

# Check system
message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
enable_testing()

set(PARALLEL_TESTS 1)
include(ProcessorCount)
ProcessorCount(CORES)
if (CORES GREATER 7)
    math(EXPR PARALLEL_TESTS "${CORES} / 4" OUTPUT_FORMAT DECIMAL)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (ENABLE_VALGRIND_TESTS)
        add_test(
            NAME yosys_verific_rs-valgrind-test
            COMMAND make valgrind ${YOSYS_RS_PLUGIN_MK_ARGS} -j ${PARALLEL_TESTS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin)
        set_tests_properties(yosys_verific_rs-valgrind-test PROPERTIES
            FAIL_REGULAR_EXPRESSION "FAILED")
        add_test(
            NAME yosys_verific_rs-valgrind-genesis-test
            COMMAND make valgrind_gen ${YOSYS_RS_PLUGIN_MK_ARGS} -j ${PARALLEL_TESTS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin)
        set_tests_properties(yosys_verific_rs-valgrind-genesis-test PROPERTIES
            FAIL_REGULAR_EXPRESSION "FAILED")
    endif()
endif()

add_test(
    NAME yosys_verific_rs-smoke-test
    COMMAND make test ${YOSYS_RS_PLUGIN_MK_ARGS} -j ${PARALLEL_TESTS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin)
set_tests_properties(yosys_verific_rs-smoke-test PROPERTIES
    FAIL_REGULAR_EXPRESSION "FAILED")

add_test(
    NAME yosys_verific_rs-genesis-test
    COMMAND make test_gen ${YOSYS_RS_PLUGIN_MK_ARGS} -j ${PARALLEL_TESTS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin)
set_tests_properties(yosys_verific_rs-genesis-test PROPERTIES
    FAIL_REGULAR_EXPRESSION "FAILED")
endif() # if (NOT APPLE)
