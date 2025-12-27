cmake_minimum_required(VERSION 3.30)
project(ConnectWith)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. SHARED LIBRARIES ---

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE "${CMAKE_SOURCE_DIR}/include")

# The Core Logic (Packets, Frames, Connection)
# We define sources here so they show up in IDEs (Visual Studio/CLion)
set(CW_SOURCES
    "src/cw/endian.h"
    "src/cw/Frame.h"
    "src/cw/utf8.h"
    "src/cw/network/connection.h"
    "src/cw/protocol/packet/packet.h"
    "src/cw/protocol/packet/packet_type.h"
    "src/cw/protocol/packet/packet_registry.h"
)

add_library(cw INTERFACE)
target_include_directories(cw INTERFACE 
    "${CMAKE_SOURCE_DIR}/src"            
    "${CMAKE_SOURCE_DIR}/src/cw/protocol" 
)
target_link_libraries(cw INTERFACE asio)
target_sources(cw INTERFACE ${CW_SOURCES}) 

# --- 2. GOOGLE TEST ---
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()

# --- 3. EXECUTABLE: SERVER ---

add_executable(Server 
    "src/main.cpp" # Contains Server Main
 "src/cw/file/file.h" "src/cw/network/Server.h" "src/cw/network/Client.h")
target_link_libraries(Server PRIVATE cw)
if(WIN32)
    target_link_libraries(Server PRIVATE ws2_32 mswsock)
endif()

# --- 4. EXECUTABLE: CLIENT ---

add_executable(Client 
    "src/client.cpp" # Contains Client Main
 "src/cw/file/file.h" "src/cw/network/Server.h" "src/cw/network/Client.h")
target_link_libraries(Client PRIVATE cw)
if(WIN32)
    target_link_libraries(Client PRIVATE ws2_32 mswsock)
endif()

# --- 5. UNIT TESTS ---

add_executable(unit_tests 
    "tests/main_test.cpp" # Contains GTest Main
 "src/cw/file/file.h" "src/cw/network/Server.h" "src/cw/network/Client.h")
target_link_libraries(unit_tests PRIVATE GTest::gtest_main cw)
if(WIN32)
    target_link_libraries(unit_tests PRIVATE ws2_32 mswsock)
endif()

include(GoogleTest)
gtest_discover_tests(unit_tests)