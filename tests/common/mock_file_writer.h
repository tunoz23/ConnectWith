#pragma once

#include <cstdint>
#include <expected>
#include <gmock/gmock.h>
#include <span>
#include <string_view>


#include "cw/file/i_file_writer.h"

namespace cw::test {

// Mock implementation of IFileWriter for unit testing.
// Allows testing FileReceiver without real filesystem I/O.
class MockFileWriter : public file::IFileWriter {
public:
  // Note: std::expected return types need parentheses because they contain
  // commas
  MOCK_METHOD((std::expected<void, file::FileWriterError>), beginFile,
              (std::string_view path, uint64_t size), (override));
  MOCK_METHOD((std::expected<void, file::FileWriterError>), writeChunk,
              (uint64_t offset, std::span<const uint8_t> data), (override));
  MOCK_METHOD(bool, finishFile, (uint64_t size), (override));
  MOCK_METHOD(void, close, (), (override));
  MOCK_METHOD(uint64_t, bytesWritten, (), (const, noexcept, override));
};

} // namespace cw::test
