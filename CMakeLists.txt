cmake_minimum_required(VERSION 3.30)
project(ConnectWith)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE "${CMAKE_SOURCE_DIR}/include")

add_library(cw INTERFACE)
target_include_directories(cw INTERFACE 
    "${CMAKE_SOURCE_DIR}/src"            
    "${CMAKE_SOURCE_DIR}/src/cw/protocol" 
)
target_link_libraries(cw INTERFACE asio)

include(FetchContent)
message(STATUS "Fetching GoogleTest...")

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)
enable_testing()


add_executable(ConnectWith 
    "src/main.cpp"
    "src/cw/endian.h" 
    "src/cw/protocol/packet/packet_type.h"
    "src/cw/protocol/packet/packet_registry.h"
    "src/cw/protocol/packet/packet.h" 
    "src/cw/Frame.h"
)

target_link_libraries(ConnectWith PRIVATE cw)

if(WIN32)
    target_link_libraries(ConnectWith PRIVATE ws2_32)
endif()


add_executable(unit_tests 
    "tests/main_test.cpp" # Your GTest file
)

# Link GTest and your Code (cw)
target_link_libraries(unit_tests PRIVATE 
    GTest::gtest_main 
    cw
)

if(WIN32)
    target_link_libraries(unit_tests PRIVATE ws2_32)
endif()

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(unit_tests)