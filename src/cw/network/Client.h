#pragma once
#include <asio.hpp>
#include <functional>
#include <iostream>
#include <string>
#include "Connection.h"

namespace cw::network {

	using asio::ip::tcp;

	class Client {
	public:
		Client(asio::io_context& context)
			: m_context(context)
		{
		}

		// Connects to a specific IP and Port
		// Note: ip_address must be an IP literal (e.g., "127.0.0.1"), not a hostname.
		void Connect(const std::string& ip_address, unsigned short port, std::function<void()> onConnect = nullptr) {

			m_connection = Connection::create(m_context);

			// 1. Create the endpoint directly from the IP string and port
			asio::ip::tcp::endpoint endpoint(
				asio::ip::make_address(ip_address),
				port
			);

			// 2. Use the socket's MEMBER function (much simpler than resolver iterator)
			m_connection->socket().async_connect(endpoint,
				[this, onConnect](std::error_code ec) {
					if (!ec) {
						std::cout << "[Client] Connected to Server!\n";

						m_connection->start();

						if (onConnect) {
							onConnect();
						}
					}
					else {
						std::cerr << "[Client] Connection failed: " << ec.message() << "\n";
					}
				});
		}

		template <typename PacketType>
		void Send(const PacketType& packet) {
			if (m_connection) {
				m_connection->send(packet);
			}
		}

		std::shared_ptr<Connection> GetConnection() {
			return m_connection;
		}

	private:
		asio::io_context& m_context;
		std::shared_ptr<Connection> m_connection;
	};
}