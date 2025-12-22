#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <span>

#include "../file/i_file_writer.h"
#include "../protocol/packet/packet.h"
#include "packet_handler.h"

namespace cw::network {

// Packet handler that receives file transfers and delegates to IFileWriter.
// Single responsibility: packet dispatch and protocol handling.
// Thread safety: Single-threaded. Must only be called from io_context thread.
class FileReceiver : public IPacketHandler {
public:
  using SendCallback = std::function<void(const packet::Ack &)>;

  // Takes an IFileWriter reference - caller owns the writer and must ensure
  // it outlives this FileReceiver.
  explicit FileReceiver(file::IFileWriter &writer,
                        SendCallback sendAck = nullptr)
      : m_writer(writer), m_sendAck(std::move(sendAck)) {}

  // Allow setting callback after construction (for Session wiring)
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
    m_writer.close();
    std::cout << "[FileReceiver] Connection closed, file handle released\n";
  }

  // Accessors for testing
  [[nodiscard]] bool isRejected() const noexcept { return m_rejected; }

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

    auto result = m_writer.beginFile(pkt.fileName, pkt.fileSize);
    if (!result) {
      m_rejected = true;
      switch (result.error()) {
      case file::FileWriterError::PathTraversal:
        std::cerr << "[Security] REJECTED: Path traversal attempt blocked: "
                  << pkt.fileName << "\n";
        break;
      case file::FileWriterError::CreateDirFailed:
        std::cerr << "[Error] Failed to create directory for: " << pkt.fileName
                  << "\n";
        break;
      case file::FileWriterError::OpenFailed:
        std::cerr << "[Error] Could not open file for writing: " << pkt.fileName
                  << "\n";
        break;
      default:
        std::cerr << "[Error] Unknown error opening file\n";
        break;
      }
      return;
    }

    m_rejected = false;
  }

  void handleFileChunk(std::span<const uint8_t> payload) {
    if (m_rejected)
      return;

    auto pkt = packet::FileChunk::deserialize(payload);
    auto result = m_writer.writeChunk(pkt.offset, pkt.data);

    if (!result) {
      std::cerr << "[Error] Failed to write chunk at offset " << pkt.offset
                << "\n";
    }
  }

  void handleFileDone(std::span<const uint8_t> payload) {
    auto pkt = packet::FileDone::deserialize(payload);

    if (m_rejected) {
      std::cout << "[Recv] File was rejected (path traversal blocked).\n";
      return;
    }

    bool valid = m_writer.finishFile(pkt.fileSize);
    std::cout << "[Recv] File Download Complete.\n";

    if (valid) {
      std::cout << "[Check] Integrity Validated (" << pkt.fileSize
                << " bytes).\n";

      if (m_sendAck) {
        packet::Ack ack;
        ack.offset = pkt.fileSize;
        m_sendAck(ack);
      }
    } else {
      std::cerr << "[Check] CORRUPTION DETECTED! Expected " << pkt.fileSize
                << " but got different byte count\n";
    }
  }

  void handleError(std::span<const uint8_t> payload) {
    auto pkt = packet::Error::deserialize(payload);
    std::cerr << "[Recv] Error: " << pkt.message << "\n";
  }

private:
  file::IFileWriter &m_writer; // External ownership via interface
  SendCallback m_sendAck;
  bool m_rejected = false;
};

} // namespace cw::network
