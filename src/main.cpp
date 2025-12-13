#include <iostream>
#include <vector>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <asio.hpp>
#include <filesystem>

#include "cw/endian.h"
#include "packet/packet.h"
#include "cw/Frame.h"
#include "cw/network/Connection.h"
#include "cw/network/Server.h"
#include "cw/file/file.h" 

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
	// 1. Argument Parsing
	// The server takes an argument determining where to put all incoming data.
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <destination_folder>" << std::endl;
		return 1;
	}

	std::string destination_folder = argv[1];
	fs::path dest_path(destination_folder);

	// 2. Directory Setup
	// Ensure the base destination folder exists
	if (!fs::exists(dest_path)) {
		try {
			fs::create_directories(dest_path);
			std::cout << "[Server] Created base directory: " << fs::absolute(dest_path) << std::endl;
		}
		catch (const std::exception& e) {
			std::cerr << "Error creating directory: " << e.what() << std::endl;
			return 1;
		}
	}
	else {
		std::cout << "[Server] Saving to existing directory: " << fs::absolute(dest_path) << std::endl;
	}

	// 3. Set Working Directory (CRITICAL FIX)
	// We change the process's current working directory to the destination folder.
	// This forces the library (cw::*) to write files inside this folder, 
	// rather than next to the executable.
	try {
		fs::current_path(dest_path);
		std::cout << "[Server] Working directory changed to: " << fs::current_path() << std::endl;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to change working directory: " << e.what() << std::endl;
		return 1;
	}

	try {
		asio::io_context io_context;

		// The Server object sets up the accept loop in its constructor
		cw::network::Server server(io_context, 8080);

		std::cout << "[Server] Listening on port 8080...\n";

		// Run the blocking loop
		io_context.run();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}