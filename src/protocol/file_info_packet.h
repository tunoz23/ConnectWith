#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cstring>
#include "../endian.h"
#include "packet_type.h"

namespace cw {
	namespace packet {

		struct FileInfo
		{
			static constexpr PacketType type = PacketType::FileInfo;

			std::uint64_t fileSize;
			std::string fileName;

			std::vector<uint8_t> serialize() const
			{
				const auto nameLen = static_cast<std::uint32_t>(fileName.size());

				std::vector<uint8_t> out;
				out.resize(sizeof(fileSize) + sizeof(nameLen) + nameLen);

				uint8_t* ptr = out.data();

				cw::endian::writeBigEndian<uint64_t>(ptr, fileSize);

				cw::endian::writeBigEndian<uint32_t>(ptr + sizeof(fileSize), nameLen);

				if (nameLen > 0)
				{
					std::memcpy(ptr + sizeof(fileSize) + sizeof(nameLen),
						fileName.data(),
						nameLen);
				}

				return out;
			}

			static FileInfo deserialize(const uint8_t* buf, size_t size)
			{
				if (size < 12)
					throw std::runtime_error("FileInfo payload too small.");

				FileInfo info;

				info.fileSize = cw::endian::readBigEndian<uint64_t>(buf);

				std::uint32_t nameLen = cw::endian::readBigEndian<uint32_t>(buf + sizeof(info.fileSize));

				if (12 + nameLen > size)
					throw std::runtime_error("FileInfo corrupted: name length mismatch.");

				if (nameLen > 0)
				{
					info.fileName.assign(
						reinterpret_cast<const char*>(buf + 12),
						nameLen
					);
				}
				else
				{
					info.fileName.clear();
				}

				return info;
			}
		};

	} 
}