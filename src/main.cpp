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
#include "cw/network/Server.h"

int main()
{
	try {
		asio::io_context io_context;

		// The Server object sets up the accept loop in its constructor
		cw::network::Server server(io_context, 8080);

		// Run the blocking loop
		io_context.run();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}
}