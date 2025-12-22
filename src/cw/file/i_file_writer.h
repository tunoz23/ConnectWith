#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace cw::file {

// Error codes for file operations
enum class FileWriterError {
  PathTraversal,   // Path escapes base directory
  CreateDirFailed, // Could not create parent directories
  OpenFailed,      // Could not open file for writing
  NotOpen          // Attempted write without open file
};

// Interface for file writing operations.
// Enables mocking for unit tests without real filesystem I/O.
class IFileWriter {
public:
  virtual ~IFileWriter() = default;

  // Begin receiving a new file
  [[nodiscard]] virtual std::expected<void, FileWriterError>
  beginFile(std::string_view relativePath, uint64_t expectedSize) = 0;

  // Write a chunk at the specified offset
  [[nodiscard]] virtual std::expected<void, FileWriterError>
  writeChunk(uint64_t offset, std::span<const uint8_t> data) = 0;

  // Finish file and validate size. Returns true if integrity check passed.
  [[nodiscard]] virtual bool finishFile(uint64_t finalSize) = 0;

  // Close the file handle
  virtual void close() = 0;

  // Accessors
  [[nodiscard]] virtual uint64_t bytesWritten() const noexcept = 0;
};

} // namespace cw::file
