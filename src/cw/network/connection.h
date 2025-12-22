#pragma once

#include <array>
#include <atomic>
#include <deque>
#include <iostream>
#include <memory>
#include <vector>

#include <asio.hpp>

#include "../frame.h"
#include "i_sender.h"
#include "packet_handler.h"

namespace cw::network {

// Connection configuration constants
inline constexpr std::size_t kReadBufferSize = 8192;
inline constexpr std::size_t kCongestionThreshold = 1024 * 1024; // 1 MB

using asio::ip::tcp;

// Manages an async TCP connection with frame-based messaging.
// Business logic is delegated to an IPacketHandler.
// Implements ISender for decoupled packet sending.
class Connection : public std::enable_shared_from_this<Connection>,
                   public ISender {
public:
  // Factory method - use this instead of constructor
  static std::shared_ptr<Connection> create(asio::io_context &io,
                                            IPacketHandler &handler) {
    return std::shared_ptr<Connection>(new Connection(io, handler));
  }

  // Access the underlying socket (for accept/connect operations)
  [[nodiscard]] tcp::socket &socket() noexcept { return m_socket; }

  // Check if send queue is congested (for backpressure)
  [[nodiscard]] bool isCongested() const noexcept override {
    return m_queueSize.load(std::memory_order_relaxed) > kCongestionThreshold;
  }

  // ISender implementation - send specific packet types
  void send(const packet::Handshake &pkt) override { sendPacket(pkt); }
  void send(const packet::FileInfo &pkt) override { sendPacket(pkt); }
  void send(const packet::FileChunk &pkt) override { sendPacket(pkt); }
  void send(const packet::FileDone &pkt) override { sendPacket(pkt); }
  void send(const packet::Ack &pkt) override { sendPacket(pkt); }
  void send(const packet::Error &pkt) override { sendPacket(pkt); }

  // Generic send for any FrameBuildable packet (backward compatibility)
  template <packet::FrameBuildable PacketT>
  void sendPacket(const PacketT &pkt) {
    auto frame = packet::buildFrame(pkt);
    auto self = shared_from_this();

    asio::post(m_socket.get_executor(),
               [this, self, msg = std::move(frame)]() mutable {
                 doWrite(std::move(msg));
               });
  }

  // Start the read loop (call after connection is established)
  void start() {
    std::cout << "[Connection] Ready.\n";
    doRead();
  }

private:
  Connection(asio::io_context &io, IPacketHandler &handler)
      : m_socket(io), m_handler(handler) {
    m_readBuffer.fill(0);
    m_incomingBuffer.reserve(kReadBufferSize);
  }

  void doRead() {
    auto self = shared_from_this();

    m_socket.async_read_some(
        asio::buffer(m_readBuffer),
        [this, self](std::error_code ec, std::size_t bytesRead) {
          if (!ec) {
            m_incomingBuffer.insert(m_incomingBuffer.end(),
                                    m_readBuffer.begin(),
                                    m_readBuffer.begin() + bytesRead);

            processBuffer();
            doRead();
          } else {
            std::cout << "[Connection] Disconnected: " << ec.message() << "\n";
            m_handler.onDisconnect();
          }
        });
  }

  void processBuffer() {
    while (auto frame = packet::tryParseFrame(m_incomingBuffer)) {
      // SPEC E1: Catch exceptions from handler to prevent std::terminate.
      // Exceptions in ASIO callbacks are fatal by default.
      try {
        m_handler.onPacket(*frame);
      } catch (const std::exception &e) {
        std::cerr << "[Connection] Packet handling error: " << e.what() << "\n";
        // Continue processing remaining frames
      }

      // Consume the processed frame
      const std::size_t consumeSize = frame->totalSize();
      m_incomingBuffer.erase(m_incomingBuffer.begin(),
                             m_incomingBuffer.begin() + consumeSize);
    }
  }

  void doWrite(std::vector<uint8_t> frame) {
    m_queueSize.fetch_add(frame.size(), std::memory_order_relaxed);
    m_writeQueue.push_back(std::move(frame));

    if (m_writeQueue.size() > 1)
      return;
    writeQueueFront();
  }

  void writeQueueFront() {
    auto self = shared_from_this();

    asio::async_write(
        m_socket, asio::buffer(m_writeQueue.front()),
        [this, self](std::error_code ec, std::size_t /*bytesWritten*/) {
          if (!ec) {
            m_queueSize.fetch_sub(m_writeQueue.front().size(),
                                  std::memory_order_relaxed);
            m_writeQueue.pop_front();

            if (!m_writeQueue.empty()) {
              writeQueueFront();
            }
          } else {
            std::cerr << "[Connection] Write Error: " << ec.message() << "\n";
            m_socket.close();
          }
        });
  }

private:
  tcp::socket m_socket;
  IPacketHandler &m_handler;

  std::array<uint8_t, kReadBufferSize> m_readBuffer{};
  std::vector<uint8_t> m_incomingBuffer;
  std::deque<std::vector<uint8_t>> m_writeQueue;
  std::atomic<std::size_t> m_queueSize{0};
};

} // namespace cw::network