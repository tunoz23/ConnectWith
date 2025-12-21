#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <span>

#include "../protocol/packet/packet.h"
#include "packet_handler.h"

namespace cw::network {

namespace fs = std::filesystem;

// SPEC E2: Validate that a path does not escape the working directory.
// Returns true if path is safe (within or equal to base).
[[nodiscard]] inline bool
isPathSafe(const fs::path &requestedPath,
           const fs::path &baseDir = fs::current_path()) {
  std::error_code ec;

  // Get the canonical (absolute, resolved) paths
  fs::path canonicalBase = fs::canonical(baseDir, ec);
  if (ec)
    return false;

  // For the requested path, we need to handle the case where it doesn't exist
  // yet. First, get the absolute path, then check if it starts with the base.
  fs::path absoluteRequested = fs::absolute(requestedPath, ec);
  if (ec)
    return false;

  // Normalize to remove . and .. components
  fs::path normalized = absoluteRequested.lexically_normal();

  // Check that the normalized path starts with the base directory
  auto [baseEnd, reqIt] =
      std::mismatch(canonicalBase.begin(), canonicalBase.end(),
                    normalized.begin(), normalized.end());

  // If we consumed all of canonicalBase, the path is within the base
  return baseEnd == canonicalBase.end();
}

// Packet handler that receives file transfers and writes them to disk.
// Thread safety: Single-threaded. Must only be called from io_context thread.
class FileReceiver : public IPacketHandler {
public:
  using SendCallback = std::function<void(const packet::Ack &)>;

  explicit FileReceiver(SendCallback sendAck = nullptr)
      : m_sendAck(std::move(sendAck)) {}

  // SPEC O1: Allow setting callback after construction.
  // This avoids the need to replace the entire handler object.
  void setAckCallback(SendCallback callback) {
    m_sendAck = std::move(callback);
  }

  void onPacket(const packet::ParsedFrame &frame) override {
    using namespace packet;

    switch (frame.type) {
    case PacketType::Handshake:
      handleHandshake(frame.payload);
      break;

    case PacketType::Ack:
      handleAck(frame.payload);
      break;

    case PacketType::FileInfo:
      handleFileInfo(frame.payload);
      break;

    case PacketType::FileChunk:
      handleFileChunk(frame.payload);
      break;

    case PacketType::FileDone:
      handleFileDone(frame.payload);
      break;

    case PacketType::Error:
      handleError(frame.payload);
      break;

    default:
      std::cout << "[Recv] Unknown Packet Type: "
                << static_cast<int>(frame.type) << "\n";
    }
  }

  void onDisconnect() override {
    if (m_outFile.is_open()) {
      m_outFile.close();
      std::cout << "[FileReceiver] Connection closed, file handle released\n";
    }
  }

private:
  void handleHandshake(std::span<const uint8_t> payload) {
    auto pkt = packet::Handshake::deserialize(payload);
    std::cout << "[Recv] Handshake (version: " << pkt.protocolVersion << ")\n";

    if (pkt.protocolVersion != packet::Handshake::kCurrentVersion) {
      std::cerr << "[Warn] Protocol version mismatch. Expected: "
                << packet::Handshake::kCurrentVersion
                << ", Got: " << pkt.protocolVersion << "\n";
    }
  }

  void handleAck(std::span<const uint8_t> payload) {
    auto pkt = packet::Ack::deserialize(payload);
    std::cout << "[Recv] Ack (offset: " << pkt.offset << ")\n";
  }

  void handleFileInfo(std::span<const uint8_t> payload) {
    auto pkt = packet::FileInfo::deserialize(payload);
    std::cout << "[Recv] Starting Download: " << pkt.fileName << " ("
              << pkt.fileSize << " bytes)\n";

    fs::path targetPath(pkt.fileName);

    // SPEC E2: Validate path before opening file.
    // Block path traversal attacks (e.g., "../../../etc/passwd").
    if (!isPathSafe(targetPath)) {
      std::cerr << "[Security] REJECTED: Path traversal attempt blocked: "
                << pkt.fileName << "\n";
      m_rejected = true;
      return;
    }

    // Create parent directories if needed
    if (targetPath.has_parent_path()) {
      std::error_code ec;
      fs::create_directories(targetPath.parent_path(), ec);
      if (ec) {
        std::cerr << "[Error] Failed to create directory: " << ec.message()
                  << "\n";
        m_rejected = true;
        return;
      }
    }

    m_outFile.open(targetPath, std::ios::binary);
    if (!m_outFile.is_open()) {
      std::cerr << "[Error] Could not open file for writing: " << targetPath
                << "\n";
      m_rejected = true;
      return;
    }

    m_rejected = false;
    m_expectedSize = pkt.fileSize;
    m_receivedBytes = 0;
  }

  void handleFileChunk(std::span<const uint8_t> payload) {
    if (m_rejected || !m_outFile.is_open())
      return;

    auto pkt = packet::FileChunk::deserialize(payload);

    m_outFile.seekp(static_cast<std::streamoff>(pkt.offset));
    m_outFile.write(reinterpret_cast<const char *>(pkt.data.data()),
                    static_cast<std::streamsize>(pkt.data.size()));

    m_receivedBytes += pkt.data.size();
  }

  void handleFileDone(std::span<const uint8_t> payload) {
    auto pkt = packet::FileDone::deserialize(payload);

    if (m_outFile.is_open()) {
      m_outFile.close();
      std::cout << "[Recv] File Download Complete.\n";
    }

    if (m_rejected) {
      std::cout << "[Recv] File was rejected (path traversal blocked).\n";
      return;
    }

    if (m_receivedBytes == pkt.fileSize) {
      std::cout << "[Check] Integrity Validated (" << m_receivedBytes
                << " bytes).\n";

      if (m_sendAck) {
        packet::Ack ack;
        ack.offset = m_receivedBytes;
        m_sendAck(ack);
      }
    } else {
      std::cerr << "[Check] CORRUPTION DETECTED! Expected " << pkt.fileSize
                << " but got " << m_receivedBytes << "\n";
    }
  }

  void handleError(std::span<const uint8_t> payload) {
    auto pkt = packet::Error::deserialize(payload);
    std::cerr << "[Recv] Error: " << pkt.message << "\n";
  }

private:
  SendCallback m_sendAck;
  std::ofstream m_outFile;
  uint64_t m_expectedSize = 0;
  uint64_t m_receivedBytes = 0;
  bool m_rejected = false;
};

} // namespace cw::network
