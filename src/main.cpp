#include <iostream>
#include <print>
#include <asio.hpp>
#include "endian.h"
#include "packet/packet.h"
#include "Frame.h"

int main(
         )
{
	cw::packet::FileChunk a;
	a.offset = 100;
	a.data = { 1,2,3,4,5 };
	a.length = a.data.size();
	auto bytes = a.serialize();
	auto b = cw::packet::FileChunk::deserialize(bytes.data(), bytes.size());

	auto k = cw::packet::buildFrame(a);

    return 0;
}