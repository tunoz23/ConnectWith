#pragma once

#include <iostream>
#include <memory>

#include <asio.hpp>

#include "connection.h"
#include "file_receiver.h"

namespace cw::network {

using asio::ip::tcp;

// Holds a connection and its associated handler together for lifetime
// management. INVARIANT: m_handler outlives m_connection (handler is created
// first, destroyed last). The handler callback is set at construction, not in
// start().
class Session : public std::enable_shared_from_this<Session> {
public:
  static std::shared_ptr<Session> create(asio::io_context &io) {
    return std::shared_ptr<Session>(new Session(io));
  }

  [[nodiscard]] tcp::socket &socket() noexcept {
    return m_connection->socket();
  }

  void start() {
    // SPEC O1: Handler is already initialized in constructor with proper
    // callback. Do NOT replace m_handler here — that would create a dangling
    // reference.
    m_connection->start();
  }

private:
  // Private constructor enforces factory usage.
  // Member order matters: m_connection stores reference to m_handler,
  // so m_handler must be declared first (destroyed last).
  explicit Session(asio::io_context &io)
      : m_handler(std::make_unique<FileReceiver>()),
        m_connection(Connection::create(io, *m_handler)) {
    // Set the ack callback AFTER both members exist.
    // This avoids the dangling reference bug from replacing m_handler in
    // start().
    m_handler->setAckCallback(
        [conn = m_connection](const packet::Ack &ack) { conn->send(ack); });
  }

  // Order matters: handler first (referenced by connection), connection second.
  std::unique_ptr<FileReceiver> m_handler;
  std::shared_ptr<Connection> m_connection;
};

// TCP Server that accepts connections and handles file transfers.
class Server {
public:
  Server(asio::io_context &ioContext, uint16_t port)
      : m_ioContext(ioContext),
        m_acceptor(ioContext, tcp::endpoint(tcp::v4(), port)) {
    std::cout << "[Server] Started on port " << port << "\n";
    doAccept();
  }

private:
  void doAccept() {
    auto session = Session::create(m_ioContext);

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
};

} // namespace cw::network
