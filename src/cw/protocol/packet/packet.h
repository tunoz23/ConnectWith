#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <limits>
#include "packet_type.h"
#include "../endian.h"

namespace cw::packet
{

	constexpr size_t MAX_STRING_LENGTH = 4096;        // 4 KB limit for messages/filenames
	constexpr size_t MAX_CHUNK_SIZE = 10 * 1024 * 1024; // 10 MB limit for file chunks

	struct Ack
	{
		static constexpr PacketType type = PacketType::Ack;
		std::uint64_t offset;

		std::size_t payloadSize() const { return sizeof(offset); }

		void serialize(std::vector<uint8_t>& out) const
		{
			cw::binary::writeBigEndian<uint64_t>(out, offset);
		}

		static Ack deserialize(const uint8_t* buf, size_t size)
		{
			if (size < sizeof(offset))
				throw std::runtime_error("Ack: payload too small.");

			Ack packet;
			packet.offset = cw::binary::readBigEndian<uint64_t>(buf);
			return packet;
		}
	};

	struct Error
	{
		static constexpr PacketType type = PacketType::Error;
		std::uint16_t code;
		std::string message;

		std::size_t payloadSize() const {
			return sizeof(code) + sizeof(uint32_t) + message.size();
		}

		void serialize(std::vector<uint8_t>& out) const
		{
			if (message.size() > MAX_STRING_LENGTH)
				throw std::length_error("Error: Message exceeds protocol limit.");

			cw::binary::writeBigEndian(out, code);
			cw::binary::writeBigEndian(out, static_cast<uint32_t>(message.size()));
			out.insert(out.end(), message.begin(), message.end());
		}

		static Error deserialize(const uint8_t* buf, size_t size)
		{
			constexpr size_t MIN_SIZE = sizeof(code) + sizeof(uint32_t);
			if (size < MIN_SIZE)
				throw std::runtime_error("Error: payload too small");

			Error p;
			size_t cursor = 0;

			p.code = cw::binary::readBigEndian<uint16_t>(buf + cursor);
			cursor += sizeof(p.code);

			uint32_t msgLen = cw::binary::readBigEndian<uint32_t>(buf + cursor);
			cursor += sizeof(msgLen);

			// SECURITY: Sanity Check before allocation
			if (msgLen > MAX_STRING_LENGTH)
				throw std::runtime_error("Error: Message length unreasonable (DoS protection).");

			// SAFETY: Subtraction prevents integer overflow
			if (size - cursor < msgLen)
				throw std::runtime_error("Error: declared length exceeds buffer.");

			if (msgLen > 0)
				p.message.assign(reinterpret_cast<const char*>(buf + cursor), msgLen);

			return p;
		}
	};

	struct FileChunk
	{
		static constexpr PacketType type = PacketType::FileChunk;
		std::uint64_t offset;
		// REMOVED: std::uint32_t length; -> Redundant. data.size() is the truth.
		std::vector<uint8_t> data;

		std::size_t payloadSize() const {
			return sizeof(offset) + sizeof(uint32_t) + data.size();
		}

		void serialize(std::vector<uint8_t>& out) const
		{
			if (data.size() > MAX_CHUNK_SIZE)
				throw std::length_error("Chunk: Data exceeds protocol limit.");

			// Explicitly cast size to uint32 check to ensure it fits field
			if (data.size() > UINT32_MAX)
				throw std::length_error("Chunk: Data too large for u32 field.");

			cw::binary::writeBigEndian(out, offset);
			cw::binary::writeBigEndian(out, static_cast<uint32_t>(data.size()));
			out.insert(out.end(), data.begin(), data.end());
		}

		static FileChunk deserialize(const uint8_t* buf, size_t size)
		{
			constexpr size_t HEADER_SIZE = sizeof(offset) + sizeof(uint32_t);
			if (size < HEADER_SIZE) throw std::runtime_error("FileChunk: payload too small.");

			FileChunk chunk;
			size_t cursor = 0;

			chunk.offset = binary::readBigEndian<uint64_t>(buf + cursor);
			cursor += sizeof(offset);

			uint32_t dataLen = binary::readBigEndian<uint32_t>(buf + cursor);
			cursor += sizeof(dataLen);

			// SECURITY: Check logic
			if (dataLen > MAX_CHUNK_SIZE)
				throw std::runtime_error("FileChunk: Size unreasonable (DoS protection).");

			// SAFETY: Overflow check
			if (size - cursor < dataLen)
				throw std::runtime_error("FileChunk: corrupted length mismatch");

			// Allocation
			chunk.data.assign(buf + cursor, buf + cursor + dataLen);

			return chunk;
		}
	};

	struct FileDone
	{
		static constexpr PacketType type = PacketType::FileDone;
		std::uint64_t fileSize;

		std::size_t payloadSize() const { return sizeof(fileSize); }

		void serialize(std::vector<uint8_t>& out) const
		{
			cw::binary::writeBigEndian<uint64_t>(out, fileSize);
		}

		static FileDone deserialize(const uint8_t* buf, size_t size)
		{
			if (size < sizeof(fileSize))
				throw std::runtime_error("FileDone: payload too small.");

			FileDone packet;
			packet.fileSize = cw::binary::readBigEndian<uint64_t>(buf);
			return packet;
		}
	};

	struct FileInfo
	{
		static constexpr PacketType type = PacketType::FileInfo;
		std::uint64_t fileSize;
		std::string fileName;

		std::size_t payloadSize() const {
			return sizeof(fileSize) + sizeof(uint32_t) + fileName.size();
		}

		void serialize(std::vector<uint8_t>& out) const
		{
			if (fileName.empty()) throw std::length_error("FileInfo: Filename empty");
			if (fileName.size() > MAX_STRING_LENGTH) throw std::length_error("FileInfo: Filename too long");

			cw::binary::writeBigEndian(out, fileSize);
			cw::binary::writeBigEndian(out, static_cast<uint32_t>(fileName.size()));
			out.insert(out.end(), fileName.begin(), fileName.end());
		}

		static FileInfo deserialize(const uint8_t* buf, size_t size)
		{
			constexpr size_t MIN_SIZE = sizeof(fileSize) + sizeof(uint32_t);
			if (size < MIN_SIZE) throw std::runtime_error("FileInfo: payload too small.");

			FileInfo info;
			size_t cursor = 0;

			info.fileSize = cw::binary::readBigEndian<uint64_t>(buf + cursor);
			cursor += sizeof(info.fileSize);

			uint32_t nameLen = cw::binary::readBigEndian<uint32_t>(buf + cursor);
			cursor += sizeof(nameLen);

			if (nameLen > MAX_STRING_LENGTH)
				throw std::runtime_error("FileInfo: Filename too long (DoS protection).");

			if (size - cursor < nameLen)
				throw std::runtime_error("FileInfo: corrupted name length mismatch.");

			if (nameLen > 0)
				info.fileName.assign(reinterpret_cast<const char*>(buf + cursor), nameLen);

			return info;
		}
	};
}