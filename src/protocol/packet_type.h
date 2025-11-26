#pragma once
#include <cstdint>

enum class PacketType : uint16_t {
	FileInfo = 1,
	FileChunk,
	FileDone,
	Error,
	Ack
};