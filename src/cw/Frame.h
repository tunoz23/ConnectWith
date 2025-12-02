#pragma once
#include <vector>
#include <cstdint>
#include <concepts>
#include "protocol/packet/packet_type.h"

namespace cw {
	namespace packet {


		//View to parsed frame buffer.
		struct ParsedFrame
		{
			std::size_t size;
			const uint8_t* payload_view;
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


			cw::endian::writeBigEndian<uint64_t>(result, static_cast<uint64_t>(payloadSz));

			cw::endian::writeBigEndian<uint16_t>(result, static_cast<uint16_t>(P::type));

			packet.serialize(result);

			return result;
		}


		ParsedFrame parseFrame(const std::vector<uint8_t>& buffer) {

			constexpr size_t HEADER_SIZE = sizeof(uint16_t) + sizeof(uint64_t);

			if (buffer.size() < HEADER_SIZE) {
				// In a real async system, we wouldn't throw here; 
				// we would tell the socket "Keep reading, I need more data".
				// But for now, throw or return a specific error type.
				throw std::runtime_error("Incomplete Frame Header");
			}

			// 2. Read Length (Big Endian -> Host)
			// Note: We read length first because usually it comes first in standard protocols,
			// but your buildFrame wrote Length then Type. We must match buildFrame.
			// Your buildFrame: Length (8), Type (2)

			const uint8_t* ptr = buffer.data();

			uint64_t payloadLen = cw::endian::readBigEndian<uint64_t>(ptr);
			ptr += sizeof(uint64_t); // Advance pointer

			// 3. Completeness Check
			// Do we actually have the payload we were promised?
			// Total needed = Header (10) + Payload (N)
			if (buffer.size() - HEADER_SIZE < payloadLen) {
				throw std::runtime_error("Incomplete Frame Body");
			}

			// 4. Read Type (Big Endian -> Host)
			uint16_t typeVal = cw::endian::readBigEndian<uint16_t>(ptr);
			ptr += sizeof(uint16_t); // Advance pointer (now points to Payload)

			// 5. Construct the View
			ParsedFrame result;
			result.type = static_cast<PacketType>(typeVal);
			result.payload_view = ptr;       // Pointing to buffer.data() + 10
			result.size = static_cast<size_t>(payloadLen);

			return result;
		}

	}
}