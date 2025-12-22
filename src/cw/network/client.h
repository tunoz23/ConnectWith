#pragma once

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <asio.hpp>

#include "../file/file_writer.h"
#include "../file/transfer_orchestrator.h"
#include "connection.h"
#include "file_receiver.h"

namespace cw::network {

namespace fs = std::filesystem;
using asio::ip::tcp;

// TCP Client with integrated file transfer support.
// Simplified: delegates transfer orchestration to TransferOrchestrator.
class Client {
public:
  explicit Client(asio::io_context &context)
      : m_context(context),
        // Client uses temp directory for handler (only receives Acks, not
        // files)
        m_dummyWriter(
            std::make_unique<file::FileWriter>(fs::temp_directory_path())),
        m_handler(std::make_unique<FileReceiver>(*m_dummyWriter)),
        m_connection(Connection::create(context, *m_handler)),
        m_orchestrator(
            std::make_unique<transfer::TransferOrchestrator>(*m_connection)) {}

  ~Client() = default;

  // Non-copyable
  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  // Connect to a server by IP address and port.
  // onConnect callback is invoked after successful connection.
  void connect(const std::string &ipAddress, uint16_t port,
               std::function<void()> onConnect = nullptr) {

    tcp::endpoint endpoint(asio::ip::make_address(ipAddress), port);

    m_connection->socket().async_connect(
        endpoint, [this, onConnect = std::move(onConnect)](std::error_code ec) {
          if (!ec) {
            std::cout << "[Client] Connected to Server!\n";
            m_connection->start();

            if (onConnect) {
              onConnect();
            }
          } else {
            std::cerr << "[Client] Connection failed: " << ec.message() << "\n";
          }
        });
  }

  // Start a file transfer using the orchestrator.
  void startTransfer(const fs::path &sourcePath) {
    m_orchestrator->startTransfer(sourcePath);
  }

  // Send a packet directly (for advanced use cases)
  template <packet::FrameBuildable PacketT> void send(const PacketT &pkt) {
    if (m_connection) {
      m_connection->sendPacket(pkt);
    }
  }

  // Access the underlying connection
  [[nodiscard]] std::shared_ptr<Connection> getConnection() noexcept {
    return m_connection;
  }

private:
  asio::io_context &m_context;
  // Client side needs a FileWriter only because FileReceiver requires it,
  // but client typically doesn't receive files (only Acks)
  std::unique_ptr<file::FileWriter> m_dummyWriter;
  std::unique_ptr<FileReceiver> m_handler;
  std::shared_ptr<Connection> m_connection;
  std::unique_ptr<transfer::TransferOrchestrator> m_orchestrator;
};

} // namespace cw::network
