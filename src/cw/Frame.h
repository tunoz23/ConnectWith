#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "binary.h"
#include "protocol/packet/packet_type.h"

namespace cw::packet {

// Protocol constants
inline constexpr std::size_t kFrameHeaderSize =
    sizeof(uint64_t) + sizeof(uint16_t); // 10 bytes

// Zero-copy view into a parsed frame buffer
struct ParsedFrame {
  std::span<const uint8_t> payload;
  PacketType type;

  [[nodiscard]] std::size_t size() const noexcept { return payload.size(); }
  [[nodiscard]] std::size_t totalSize() const noexcept {
    return kFrameHeaderSize + payload.size();
  }
};

// Concept for types that can be serialized into a frame
template <typename T>
concept FrameBuildable = requires(const T pkt, std::vector<uint8_t> &out) {
  { T::type } -> std::convertible_to<PacketType>;
  { pkt.payloadSize() } -> std::same_as<std::size_t>;
  { pkt.serialize(out) } -> std::same_as<void>;
};

// Build a complete frame from a packet
template <FrameBuildable P>
[[nodiscard]] std::vector<uint8_t> buildFrame(const P &packet) {
  const std::size_t payloadSize = packet.payloadSize();
  const std::size_t totalSize = kFrameHeaderSize + payloadSize;

  std::vector<uint8_t> result;
  result.reserve(totalSize);

  // Wire format: [Length: 8 bytes] [Type: 2 bytes] [Payload: N bytes]
  binary::writeBigEndian(result, static_cast<uint64_t>(payloadSize));
  binary::writeBigEndian(result, static_cast<uint16_t>(P::type));
  packet.serialize(result);

  return result;
}

// Maximum reasonable payload size (prevents integer overflow attacks)
inline constexpr uint64_t kMaxPayloadSize = 1ULL << 30; // 1 GB

// Try to parse a frame from a buffer (returns nullopt if incomplete)
// Returns nullopt on incomplete data; call parseFrame for error throwing
// version.
[[nodiscard]] inline std::optional<ParsedFrame>
tryParseFrame(std::span<const uint8_t> buffer) noexcept {
  if (buffer.size() < kFrameHeaderSize) {
    return std::nullopt;
  }

  const uint8_t *ptr = buffer.data();

  // Read length (8 bytes)
  const uint64_t payloadLen = binary::readBigEndian<uint64_t>(ptr);
  ptr += sizeof(uint64_t);

  // Sanity check: reject unreasonably large payloads (prevents overflow)
  if (payloadLen > kMaxPayloadSize) {
    return std::nullopt; // Caller should use parseFrame if they want an
                         // exception
  }

  // Check if we have the complete payload
  if (buffer.size() < kFrameHeaderSize + payloadLen) {
    return std::nullopt;
  }

  // Read type (2 bytes)
  const uint16_t typeVal = binary::readBigEndian<uint16_t>(ptr);
  ptr += sizeof(uint16_t);

  return ParsedFrame{.payload =
                         buffer.subspan(kFrameHeaderSize,
                                        static_cast<std::size_t>(payloadLen)),
                     .type = static_cast<PacketType>(typeVal)};
}

// Parse a frame, throwing if incomplete or malformed
[[nodiscard]] inline ParsedFrame parseFrame(std::span<const uint8_t> buffer) {
  if (buffer.size() < kFrameHeaderSize) {
    throw std::runtime_error("Incomplete Frame Header");
  }

  // Read and validate payload length
  const uint64_t payloadLen = binary::readBigEndian<uint64_t>(buffer.data());
  if (payloadLen > kMaxPayloadSize) {
    throw std::runtime_error("Payload length unreasonable (possible attack)");
  }

  if (auto frame = tryParseFrame(buffer)) {
    return *frame;
  }

  throw std::runtime_error("Incomplete Frame Body");
}

// Overload for vector (backward compatibility)
[[nodiscard]] inline ParsedFrame
parseFrame(const std::vector<uint8_t> &buffer) {
  return parseFrame(std::span<const uint8_t>(buffer));
}

[[nodiscard]] inline std::optional<ParsedFrame>
tryParseFrame(const std::vector<uint8_t> &buffer) noexcept {
  return tryParseFrame(std::span<const uint8_t>(buffer));
}

} // namespace cw::packet
