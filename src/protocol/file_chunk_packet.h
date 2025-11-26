#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>
#include "packet_type.h"
#include "../endian.h"
namespace cw 
{

	namespace packet
	{

		struct FileChunk
		{
			static constexpr PacketType type = PacketType::FileChunk;
			
			std::uint64_t offset;
			std::uint32_t length;
			std::vector<uint8_t> data;


			std::vector<uint8_t> serialize() 
			{
				length = static_cast<std::uint32_t>(data.size());
	
				if (data.size() > UINT32_MAX)
					throw std::runtime_error("Chunk too large.");





				std::vector<uint8_t> out;
				out.resize(sizeof(offset) + sizeof(length) + data.size());
				

				cw::endian::writeBigEndian<uint64_t>(out.data(), offset);

				endian::writeBigEndian<uint32_t>(out.data() + sizeof(offset), length);

				if (!data.empty()) 
					std::memcpy(out.data() + sizeof(offset) + sizeof(length), data.data(), data.size());
				

				return out;
			}

			static FileChunk deserialize(const uint8_t* buf, size_t size)
			{
				if (size < 12) throw std::runtime_error("FileChunk payload is too small.");
				
				FileChunk chunk;

				chunk.offset = endian::readBigEndian<uint64_t>(buf);

				// Read length
				chunk.length = endian::readBigEndian<uint32_t>(buf + sizeof(FileChunk::offset));

				// Validate size
				if (12 + chunk.length > size)
					throw std::runtime_error("FileChunk corrupted: length mismatch");

				// Copy data
				chunk.data.assign(buf + 12, buf + 12 + chunk.length);

				return chunk;
			}
		};
	}
}