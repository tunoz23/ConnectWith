cmake_minimum_required(VERSION 3.30)
project(ConnectWith)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# OPTIONS
# =============================================================================

option(BUILD_TESTING "Build tests" ON)
option(ENABLE_COVERAGE "Enable code coverage (GCC/Clang)" OFF)
option(ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)

# =============================================================================
# COMPILER FLAGS
# =============================================================================

if(ENABLE_COVERAGE AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

if(ENABLE_SANITIZERS AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

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
    "src/cw/file/i_file_writer.h"
    "src/cw/file/path_validator.h"
    "src/cw/file/file_writer.h"
    "src/cw/file/file_transfer.h"
    "src/cw/file/transfer_orchestrator.h"
    "src/cw/protocol/packet/packet_type.h"
    "src/cw/protocol/packet/packet.h"
    "src/cw/network/packet_handler.h"
    "src/cw/network/i_sender.h"
    "src/cw/network/file_receiver.h"
    "src/cw/network/connection.h"
    "src/cw/network/server.h"
    "src/cw/network/client.h"
)

add_library(cw INTERFACE)
target_include_directories(cw INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(cw INTERFACE asio)
target_sources(cw INTERFACE ${CW_HEADERS})

# =============================================================================
# 2. GOOGLE TEST
# =============================================================================

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# =============================================================================
# 3. EXECUTABLES
# =============================================================================

# Server executable
add_executable(Server "src/server_main.cpp")
target_link_libraries(Server PRIVATE cw)
if(WIN32)
    target_link_libraries(Server PRIVATE ws2_32 mswsock)
endif()

# Client executable
add_executable(Client "src/client.cpp")
target_link_libraries(Client PRIVATE cw)
if(WIN32)
    target_link_libraries(Client PRIVATE ws2_32 mswsock)
endif()

# =============================================================================
# 4. UNIT TESTS
# =============================================================================

if(BUILD_TESTING)
    # Unit tests - pure logic, no I/O
    add_executable(unit_tests
        "tests/unit/binary_test.cpp"
        "tests/unit/frame_test.cpp"
        "tests/unit/packet_test.cpp"
        "tests/unit/file_receiver_test.cpp"
    )
    target_link_libraries(unit_tests PRIVATE GTest::gtest_main GTest::gmock cw)
    target_include_directories(unit_tests PRIVATE "${CMAKE_SOURCE_DIR}/tests/common")
    if(WIN32)
        target_link_libraries(unit_tests PRIVATE ws2_32 mswsock)
    endif()

    # Integration tests - uses filesystem, real modules
    add_executable(integration_tests
        "tests/integration/transfer_test.cpp"
        "tests/integration/path_validator_test.cpp"
    )
    target_link_libraries(integration_tests PRIVATE GTest::gtest_main GTest::gmock cw)
    target_include_directories(integration_tests PRIVATE "${CMAKE_SOURCE_DIR}/tests/common")
    if(WIN32)
        target_link_libraries(integration_tests PRIVATE ws2_32 mswsock)
    endif()

    # Register tests with CTest
    include(GoogleTest)
    gtest_discover_tests(unit_tests TEST_PREFIX "unit.")
    gtest_discover_tests(integration_tests TEST_PREFIX "integration.")
endif()