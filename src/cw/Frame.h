#pragma once
#include <vector>
#include <cstdint>
#include <concepts>
#include "protocol/packet/packet_type.h"
#include "protocol/packet/packet.h"

namespace cw {
	namespace packet {

		template<typename T>
		concept FrameBuildable =
			requires(const T pkt) {
				{ T::type } -> std::convertible_to<cw::packet::PacketType>;
				{ pkt.serialize() } -> std::same_as<std::vector<uint8_t>>;
		};


		template<FrameBuildable P>
		std::vector<uint8_t> buildFrame(const P & packet) {


			return std::vector<uint8_t>(10);



		}
	}

}