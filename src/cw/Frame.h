#pragma once
#include <vector>
#include <cstdint>
#include <concepts>
#include <stdexcept>

// Ensure this matches your file name (e.g. src/cw/endian.h)
#include "endian.h" 
#include "protocol/packet/packet_type.h"

namespace cw {
	namespace packet {

		// View to parsed frame buffer.
		// Optimized Order: Pointer (8) -> Size (8) -> Type (2) -> Padding (6)
		struct ParsedFrame
		{
			const uint8_t* payload_view; // Reverted to payload_view as requested
			std::size_t size;
			PacketType type;
		};

		template<typename T>
		concept FrameBuildable =
			requires(const T pkt, std::vector<uint8_t>&out) {
				{ T::type } -> std::convertible_to<cw::packet::PacketType>;
				{ pkt.payloadSize() } -> std::same_as<std::size_t>;
				{ pkt.serialize(out) } -> std::same_as<void>;
		};

		template<FrameBuildable P>
		std::vector<uint8_t> buildFrame(const P& packet) {

			std::size_t payloadSz = packet.payloadSize();

			// Header: [Type: 2 bytes] + [Length: 8 bytes]
			constexpr std::size_t HEADER_SIZE = sizeof(uint16_t) + sizeof(uint64_t);
			std::size_t totalSize = HEADER_SIZE + payloadSz;

			std::vector<uint8_t> result;
			result.reserve(totalSize);

			// Note: We write LENGTH (8 bytes) then TYPE (2 bytes)
			// Use 'template' keyword for MSVC compatibility
			cw::binary::template writeBigEndian<uint64_t>(result, static_cast<uint64_t>(payloadSz));
			cw::binary::template writeBigEndian<uint16_t>(result, static_cast<uint16_t>(P::type));

			packet.serialize(result);

			return result;
		}

		inline ParsedFrame parseFrame(const std::vector<uint8_t>& buffer) {

			constexpr size_t HEADER_SIZE = sizeof(uint16_t) + sizeof(uint64_t);

			if (buffer.size() < HEADER_SIZE) {
				throw std::runtime_error("Incomplete Frame Header");
			}

			const uint8_t* ptr = buffer.data();

			// Read Length (8 bytes)
			uint64_t payloadLen = cw::binary::template readBigEndian<uint64_t>(ptr);
			ptr += sizeof(uint64_t); // Advance pointer

			// Bounds Check (Safe Subtraction)
			if (buffer.size() - HEADER_SIZE < payloadLen)
			{
				throw std::runtime_error("Incomplete Frame Body");
			}

			// Read Type (2 bytes)
			// Added 'template' keyword here too
			uint16_t typeVal = cw::binary::template readBigEndian<uint16_t>(ptr);
			ptr += sizeof(uint16_t);

			ParsedFrame result;
			result.payload_view = ptr;      // Using payload_view as requested
			result.size = static_cast<size_t>(payloadLen);
			result.type = static_cast<PacketType>(typeVal);

			return result;
		}
	}
}