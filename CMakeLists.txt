cmake_minimum_required(VERSION 3.30)
project(ConnectWith)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(asio INTERFACE)
target_include_directories(asio INTERFACE "${CMAKE_SOURCE_DIR}/include")


add_executable(ConnectWith "src/main.cpp"   "src/endian.h" "src/protocol/packet_type.h" "src/protocol/file_info_packet.h" "src/protocol/file_chunk_packet.h" "src/protocol/file_done_packet.h" "src/protocol/error_packet.h" "src/protocol/packet_registry.h")


target_link_libraries(ConnectWith PRIVATE asio)
if(WIN32)
	target_link_libraries(ConnectWith PRIVATE ws2_32)
endif()