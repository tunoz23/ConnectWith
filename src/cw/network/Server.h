#pragma once

#include <filesystem>
#include <iostream>
#include <memory>

#include <asio.hpp>

#include "../file/file_writer.h"
#include "connection.h"
#include "file_receiver.h"

namespace cw::network {

namespace fs = std::filesystem;
using asio::ip::tcp;

// Holds a connection and its associated handler together for lifetime
// management. INVARIANT: m_writer outlives m_handler outlives m_connection.
// Members are destroyed in reverse declaration order.
class Session : public std::enable_shared_from_this<Session> {
public:
  // Factory: creates session with file writer targeting baseDir
  static std::shared_ptr<Session> create(asio::io_context &io,
                                         const fs::path &baseDir) {
    return std::shared_ptr<Session>(new Session(io, baseDir));
  }

  [[nodiscard]] tcp::socket &socket() noexcept {
    return m_connection->socket();
  }

  void start() {
    // Handler is already initialized in constructor with proper callback.
    m_connection->start();
  }

private:
  // Private constructor enforces factory usage.
  // Member order matters for destruction: writer last, connection first.
  explicit Session(asio::io_context &io, const fs::path &baseDir)
      : m_writer(std::make_unique<file::FileWriter>(baseDir)),
        m_handler(std::make_unique<FileReceiver>(*m_writer)),
        m_connection(Connection::create(io, *m_handler)) {
    // Set the ack callback AFTER all members exist.
    m_handler->setAckCallback(
        [conn = m_connection](const packet::Ack &ack) { conn->send(ack); });
  }

  // Order matters: writer first (used by handler), handler second (used by
  // connection), connection last
  std::unique_ptr<file::FileWriter> m_writer;
  std::unique_ptr<FileReceiver> m_handler;
  std::shared_ptr<Connection> m_connection;
};

// TCP Server that accepts connections and handles file transfers.
// Now takes explicit destination directory (no global current_path dependency).
class Server {
public:
  Server(asio::io_context &ioContext, uint16_t port, fs::path destDir)
      : m_ioContext(ioContext),
        m_acceptor(ioContext, tcp::endpoint(tcp::v4(), port)),
        m_destDir(std::move(destDir)) {
    std::cout << "[Server] Started on port " << port << "\n";
    std::cout << "[Server] Saving files to: " << m_destDir << "\n";
    doAccept();
  }

private:
  void doAccept() {
    auto session = Session::create(m_ioContext, m_destDir);

    m_acceptor.async_accept(
        session->socket(), [this, session](std::error_code ec) {
          if (!ec) {
            std::cout << "[Server] Client Connected: "
                      << session->socket().remote_endpoint() << "\n";
            session->start();
          } else {
            std::cerr << "[Server] Accept Error: " << ec.message() << "\n";
          }

          if (m_acceptor.is_open()) {
            doAccept();
          }
        });
  }

private:
  asio::io_context &m_ioContext;
  tcp::acceptor m_acceptor;
  fs::path m_destDir;
};

} // namespace cw::network
