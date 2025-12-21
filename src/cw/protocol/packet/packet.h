#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../binary.h"
#include "packet_type.h"


namespace cw::packet {

// Protocol limits
inline constexpr std::size_t kMaxStringLength = 4096;
inline constexpr std::size_t kMaxChunkSize = 10 * 1024 * 1024;

// ============================================================================
// Handshake - Protocol version negotiation (must be first packet)
// ============================================================================

struct Handshake {
  static constexpr PacketType type = PacketType::Handshake;
  static constexpr uint16_t kCurrentVersion = 1;

  uint16_t protocolVersion = kCurrentVersion;
  uint32_t capabilities = 0; // Reserved for future feature flags

  [[nodiscard]] std::size_t payloadSize() const noexcept {
    return sizeof(protocolVersion) + sizeof(capabilities);
  }

  void serialize(std::vector<uint8_t> &out) const {
    binary::writeBigEndian(out, protocolVersion);
    binary::writeBigEndian(out, capabilities);
  }

  [[nodiscard]] static Handshake deserialize(std::span<const uint8_t> payload) {
    constexpr std::size_t kMinSize = sizeof(uint16_t) + sizeof(uint32_t);
    if (payload.size() < kMinSize) {
      throw std::runtime_error("Handshake: payload too small");
    }

    Handshake pkt;
    std::size_t cursor = 0;

    pkt.protocolVersion =
        binary::readBigEndian<uint16_t>(payload.data() + cursor);
    cursor += sizeof(pkt.protocolVersion);

    pkt.capabilities = binary::readBigEndian<uint32_t>(payload.data() + cursor);

    return pkt;
  }
};

// ============================================================================
// Ack - Acknowledgment packet
// ============================================================================

struct Ack {
  static constexpr PacketType type = PacketType::Ack;

  uint64_t offset = 0;

  [[nodiscard]] std::size_t payloadSize() const noexcept {
    return sizeof(offset);
  }

  void serialize(std::vector<uint8_t> &out) const {
    binary::writeBigEndian(out, offset);
  }

  [[nodiscard]] static Ack deserialize(std::span<const uint8_t> payload) {
    if (payload.size() < sizeof(uint64_t)) {
      throw std::runtime_error("Ack: payload too small");
    }

    Ack pkt;
    pkt.offset = binary::readBigEndian<uint64_t>(payload.data());
    return pkt;
  }
};

// ============================================================================
// Error - Error notification
// ============================================================================

struct Error {
  static constexpr PacketType type = PacketType::Error;

  uint16_t code = 0;
  std::string message;

  [[nodiscard]] std::size_t payloadSize() const noexcept {
    return sizeof(code) + sizeof(uint32_t) + message.size();
  }

  void serialize(std::vector<uint8_t> &out) const {
    if (message.size() > kMaxStringLength) {
      throw std::length_error("Error: message exceeds protocol limit");
    }

    binary::writeBigEndian(out, code);
    binary::writeBigEndian(out, static_cast<uint32_t>(message.size()));
    out.insert(out.end(), message.begin(), message.end());
  }

  [[nodiscard]] static Error deserialize(std::span<const uint8_t> payload) {
    constexpr std::size_t kMinSize = sizeof(uint16_t) + sizeof(uint32_t);
    if (payload.size() < kMinSize) {
      throw std::runtime_error("Error: payload too small");
    }

    Error pkt;
    std::size_t cursor = 0;

    pkt.code = binary::readBigEndian<uint16_t>(payload.data() + cursor);
    cursor += sizeof(pkt.code);

    const uint32_t msgLen =
        binary::readBigEndian<uint32_t>(payload.data() + cursor);
    cursor += sizeof(msgLen);

    if (msgLen > kMaxStringLength) {
      throw std::runtime_error("Error: message length unreasonable");
    }
    if (payload.size() - cursor < msgLen) {
      throw std::runtime_error("Error: declared length exceeds buffer");
    }

    if (msgLen > 0) {
      pkt.message.assign(
          reinterpret_cast<const char *>(payload.data() + cursor), msgLen);
    }

    return pkt;
  }
};

// ============================================================================
// FileInfo - File transfer header
// ============================================================================

struct FileInfo {
  static constexpr PacketType type = PacketType::FileInfo;

  uint64_t fileSize = 0;
  std::string fileName;

  [[nodiscard]] std::size_t payloadSize() const noexcept {
    return sizeof(fileSize) + sizeof(uint32_t) + fileName.size();
  }

  void serialize(std::vector<uint8_t> &out) const {
    if (fileName.empty()) {
      throw std::length_error("FileInfo: filename empty");
    }
    if (fileName.size() > kMaxStringLength) {
      throw std::length_error("FileInfo: filename too long");
    }

    binary::writeBigEndian(out, fileSize);
    binary::writeBigEndian(out, static_cast<uint32_t>(fileName.size()));
    out.insert(out.end(), fileName.begin(), fileName.end());
  }

  [[nodiscard]] static FileInfo deserialize(std::span<const uint8_t> payload) {
    constexpr std::size_t kMinSize = sizeof(uint64_t) + sizeof(uint32_t);
    if (payload.size() < kMinSize) {
      throw std::runtime_error("FileInfo: payload too small");
    }

    FileInfo pkt;
    std::size_t cursor = 0;

    pkt.fileSize = binary::readBigEndian<uint64_t>(payload.data() + cursor);
    cursor += sizeof(pkt.fileSize);

    const uint32_t nameLen =
        binary::readBigEndian<uint32_t>(payload.data() + cursor);
    cursor += sizeof(nameLen);

    if (nameLen > kMaxStringLength) {
      throw std::runtime_error("FileInfo: filename too long");
    }
    if (payload.size() - cursor < nameLen) {
      throw std::runtime_error("FileInfo: corrupted name length");
    }

    if (nameLen > 0) {
      pkt.fileName.assign(
          reinterpret_cast<const char *>(payload.data() + cursor), nameLen);
    }

    return pkt;
  }
};

// ============================================================================
// FileChunk - File data chunk
// ============================================================================

struct FileChunk {
  static constexpr PacketType type = PacketType::FileChunk;

  uint64_t offset = 0;
  std::vector<uint8_t> data;

  [[nodiscard]] std::size_t payloadSize() const noexcept {
    return sizeof(offset) + sizeof(uint32_t) + data.size();
  }

  void serialize(std::vector<uint8_t> &out) const {
    if (data.size() > kMaxChunkSize) {
      throw std::length_error("FileChunk: data exceeds protocol limit");
    }
    if (data.size() > std::numeric_limits<uint32_t>::max()) {
      throw std::length_error("FileChunk: data too large for u32 field");
    }

    binary::writeBigEndian(out, offset);
    binary::writeBigEndian(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
  }

  [[nodiscard]] static FileChunk deserialize(std::span<const uint8_t> payload) {
    constexpr std::size_t kHeaderSize = sizeof(uint64_t) + sizeof(uint32_t);
    if (payload.size() < kHeaderSize) {
      throw std::runtime_error("FileChunk: payload too small");
    }

    FileChunk pkt;
    std::size_t cursor = 0;

    pkt.offset = binary::readBigEndian<uint64_t>(payload.data() + cursor);
    cursor += sizeof(pkt.offset);

    const uint32_t dataLen =
        binary::readBigEndian<uint32_t>(payload.data() + cursor);
    cursor += sizeof(dataLen);

    if (dataLen > kMaxChunkSize) {
      throw std::runtime_error("FileChunk: size unreasonable");
    }
    if (payload.size() - cursor < dataLen) {
      throw std::runtime_error("FileChunk: corrupted length");
    }

    pkt.data.assign(payload.data() + cursor, payload.data() + cursor + dataLen);

    return pkt;
  }
};

// ============================================================================
// FileDone - File transfer completion marker
// ============================================================================

struct FileDone {
  static constexpr PacketType type = PacketType::FileDone;

  uint64_t fileSize = 0;

  [[nodiscard]] std::size_t payloadSize() const noexcept {
    return sizeof(fileSize);
  }

  void serialize(std::vector<uint8_t> &out) const {
    binary::writeBigEndian(out, fileSize);
  }

  [[nodiscard]] static FileDone deserialize(std::span<const uint8_t> payload) {
    if (payload.size() < sizeof(uint64_t)) {
      throw std::runtime_error("FileDone: payload too small");
    }

    FileDone pkt;
    pkt.fileSize = binary::readBigEndian<uint64_t>(payload.data());
    return pkt;
  }
};

} // namespace cw::packet