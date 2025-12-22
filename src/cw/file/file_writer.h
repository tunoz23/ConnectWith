#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>

#include "i_file_writer.h"
#include "path_validator.h"

namespace cw::file {

namespace fs = std::filesystem;

// Responsible for writing file data to disk with security validation.
// Implements IFileWriter interface.
// Single responsibility: disk I/O for file reception.
// Thread safety: NOT thread-safe. Use one instance per transfer.
class FileWriter : public IFileWriter {
public:
  // Backward compatibility type alias
  using Error = FileWriterError;

  // Construct with explicit base directory (no global state dependency)
  explicit FileWriter(fs::path baseDir) : m_baseDir(std::move(baseDir)) {}

  ~FileWriter() override { close(); }

  // Non-copyable, movable
  FileWriter(const FileWriter &) = delete;
  FileWriter &operator=(const FileWriter &) = delete;
  FileWriter(FileWriter &&) = default;
  FileWriter &operator=(FileWriter &&) = default;

  // Begin receiving a new file
  [[nodiscard]] std::expected<void, FileWriterError>
  beginFile(std::string_view relativePath, uint64_t expectedSize) override {
    close(); // Close any previous file

    fs::path targetPath = m_baseDir / relativePath;

    // SECURITY: Validate path before any filesystem operations
    if (!isPathSafe(targetPath, m_baseDir)) {
      return std::unexpected(FileWriterError::PathTraversal);
    }

    // Create parent directories if needed
    if (targetPath.has_parent_path()) {
      std::error_code ec;
      fs::create_directories(targetPath.parent_path(), ec);
      if (ec)
        return std::unexpected(FileWriterError::CreateDirFailed);
    }

    m_file.open(targetPath, std::ios::binary);
    if (!m_file.is_open()) {
      return std::unexpected(FileWriterError::OpenFailed);
    }

    m_expectedSize = expectedSize;
    m_bytesWritten = 0;
    return {};
  }

  // Write a chunk at the specified offset
  [[nodiscard]] std::expected<void, FileWriterError>
  writeChunk(uint64_t offset, std::span<const uint8_t> data) override {
    if (!m_file.is_open()) {
      return std::unexpected(FileWriterError::NotOpen);
    }

    m_file.seekp(static_cast<std::streamoff>(offset));
    m_file.write(reinterpret_cast<const char *>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    m_bytesWritten += data.size();
    return {};
  }

  // Finish file and validate size. Returns true if integrity check passed.
  [[nodiscard]] bool finishFile(uint64_t finalSize) override {
    bool valid = (m_bytesWritten == finalSize);
    close();
    return valid;
  }

  // Close the file handle
  void close() override {
    if (m_file.is_open()) {
      m_file.close();
    }
  }

  // Accessors
  [[nodiscard]] uint64_t bytesWritten() const noexcept override {
    return m_bytesWritten;
  }
  [[nodiscard]] const fs::path &baseDir() const noexcept { return m_baseDir; }

private:
  fs::path m_baseDir;
  std::ofstream m_file;
  uint64_t m_expectedSize = 0;
  uint64_t m_bytesWritten = 0;
};

} // namespace cw::file
