#include <filesystem>
#include <iostream>
#include <string>

#include <asio.hpp>

#include "cw/network/client.h"

namespace fs = std::filesystem;

constexpr uint16_t kDefaultPort = 8080;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <path_to_send> <server_ip>\n";
    return 1;
  }

  const fs::path sourcePath(argv[1]);
  const std::string serverIp = argv[2];

  if (!fs::exists(sourcePath)) {
    std::cerr << "[Client] Path does not exist: " << sourcePath << "\n";
    return 1;
  }

  try {
    asio::io_context ioContext;
    cw::network::Client client(ioContext);

    client.connect(serverIp, kDefaultPort, [&client, &sourcePath]() {
      std::cout << "[Client] Connected! Starting upload...\n";
      client.startTransfer(sourcePath);
    });

    ioContext.run();
  } catch (const std::exception &e) {
    std::cerr << "[Client] Exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}