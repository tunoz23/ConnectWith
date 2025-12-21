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

  // Change working directory so files are written to destination
  try {
    fs::current_path(destPath);
    std::cout << "[Server] Working directory: " << fs::current_path() << "\n";
  } catch (const std::exception &e) {
    std::cerr << "[Server] Failed to change directory: " << e.what() << "\n";
    return 1;
  }

  try {
    asio::io_context ioContext;
    cw::network::Server server(ioContext, kDefaultPort);

    std::cout << "[Server] Listening on port " << kDefaultPort << "...\n";
    ioContext.run();
  } catch (const std::exception &e) {
    std::cerr << "[Server] Exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
