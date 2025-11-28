#pragma once
#include <cstdint>



namespace cw {
	namespace packet {

		enum class PacketType : uint16_t {
			FileInfo = 1,
			FileChunk,
			FileDone,
			Error,
			Ack
		};
	}
}