#pragma once


#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include "packet_type.h"
#include "../endian.h"

namespace cw
{


	namespace packet
	{


		struct Ack
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

			static Ack deserialize(const uint8_t* buf, size_t size)
			{
				if (size < sizeof(std::uint64_t))
					throw std::runtime_error("AckPacket payload too small.");

				Ack packet;

				packet.offset = cw::endian::readBigEndian<uint64_t>(buf);

				return packet;
			}
		};

	
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


		struct FileChunk
		{
			static constexpr cw::packet::PacketType type = cw::packet::PacketType::FileChunk;

			std::uint64_t offset;
			std::uint32_t length;
			std::vector<uint8_t> data;


			std::vector<uint8_t> serialize()  const
			{

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