#pragma once

#include <cstdint>
#include <gtest/gtest.h>
#include <vector>


#include "cw/frame.h"

namespace cw::test {

// Helper to build a ParsedFrame from any packet type.
// Stores the frame buffer internally to keep the span valid.
class FrameBuilder {
public:
  template <packet::FrameBuildable P> packet::ParsedFrame build(const P &pkt) {
    m_buffer = packet::buildFrame(pkt);
    return packet::parseFrame(m_buffer);
  }

private:
  std::vector<uint8_t> m_buffer;
};

// Helper to create a byte vector from initializer list
inline std::vector<uint8_t> bytes(std::initializer_list<uint8_t> init) {
  return std::vector<uint8_t>(init);
}

// Helper to create frame header manually for testing malformed frames
inline std::vector<uint8_t> makeFrameHeader(uint64_t payloadLen,
                                            uint16_t type) {
  std::vector<uint8_t> header;
  header.reserve(10);

  // Big-endian payload length (8 bytes)
  for (int i = 7; i >= 0; --i) {
    header.push_back(static_cast<uint8_t>((payloadLen >> (i * 8)) & 0xFF));
  }
  // Big-endian type (2 bytes)
  header.push_back(static_cast<uint8_t>((type >> 8) & 0xFF));
  header.push_back(static_cast<uint8_t>(type & 0xFF));

  return header;
}

} // namespace cw::test
