######################################################################
# CMakeLists.txt - DCH Server (Interface) build configuration
#
# Builds the Interface process for Linux PREEMPT_RT
######################################################################

cmake_minimum_required(VERSION 3.16)
project(dch_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(INTERFACE_SOURCES
    main.cpp
    tcpserver.cpp
    tcpclient.cpp
    ipc_mailbox.cpp
    console.cpp
    telnetsrv_stub.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# Build the executable
add_executable(interface ${INTERFACE_SOURCES})

# Link libraries
target_link_libraries(interface
    pthread
    rt          # POSIX real-time extensions (shm_open, etc.)
)

# Compiler flags
target_compile_options(interface PRIVATE
    -Wall
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
    -Wno-write-strings
)

# Preprocessor defines (most are already in overheadconst.h)
target_compile_definitions(interface PRIVATE
)

# Install target
install(TARGETS interface DESTINATION /opt/dchservices/bin)
