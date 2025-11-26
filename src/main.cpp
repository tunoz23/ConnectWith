#include <iostream>
#include <print>
#include <asio.hpp>
#include "endian.h"
#include "protocol/file_chunk_packet.h"

int main(
         )
{
	cw::packet::FileChunk a;
	a.offset = 100;
	a.data = { 1,2,3,4,5 };
	auto bytes = a.serialize();
	auto b = cw::packet::FileChunk::deserialize(bytes.data(), bytes.size());

    return 0;
}