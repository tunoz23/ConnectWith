#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include <asio.hpp>

#include "../file/file_transfer.h"
#include "connection.h"
#include "file_receiver.h"


namespace cw::network {

namespace fs = std::filesystem;
using asio::ip::tcp;

// TCP Client with integrated file transfer support.
// Uses std::jthread for safe background file transfers.
class Client {
public:
  explicit Client(asio::io_context &context)
      : m_context(context), m_handler(std::make_unique<FileReceiver>()),
        m_connection(Connection::create(context, *m_handler)) {}

  ~Client() {
    // jthread automatically joins/stops on destruction
  }

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

  // Start a file transfer in a background thread.
  // Uses std::jthread for automatic cleanup and cancellation support.
  void startTransfer(const fs::path &sourcePath) {
    if (!fs::exists(sourcePath)) {
      std::cerr << "[Client] Path does not exist: " << sourcePath << "\n";
      return;
    }

    // Stop any existing transfer
    if (m_transferThread.joinable()) {
      m_transferThread.request_stop();
      m_transferThread.join();
    }

    m_transferThread =
        std::jthread([this, sourcePath](std::stop_token stopToken) {
          try {
            transferFiles(sourcePath, stopToken);
          } catch (const std::exception &e) {
            std::cerr << "[Client] Transfer error: " << e.what() << "\n";
          }
        });
  }

  // Send a packet directly (for advanced use cases)
  template <packet::FrameBuildable PacketT> void send(const PacketT &pkt) {
    if (m_connection) {
      m_connection->send(pkt);
    }
  }

  // Access the underlying connection
  [[nodiscard]] std::shared_ptr<Connection> getConnection() noexcept {
    return m_connection;
  }

private:
  void transferFiles(const fs::path &sourcePath, std::stop_token stopToken) {
    if (fs::is_directory(sourcePath)) {
      for (const auto &entry : fs::recursive_directory_iterator(sourcePath)) {
        if (stopToken.stop_requested()) {
          std::cout << "[Client] Transfer cancelled.\n";
          return;
        }

        if (entry.is_regular_file()) {
          std::cout << "Sending: " << entry.path().string() << std::endl;
          std::string relativePath =
              fs::relative(entry.path(), sourcePath).string();
          transfer::sendFile(m_connection, entry.path().string(), relativePath);
        }
      }
    } else {
      std::cout << "Sending: " << sourcePath.string() << std::endl;
      transfer::sendFile(m_connection, sourcePath.string(),
                         sourcePath.filename().string());
    }
  }

private:
  asio::io_context &m_context;
  std::unique_ptr<FileReceiver> m_handler;
  std::shared_ptr<Connection> m_connection;
  std::jthread m_transferThread;
};

} // namespace cw::network
