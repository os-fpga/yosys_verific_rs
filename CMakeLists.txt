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
set(VERSION_PATCH 60)

project(yosys_verific_rs)

if (PRODUCTION_BUILD)
    get_filename_component(FLEX_LM_SRC_DIR "../Tools/Flex_LM"
        REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if (NOT RAPTOR)
        add_subdirectory(${FLEX_LM_SRC_DIR} Flex_LM)
    endif()
endif()

add_subdirectory(logic_synthesis-rs)

option(ENABLE_VERIFIC "Enable Verific frontend" ON)
option(YOSYS_LIB "Build Yosys also as library" OFF)
if (APPLE)
    option(BUILD_YOSYS_PLUGINS "Enable build of yosys-plugins" OFF)
else()
    option(BUILD_YOSYS_PLUGINS "Enable build of yosys-plugins" ON)
endif()
option(ENABLE_VALGRIND_TESTS "Enable Valgrind tests" OFF)

set(BUILD_LSORACLE OFF)
set(VERIFIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/verific/verific-vMay22)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(YOSYS_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/yosys/debug-install)
else()
    set(YOSYS_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/yosys/install)
endif()

list(APPEND VERIFIC_MK_ARGS 
    "VERSION=-O3"
    "TOPFLAGS=-I/usr/include/tcl -fPIC -std=c++11"
)
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
        "VERIFIC_DIR=${VERIFIC_DIR}"
    )
endif()

if (PRODUCTION_BUILD)
    string(CONCAT FLEX_LM_LDLIBS 
        "${FLEX_LM_SRC_DIR}/x64_lsb/liblmgr_trl.a "
        "${FLEX_LM_SRC_DIR}/x64_lsb/libcrvs.a "
        "${FLEX_LM_SRC_DIR}/x64_lsb/libsb.a "
        "${FLEX_LM_SRC_DIR}/x64_lsb/activation/lib/libnoact.a "
        "${FLEX_LM_SRC_DIR}/x64_lsb/liblmgr_dongle.a "
        "${CMAKE_CURRENT_BINARY_DIR}/Flex_LM/librs_licenseManager.a "
    )
    list(APPEND YOSYS_MK_ARGS
        "PRODUCTION_BUILD=1"
#        "EXTRA_LDLIBS=${FLEX_LM_LDLIBS}"
#        "EXTRA_CXX_FLAGS=-I${FLEX_LM_SRC_DIR} -I${FLEX_LM_SRC_DIR}/machind -DPRODUCTION_BUILD=1"
#        "EXTRA_OBJS=${FLEX_LM_SRC_DIR}/x64_lsb/lm_new.o"
#        "EXTRA_LD_FLAGS=-Wl,--copy-dt-needed-entries"
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
    add_dependencies(verific_clean yosys_clean)
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
    clean_all
    COMMAND $(MAKE) verific_clean
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Clean All"
)

add_custom_command(
    OUTPUT ${YOSYS_PATH}/bin/abc
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/logic_synthesis-rs/bin/abc ${YOSYS_PATH}/bin/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Copy abc executable into bin"
    DEPENDS abc yosys
)

add_custom_command(
    OUTPUT ${YOSYS_PATH}/bin/de
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/logic_synthesis-rs/bin/de ${YOSYS_PATH}/bin/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Copy de executable into bin"
    DEPENDS de yosys
)

if (BUILD_LSORACLE)
    add_custom_command(
        OUTPUT ${YOSYS_PATH}/bin/lsoracle
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/logic_synthesis-rs/bin/lsoracle ${YOSYS_PATH}/bin/
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Copy LSOracle executable into bin"
        DEPENDS lsoracle yosys
    )
    add_custom_target(
        copy_files ALL
        DEPENDS ${YOSYS_PATH}/bin/abc ${YOSYS_PATH}/bin/de ${YOSYS_PATH}/bin/lsoracle
    )
else()    
    add_custom_target(
        copy_files ALL
        DEPENDS ${YOSYS_PATH}/bin/abc ${YOSYS_PATH}/bin/de)
endif()

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

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (ENABLE_VALGRIND_TESTS)
        add_test(
            NAME yosys_verific_rs-valgrind-test
            COMMAND make valgrind ${YOSYS_RS_PLUGIN_MK_ARGS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin)
        set_tests_properties(yosys_verific_rs-valgrind-test PROPERTIES
            FAIL_REGULAR_EXPRESSION "FAILED")
    endif()
endif()

add_test(
    NAME yosys_verific_rs-smoke-test
    COMMAND make test ${YOSYS_RS_PLUGIN_MK_ARGS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/yosys-rs-plugin)
set_tests_properties(yosys_verific_rs-smoke-test PROPERTIES
    FAIL_REGULAR_EXPRESSION "FAILED")
