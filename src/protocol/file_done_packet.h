#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include "../endian.h"
#include "packet_type.h"

namespace cw 
{
	namespace packet
	{

		struct FileDone
		{
			static constexpr PacketType type = PacketType::FileDone;

			std::uint64_t fileSize;   

			std::vector<uint8_t> serialize() const
			{
				std::vector<uint8_t> out;
				out.resize(sizeof(fileSize));

				// fileSize at offset 0
				cw::endian::writeBigEndian<uint64_t>(out.data(), fileSize);

				return out;
			}

			static FileDone deserialize(const uint8_t* buf, size_t size)
			{
				if (size < sizeof(std::uint64_t))
					throw std::runtime_error("FileDone payload too small.");

				FileDone packet;
				packet.fileSize = cw::endian::readBigEndian<uint64_t>(buf);

				return packet;
			}
		};

	} 
}