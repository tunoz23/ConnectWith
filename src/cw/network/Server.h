#pragma once
#include <iostream>
#include <asio.hpp>
#include "cw/network/Connection.h" // Your existing Connection class

namespace cw::network {

	class Server {
	public:
		// Constructor: Starts listening immediately
		Server(asio::io_context& io_context, uint16_t port)
			: m_ioContext(io_context),
			m_acceptor(io_context, tcp::endpoint(tcp::v4(), port))
		{
			std::cout << "[Server] Started on port " << port << "\n";
			doAccept();
		}

	private:
		void doAccept() {
			// 1. Create the Connection wrapper (Eager allocation)
			// We create this *before* the connection is finalized so we have a socket to give to the acceptor.
			auto new_conn = Connection::create(m_ioContext);

			// 2. Async Accept
			m_acceptor.async_accept(
				new_conn->socket(), // We use the socket inside the connection
				[this, new_conn](std::error_code ec) {

					if (!ec) {
						// Success: Start the connection's read loop
						std::cout << "[Server] Client Connected: "
							<< new_conn->socket().remote_endpoint() << "\n";

						new_conn->start();
					}
					else {
						std::cerr << "[Server] Accept Error: " << ec.message() << "\n";
					}

					// 3. Loop: Only continue if the acceptor is still open
					if (m_acceptor.is_open()) {
						doAccept();
					}
				});
		}

	private:
		asio::io_context& m_ioContext;
		asio::ip::tcp::acceptor m_acceptor;
	};
}