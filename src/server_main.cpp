#include <filesystem>
#include <iostream>

#include <asio.hpp>

#include "cw/network/server.h"

namespace fs = std::filesystem;

constexpr uint16_t kDefaultPort = 8080;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <destination_folder>\n";
    return 1;
  }

  const fs::path destPath(argv[1]);

  // Ensure destination directory exists
  if (!fs::exists(destPath)) {
    try {
      fs::create_directories(destPath);
      std::cout << "[Server] Created directory: " << fs::absolute(destPath)
                << "\n";
    } catch (const std::exception &e) {
      std::cerr << "[Server] Error creating directory: " << e.what() << "\n";
      return 1;
    }
  } else {
    std::cout << "[Server] Saving to: " << fs::absolute(destPath) << "\n";
  }

  // Get absolute path for the server (no global current_path mutation)
  fs::path absoluteDest = fs::absolute(destPath);

  try {
    asio::io_context ioContext;
    // Pass destination directory explicitly to Server
    cw::network::Server server(ioContext, kDefaultPort, absoluteDest);

    std::cout << "[Server] Listening on port " << kDefaultPort << "...\n";
    ioContext.run();
  } catch (const std::exception &e) {
    std::cerr << "[Server] Exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
