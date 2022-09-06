cmake_minimum_required(VERSION 3.12)

project(analyze)

if (PRODUCTION_BUILD)
    message("Production Build type set to ON")
    set (PRODUCTION_BUILD_FLAG "-DPRODUCTION_BUILD=1")
    add_definitions(-DPRODUCTION_BUILD)
    if (NOT RAPTOR)
        get_filename_component(FLEX_LM_SRC_DIR "../../Raptor_Tools/Flex_LM"
          REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        
        # Flex_LM directory already has been added by vpr/CMakeLists.txt
    endif()
    message("FLEX: "  ${FLEX_LM_SRC_DIR})
endif(PRODUCTION_BUILD)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

add_executable(analyze ${CMAKE_CURRENT_SOURCE_DIR}/src/analyze.cc)

list(APPEND DE_CXX_FLAGS_DEBUG
    -Werror -Wall -O0 -D_REENTRANT -g ${MEM_SANITIZER_FLAGS} ${CXX_DEFINES}
)
list(APPEND DE_CXX_FLAGS_RELEASE
    -Werror -Wall -O2 -D_REENTRANT -DNDEBUG ${CXX_DEFINES}
)

get_filename_component(VERIFIC_HOME "../../Raptor_Tools/verific_rs"
    REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
message("VERIFIC_HOME: ${VERIFIC_HOME}")

file(GLOB_RECURSE LIB_HEADERS
    ${VERIFIC_HOME}/*/*.h)

files_to_dirs(LIB_HEADERS LIB_INCLUDE_DIRS)

include_directories(analyze PUBLIC ${LIB_INCLUDE_DIRS}
                            ${FLEX_LM_SRC_DIR}
                            ${FLEX_LM_SRC_DIR}/machind
)

target_include_directories(analyze PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/)

add_definitions(-DVHDL_PACKAGES="${VERIFIC_HOME}/vhdl_packages")

if (APPLE)
    set(OS "mac")
elseif (UNIX)
    set(OS "linux")
endif()

list(APPEND VERIFIC_LIBS
    ${VERIFIC_HOME}/verilog/verilog-${OS}.a
    ${VERIFIC_HOME}/database/database-${OS}.a
    ${VERIFIC_HOME}/util/util-${OS}.a
    ${VERIFIC_HOME}/containers/containers-${OS}.a
    ${VERIFIC_HOME}/hier_tree/hier_tree-${OS}.a
    ${VERIFIC_HOME}/vhdl/vhdl-${OS}.a
    )

target_link_libraries(analyze PUBLIC ${VERIFIC_LIBS})

target_compile_options(analyze PUBLIC $<$<CONFIG:DEBUG>:${DE_CXX_FLAGS_DEBUG}>)
target_compile_options(analyze PUBLIC $<$<CONFIG:RELEASE>:${DE_CXX_FLAGS_RELEASE}>)

set(ANALYZE_BIN  ${CMAKE_CURRENT_SOURCE_DIR}/install/bin)
set(ANALYZE_SHARE  ${CMAKE_CURRENT_SOURCE_DIR}/install/share)

install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/analyze
    DESTINATION ${ANALYZE_BIN})
install(
    DIRECTORY ${VERIFIC_HOME}/vhdl_packages/
    DESTINATION ${ANALYZE_SHARE}/vhdl_packages)
