cmake_minimum_required(VERSION 3.16)
project(microvpr CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(MICROVPR_ENABLE_SANITIZERS "Build with ASan/UBSan" OFF)

add_library(microvpr_core
    src/netlist.cpp
    src/blif_parser.cpp
    src/netlist_evaluator.cpp
    src/tech_mapper.cpp
)
target_include_directories(microvpr_core PUBLIC include)
target_compile_options(microvpr_core PRIVATE -Wall -Wextra)

if(MICROVPR_ENABLE_SANITIZERS)
    target_compile_options(microvpr_core PUBLIC -fsanitize=address,undefined -g)
    target_link_options(microvpr_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(microvpr_cli src/main.cpp)
target_link_libraries(microvpr_cli PRIVATE microvpr_core)

enable_testing()
add_subdirectory(tests)
