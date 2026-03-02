######################################################################
# CMakeLists.txt - Overhead (Backend) build configuration
#
# Builds the Overhead process for Linux PREEMPT_RT
######################################################################

cmake_minimum_required(VERSION 3.16)
project(overhead LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(OVERHEAD_SOURCES
    Main.cpp
    overhead.cpp
    Utils.cpp
    Serial.cpp
    LoadCell.cpp
    1510Loadcell.cpp
    HBMLoadCell.cpp
    io.cpp
    3724_io.cpp
    3724_io_2.cpp
    3724_io_3.cpp
    3724_io_4.cpp
    Vsbc6_io.cpp
    InterSystems.cpp
    DropManager.cpp
    Simulator.cpp
    hbm_sim.cpp
    ispd_sim.cpp
    ISPDLoadCell.cpp
    backendshell.cpp
    ../interface/console.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface
)

# Build the executable
add_executable(overhead ${OVERHEAD_SOURCES})

# Link libraries
target_link_libraries(overhead
    pthread
    rt          # POSIX real-time extensions (shm_open, timers, etc.)
)

# Compiler flags
# -fpermissive: Original MSVC code uses implicit int return types extensively
# -g: Debug symbols for stack traces
# -rdynamic: Export symbols for backtrace_symbols
target_link_options(overhead PRIVATE -no-pie -rdynamic)
target_compile_options(overhead PRIVATE
    -fpermissive
    -g
    -O0
    -fno-pie
    -rdynamic
    -fno-omit-frame-pointer
    -Wall
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
    -Wno-write-strings
    -Wno-unused-value
    -Wno-parentheses
    -Wno-format
    -Wno-narrowing
    -Wno-missing-field-initializers
    -Wno-reorder
    -Wno-register
    -Wno-comment
)

# Preprocessor defines
# Most are already in overheadconst.h
# NOTE: _SIMULATION_MODE_ and _WEIGHT_SIMULATION_MODE_ are controlled in
# overheadconst.h - do NOT add them here or they override the header.
# Uncomment in overheadconst.h to enable simulation mode for testing.
target_compile_definitions(overhead PRIVATE
)

# Install target
install(TARGETS overhead DESTINATION /opt/dchservices/bin)
