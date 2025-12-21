cmake_minimum_required(VERSION 3.30)
project(ConnectWith)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# =============================================================================
# 1. LIBRARIES
# =============================================================================

# ASIO (header-only networking library)
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE "${CMAKE_SOURCE_DIR}/include")

# Core library (header-only)
set(CW_HEADERS
    "src/cw/binary.h"
    "src/cw/frame.h"
    "src/cw/protocol/packet/packet_type.h"
    "src/cw/protocol/packet/packet.h"
    "src/cw/network/packet_handler.h"
    "src/cw/network/file_receiver.h"
    "src/cw/network/connection.h"
    "src/cw/network/server.h"
    "src/cw/network/client.h"
    "src/cw/file/file_transfer.h"
)

add_library(cw INTERFACE)
target_include_directories(cw INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(cw INTERFACE asio)
target_sources(cw INTERFACE ${CW_HEADERS})

# =============================================================================
# 2. GOOGLE TEST
# =============================================================================

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()

# =============================================================================
# 3. EXECUTABLES
# =============================================================================

# Server executable
add_executable(Server "src/server_main.cpp")
target_link_libraries(Server PRIVATE cw)
if(WIN32)
    target_link_libraries(Server PRIVATE ws2_32)
endif()

# Client executable
add_executable(Client "src/client.cpp")
target_link_libraries(Client PRIVATE cw)
if(WIN32)
    target_link_libraries(Client PRIVATE ws2_32)
endif()

# =============================================================================
# 4. UNIT TESTS
# =============================================================================

add_executable(unit_tests "tests/packet_test.cpp")
target_link_libraries(unit_tests PRIVATE GTest::gtest_main cw)
if(WIN32)
    target_link_libraries(unit_tests PRIVATE ws2_32)
endif()

include(GoogleTest)
gtest_discover_tests(unit_tests)