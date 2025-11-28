cmake_minimum_required(VERSION 3.30)
project(ConnectWith)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(asio INTERFACE)
target_include_directories(asio INTERFACE "${CMAKE_SOURCE_DIR}/include")
add_library(cw INTERFACE)
target_include_directories(cw INTERFACE "${CMAKE_SOURCE_DIR}/src/cw/protocol")
target_include_directories(cw INTERFACE "${CMAKE_SOURCE_DIR}/src/cw")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(ConnectWith "src/main.cpp"   "src/cw/endian.h" "src/cw/protocol/packet/packet_type.h"
"src/cw/protocol/packet/packet_registry.h"
"src/cw/protocol/packet/packet.h"  "src/cw/Frame.h")


target_link_libraries(ConnectWith PRIVATE asio cw)
if(WIN32)
	target_link_libraries(ConnectWith PRIVATE ws2_32)
endif()