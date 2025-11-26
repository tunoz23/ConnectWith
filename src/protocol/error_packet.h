#pragma once



#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <string>
#include "packet_type.h"
#include "../endian.h"

namespace cw {
	namespace packet {

		struct Error
		{
			static constexpr PacketType type = PacketType::Error;

			std::uint16_t code;
			std::string message;

			std::vector<uint8_t> serialize() const
			{
				uint32_t msgLen = static_cast<uint32_t>(message.size());
				std::vector<uint8_t> out;
				out.resize(sizeof(code) + sizeof(msgLen) + msgLen);

				uint8_t* ptr = out.data();
				cw::endian::writeBigEndian<uint16_t>(ptr, code);
				cw::endian::writeBigEndian<uint32_t>(ptr + sizeof(code), msgLen);

				if (msgLen > 0)
					std::memcpy(ptr + sizeof(code) + sizeof(msgLen), message.data(), msgLen);

				return out;
			}

			static Error deserialize(const uint8_t* buf, size_t size)
			{
				if (size < sizeof(std::uint16_t) + sizeof(std::uint32_t))
					throw std::runtime_error("ErrorPacket payload too small");

				Error p;

				p.code = cw::endian::readBigEndian<uint16_t>(buf);
				uint32_t msgLen = cw::endian::readBigEndian<uint32_t>(buf + sizeof(p.code));

				if (sizeof(p.code) + sizeof(msgLen) + msgLen > size)
					throw std::runtime_error("ErrorPacket corrupted");

				p.message.resize(msgLen);
				if (msgLen > 0)
					std::memcpy(p.message.data(), buf + sizeof(p.code) + sizeof(msgLen), msgLen);

				return p;
			}
		};

	}
}
