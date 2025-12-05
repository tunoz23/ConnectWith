#include <iostream>
#include <vector>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <asio.hpp>




#include "cw/endian.h"
#include "packet/packet.h"
#include "cw/Frame.h"
#include "cw/network/Connection.h"

using asio::ip::tcp;

int main()
{
	try 
	{
		asio::io_context io_context;

		tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));

		std::cout << "[Server] Listening on 8080...\n";

		std::function<void()> do_accept = [&]()
		{

			auto new_conn = cw::network::Connection::create(io_context);

			acceptor.async_accept(new_conn->socket(),
				[&, new_conn](std::error_code ec)
				{
					if (!ec) {
						std::cout << "[Server] New Client Connected!\n";

						new_conn->start();
					}

					do_accept();
				});
			};

		do_accept(); 

		io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
}