#pragma once

#include <cstdint>

namespace cw::packet {

enum class PacketType : uint16_t {
  Handshake = 0,
  FileInfo = 1,
  FileChunk = 2,
  FileDone = 3,
  Error = 4,
  Ack = 5
};

} // namespace cw::packet