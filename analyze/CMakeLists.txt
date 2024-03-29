cmake_minimum_required(VERSION 3.12)

project(analyze)

if (PRODUCTION_BUILD)
    # message("Production Build type set to ON")
    # add_definitions(-DPRODUCTION_BUILD)
    # if ((NOT RAPTOR) AND (NOT YOSYS_VERIFIC))
    #     get_filename_component(FLEX_LM_SRC_DIR "../Raptor_Tools/Flex_LM"
    #         REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
    #     add_subdirectory(${FLEX_LM_SRC_DIR})
    # endif()
    # message("FLEX: "  ${FLEX_LM_SRC_DIR})
endif(PRODUCTION_BUILD)

if(ENABLE_VERIFIC)
    get_filename_component(READ_VERILOG_SRC_DIR "../Raptor_Tools/gatelevel_readers/read_verilog"
        REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
    if ((NOT RAPTOR) AND (NOT YOSYS_VERIFIC))
        add_subdirectory(${READ_VERILOG_SRC_DIR} "libreadver")
        message("READ_VERILOG_SRC_DIR: ${READ_VERILOG_SRC_DIR}")
    endif()
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(OPENSSL REQUIRED openssl)

if( OPENSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    message("SSL not found")
    # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SHARE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/share/verific)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

function(files_to_dirs file_list directory_list)
    # Extract the directories for each header file
    foreach(file ${${file_list}})
        get_filename_component(dir ${file} DIRECTORY)
        list(APPEND dir_list ${dir})
    endforeach()

    # Remove any duplicates
    list(LENGTH "${dir_list}" length)

    if(${length} GREATER 1) # Avoid error with zero-length lists
        list(REMOVE_DUPLICATES ${dir_list})
    endif()

    # Set the second argument in the caller's scope
    set(${directory_list} ${dir_list} PARENT_SCOPE)
endfunction(files_to_dirs)

file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc)

add_executable(analyze ${SOURCE_FILES})

if(ENABLE_VERIFIC)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_dependencies(analyze verific)
    endif()
endif()

list(APPEND ANALYZE_CXX_FLAGS_DEBUG
    -fPIC -Werror -Wall -O0 -g ${MEM_SANITIZER_FLAGS} ${CXX_DEFINES}
)
list(APPEND ANALYZE_CXX_FLAGS_RELEASE
    -fPIC -Werror -Wall -O3 -DNDEBUG ${CXX_DEFINES}
)

if(ENABLE_VERIFIC)
    get_filename_component(VERIFIC_HOME "../Raptor_Tools/verific_rs"
        REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
    message("VERIFIC_HOME: ${VERIFIC_HOME}")

    file(GLOB_RECURSE LIB_HEADERS
        ${VERIFIC_HOME}/*/*.h)

    files_to_dirs(LIB_HEADERS LIB_INCLUDE_DIRS)
endif()

if (PRODUCTION_BUILD)
#     # add_dependencies(analyze rs_licenseManager)
#     # include_directories(analyze PUBLIC ${FLEX_LM_SRC_DIR}
#     #                             ${FLEX_LM_SRC_DIR}/machind
# )
endif(PRODUCTION_BUILD)

target_include_directories(analyze PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/)
target_include_directories(analyze PUBLIC ${LIB_INCLUDE_DIRS})

if(ENABLE_VERIFIC)
    target_include_directories(analyze PUBLIC ${READ_VERILOG_SRC_DIR}/src/read_nl/)
endif()

if (APPLE)
    set(OS "mac")
elseif (UNIX)
    set(OS "linux")
endif()

if(ENABLE_VERIFIC)
    list(APPEND VERIFIC_LIBS
        ${VERIFIC_HOME}/verilog/verilog-${OS}.a
        ${VERIFIC_HOME}/database/database-${OS}.a
        ${VERIFIC_HOME}/util/util-${OS}.a
        ${VERIFIC_HOME}/containers/containers-${OS}.a
        ${VERIFIC_HOME}/hier_tree/hier_tree-${OS}.a
        ${VERIFIC_HOME}/vhdl/vhdl-${OS}.a
        )

    add_dependencies(analyze libreadver)
endif()

if (PRODUCTION_BUILD)
    # target_link_libraries(analyze PUBLIC rs_licenseManager)
endif()

if(ENABLE_VERIFIC)
    target_link_libraries(analyze PUBLIC ${VERIFIC_LIBS})
    target_link_libraries(analyze PUBLIC libreadver)
endif()

target_link_libraries(analyze PUBLIC ${OPENSSL_LIBRARIES})

target_compile_options(analyze PUBLIC $<$<CONFIG:DEBUG>:${ANALYZE_CXX_FLAGS_DEBUG}>)
target_compile_options(analyze PUBLIC $<$<CONFIG:RELEASE>:${ANALYZE_CXX_FLAGS_RELEASE}>)

if(ENABLE_VERIFIC)
    add_custom_command(TARGET analyze
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${VERIFIC_HOME}/vhdl_packages ${SHARE_DIRECTORY}/vhdl_packages
        )
endif()

install(
    PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/analyze
    DESTINATION bin
)

if(ENABLE_VERIFIC)
    install(
        DIRECTORY ${SHARE_DIRECTORY}
        DESTINATION share
        USE_SOURCE_PERMISSIONS
    )
endif()

add_custom_target(
    clean_analyze
    COMMAND find . -name "port_info.json" | xargs rm -rf
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/
    COMMENT "Clean All"
)

MACRO(SUBDIRLIST TESTS CURR_DIR)
    FILE(GLOB files RELATIVE ${CURR_DIR} ${CURR_DIR}/*)
    SET(DIR_LIST "")
  FOREACH(test ${files})
    IF(IS_DIRECTORY ${CURR_DIR}/${test})
        LIST(APPEND DIR_LIST ${test})
    ENDIF()
  ENDFOREACH()
  SET(${TESTS} ${DIR_LIST})
ENDMACRO()

SUBDIRLIST(TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/)

enable_testing()
foreach(test ${TESTS})
  add_test(
      NAME analyze-smoke-test-${test}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/analyze -f analyze.cmd
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test})
endforeach()

foreach(test ${TESTS})
    add_test(analyze-smoke-test-port-cmp-${test}
        ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test}/port_info.json
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test}/gold_port.json)
    add_test(analyze-smoke-test-hier-cmp-${test}
        ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test}/hier_info.json
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test}/gold_hier.json)
endforeach()

