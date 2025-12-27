#pragma once

#include "../network/i_sender.h"
#include "file_transfer.h"
#include <atomic>
#include <filesystem>
#include <iostream>
#include <thread>


namespace cw::transfer {

namespace fs = std::filesystem;

// Orchestrates file transfers by iterating directories and sending files.
// Single responsibility: directory traversal + transfer coordination.
// Uses std::thread with atomic flag for cancellation support.
class TransferOrchestrator {
public:
  explicit TransferOrchestrator(network::ISender &sender)
      : m_sender(sender), m_stopRequested(false) {}

  ~TransferOrchestrator() {
    requestStop();
    if (m_transferThread.joinable()) {
      m_transferThread.join();
    }
  }

  // Non-copyable, non-movable (due to thread member)
  TransferOrchestrator(const TransferOrchestrator &) = delete;
  TransferOrchestrator &operator=(const TransferOrchestrator &) = delete;
  TransferOrchestrator(TransferOrchestrator &&) = delete;
  TransferOrchestrator &operator=(TransferOrchestrator &&) = delete;

  // Start a file transfer in a background thread.
  // If sourcePath is a directory, recursively transfers all files.
  void startTransfer(const fs::path &sourcePath) {
    if (!fs::exists(sourcePath)) {
      std::cerr << "[Orchestrator] Path does not exist: " << sourcePath << "\n";
      return;
    }

    // Stop any existing transfer
    if (m_transferThread.joinable()) {
      requestStop();
      m_transferThread.join();
    }

    // Reset stop flag for new transfer
    m_stopRequested.store(false, std::memory_order_relaxed);

    m_transferThread = std::thread([this, sourcePath]() {
      try {
        transferFiles(sourcePath);
      } catch (const std::exception &e) {
        std::cerr << "[Orchestrator] Transfer error: " << e.what() << "\n";
      }
    });
  }

  // Request stop of current transfer
  void requestStop() { m_stopRequested.store(true, std::memory_order_relaxed); }

  // Check if a transfer is in progress
  [[nodiscard]] bool isTransferring() const noexcept {
    return m_transferThread.joinable();
  }

private:
  void transferFiles(const fs::path &sourcePath) {
    if (fs::is_directory(sourcePath)) {
      for (const auto &entry : fs::recursive_directory_iterator(sourcePath)) {
        if (m_stopRequested.load(std::memory_order_relaxed)) {
          std::cout << "[Orchestrator] Transfer cancelled.\n";
          return;
        }

        if (entry.is_regular_file()) {
          std::cout << "Sending: " << entry.path().string() << std::endl;
          std::string relativePath =
              fs::relative(entry.path(), sourcePath).string();
          sendFile(m_sender, entry.path().string(), relativePath);
        }
      }
    } else {
      std::cout << "Sending: " << sourcePath.string() << std::endl;
      sendFile(m_sender, sourcePath.string(), sourcePath.filename().string());
    }
  }

private:
  network::ISender &m_sender;
  std::thread m_transferThread;
  std::atomic<bool> m_stopRequested;
};

} // namespace cw::transfer
