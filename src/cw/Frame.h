#pragma once
#include <vector>
#include <cstdint>
#include <concepts>
#include "protocol/packet/packet_type.h"

namespace cw {
	namespace packet {

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


		void parseFrame(const std::vector<uint8_t>& buffer)
		{
			 
		
		
		}


	}
}