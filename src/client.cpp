#include <asio.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <filesystem>

#include "cw/network/Connection.h"
#include "cw/network/Client.h"
#include "cw/protocol/packet/packet.h" 

// Ensure this path matches where you saved the file header
#include "cw/file/file.h" 

using namespace cw::network;
namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
	// 1. Argument Parsing
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <path_to_send> <server_ip>" << std::endl;
		return 1;
	}

	std::string source_path_str = argv[1];
	std::string server_ip = argv[2];
	fs::path source_path(source_path_str);

	if (!fs::exists(source_path)) {
		std::cerr << "Path does not exist: " << source_path << std::endl;
		return 1;
	}

	try {
		asio::io_context io_context;
		Client client(io_context);

		// Connect to the provided IP on port 8080
		// We capture &client to access the connection object, and source_path for the logic
		client.Connect(server_ip, 8080, [&client, source_path]() {

			std::cout << "[Client] Connected! Starting upload...\n";

			auto conn = client.GetConnection();

			if (conn) {
				// DEADLOCK FIX: 
				// We MUST run the file transfer in a background thread.
				// If we ran it here directly, the 'sleep' inside sendFile would 
				// block io_context, preventing the network from actually sending packets!
				std::thread t([conn, source_path]() {
					try {
						if (fs::is_directory(source_path)) {
							// Recursively find every file to send
							for (const auto& entry : fs::recursive_directory_iterator(source_path)) {
								if (entry.is_regular_file()) {
									std::cout << "Sending: " << entry.path().string() << std::endl;

									// Calculate the relative path to send to the server
									// This allows the server to recreate the directory structure
									// e.g. if source is "data" and file is "data/imgs/1.png", relative is "imgs/1.png"
									std::string relative_path = fs::relative(entry.path(), source_path).string();

									// Pass both the absolute path (for reading) and relative path (for saving on server)
									cw::sendFile(conn, entry.path().string(), relative_path);
								}
							}
						}
						else {
							// Single file case
							std::cout << "Sending: " << source_path.string() << std::endl;
							// For a single file, the relative path is just the filename
							cw::sendFile(conn, source_path.string(), source_path.filename().string());
						}
					}
					catch (const std::exception& e) {
						std::cerr << "File system error inside thread: " << e.what() << "\n";
					}
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