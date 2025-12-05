#include <asio.hpp>
#include <iostream>
#include "cw/network/Connection.h"
#include "cw/protocol/packet/packet.h" // For packet definitions

using asio::ip::tcp;

int main()
{
	try {
		asio::io_context io_context;

		auto conn = cw::network::Connection::create(io_context);

		tcp::resolver resolver(io_context);
		auto endpoints = resolver.resolve("127.0.0.1", "8080");

		asio::async_connect(conn->socket(), endpoints,
			[conn](std::error_code ec, tcp::endpoint)
			{
				if (!ec)
				{
					std::cout << "[Client] Connected to Server!\n";

					conn->start();

					cw::packet::FileInfo pkt;
					pkt.fileName = "hello.txt";
					pkt.fileSize = 1234;

					conn->send(pkt);
				}
			});

		io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

}