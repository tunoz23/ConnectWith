#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

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

    // Work guard keeps io_context.run() alive
    auto workGuard = asio::make_work_guard(ioContext);

    cw::network::Client client(ioContext);

    // Track when transfer is done
    std::atomic<bool> transferStarted{false};

    client.connect(serverIp, kDefaultPort,
                   [&client, &sourcePath, &transferStarted]() {
                     std::cout << "[Client] Connected! Starting upload...\n";
                     client.startTransfer(sourcePath);
                     transferStarted.store(true);
                   });

    // Run io_context in a separate thread so we can poll for transfer
    // completion
    std::thread ioThread([&ioContext]() { ioContext.run(); });

    // Wait for transfer to start
    while (!transferStarted.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Poll for transfer completion (orchestrator.isTransferring())
    while (client.isTransferring()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Transfer done - release work guard and stop io_context
    workGuard.reset();
    ioContext.stop();
    ioThread.join();

    std::cout << "[Client] Transfer complete.\n";
  } catch (const std::exception &e) {
    std::cerr << "[Client] Exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}