#include <asio.hpp>
#include <iostream>
#include <string>
#include <thread>

#include "cw/network/Connection.h"
#include "cw/network/Client.h"
#include "cw/protocol/packet/packet.h" 

// Ensure this path matches where you saved the file header
#include "cw/file/file.h" 

using namespace cw::network;

int main()
{
	try {
		asio::io_context io_context;
		Client client(io_context);

		// Connect to localhost:8080
		// We capture &client to access the connection object
		client.Connect("127.0.0.1", 8080, [&client]() {

			std::cout << "[Client] Connected! Starting upload...\n";

			auto conn = client.GetConnection();

			if (conn) {
				// DEADLOCK FIX: 
				// We MUST run the file transfer in a background thread.
				// If we ran it here directly, the 'sleep' inside sendFile would 
				// block io_context, preventing the network from actually sending packets!
				std::thread t([conn]() {
					// Make sure "dat/xxx.jpg" exists relative to the executable
					// or provide an absolute path.
					cw::sendFile(conn, "dat/xxx.jpg");
					});

				// Detach allows the thread to run independently while io_context handles the network
				t.detach();
			}
			});

		// The Engine: Pumps data sent by the thread
		io_context.run();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}