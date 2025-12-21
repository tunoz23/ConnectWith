#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "../network/connection.h"
#include "../protocol/packet/packet.h"

namespace cw::transfer {

namespace fs = std::filesystem;

// Transfer configuration
inline constexpr std::size_t kDefaultChunkSize = 4096;
inline constexpr std::chrono::milliseconds kBackpressureDelay{1};

// Send a file over a connection.
// localPath: absolute path to the file on disk
// remoteName: path/name as it should appear on the receiver (optional)
inline void sendFile(std::shared_ptr<network::Connection> conn,
                     const std::string &localPath,
                     const std::string &remoteName = "") {

  fs::path path(localPath);
  if (!fs::exists(path)) {
    std::cerr << "[Transfer] File not found: " << localPath << "\n";
    return;
  }

  const uint64_t fileSize = fs::file_size(path);

  // Determine the name to send
  std::string nameToSend =
      remoteName.empty() ? path.filename().generic_string() : remoteName;

  // Normalize path separators for cross-platform compatibility
  std::replace(nameToSend.begin(), nameToSend.end(), '\\', '/');

  std::cout << "[Transfer] Sending " << nameToSend << " (" << fileSize
            << " bytes)...\n";

  // Send handshake first (protocol versioning)
  packet::Handshake handshake;
  conn->send(handshake);

  // Send file header
  packet::FileInfo info;
  info.fileName = nameToSend;
  info.fileSize = fileSize;
  conn->send(info);

  // Stream file in chunks
  std::ifstream file(localPath, std::ios::binary);
  uint64_t offset = 0;

  while (file) {
    // Backpressure: wait if the send queue is too full
    while (conn->isCongested()) {
      std::this_thread::sleep_for(kBackpressureDelay);
    }

    packet::FileChunk chunk;
    chunk.offset = offset;
    chunk.data.resize(kDefaultChunkSize);

    file.read(reinterpret_cast<char *>(chunk.data.data()), kDefaultChunkSize);
    const std::size_t bytesRead = static_cast<std::size_t>(file.gcount());

    if (bytesRead == 0)
      break;

    if (bytesRead < kDefaultChunkSize) {
      chunk.data.resize(bytesRead);
    }

    conn->send(chunk);
    offset += bytesRead;
  }

  // Send completion marker
  packet::FileDone done;
  done.fileSize = fileSize;
  conn->send(done);

  std::cout << "[Transfer] Upload Complete. Sent " << offset << " bytes.\n";
}

} // namespace cw::transfer
