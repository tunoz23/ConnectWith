#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>
#include "packet_type.h"
#include "../endian.h"

namespace cw {
	namespace packet {

		struct AckPacket
		{
			static constexpr PacketType type = PacketType::Ack;

			std::uint64_t offset;   

			std::vector<uint8_t> serialize() const
			{
				std::vector<uint8_t> out;
				out.resize(sizeof(offset));

				// Write offset at position 0
				cw::endian::writeBigEndian<uint64_t>(out.data(), offset);

				return out;
			}

			static AckPacket deserialize(const uint8_t* buf, size_t size)
			{
				if (size < sizeof(std::uint64_t))
					throw std::runtime_error("AckPacket payload too small.");

				AckPacket packet;

				packet.offset = cw::endian::readBigEndian<uint64_t>(buf);

				return packet;
			}
		};

	} 
}