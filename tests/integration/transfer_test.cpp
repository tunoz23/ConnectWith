// Integration tests for full file transfer pipeline
// Uses real FileWriter with temp directory - tests module interactions

#include <gtest/gtest.h>
#include <vector>

#include "cw/file/file_writer.h"
#include "cw/frame.h"
#include "cw/network/file_receiver.h"
#include "cw/protocol/packet/packet.h"
#include "temp_directory.h"

using namespace cw;
using namespace cw::network;
using namespace cw::packet;

// ============================================================================
// Integration Test Fixture
// ============================================================================

class TransferIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    tempDir_ = std::make_unique<test::TempDirectory>();
    writer_ = std::make_unique<file::FileWriter>(tempDir_->path());

    ackReceived_ = false;
    receiver_ =
        std::make_unique<FileReceiver>(*writer_, [this](const Ack &ack) {
          ackReceived_ = true;
          lastAckOffset_ = ack.offset;
        });
  }

  // Simulate receiving a packet
  template <FrameBuildable P> void receivePacket(const P &pkt) {
    frameBuffer_ = buildFrame(pkt);
    receiver_->onPacket(parseFrame(frameBuffer_));
  }

  std::unique_ptr<test::TempDirectory> tempDir_;
  std::unique_ptr<file::FileWriter> writer_;
  std::unique_ptr<FileReceiver> receiver_;
  std::vector<uint8_t> frameBuffer_;
  bool ackReceived_ = false;
  uint64_t lastAckOffset_ = 0;
};

// ============================================================================
// Full Transfer Round-Trip Tests
// ============================================================================

TEST_F(TransferIntegrationTest, SmallFile_TransferComplete) {
  const std::string filename = "small.txt";
  const std::vector<uint8_t> content = {'H', 'e', 'l', 'l', 'o'};

  // 1. Send FileInfo
  FileInfo info;
  info.fileName = filename;
  info.fileSize = content.size();
  receivePacket(info);

  // 2. Send single chunk with all data
  FileChunk chunk;
  chunk.offset = 0;
  chunk.data = content;
  receivePacket(chunk);

  // 3. Send FileDone
  FileDone done;
  done.fileSize = content.size();
  receivePacket(done);

  // Verify
  EXPECT_TRUE(ackReceived_);
  EXPECT_EQ(lastAckOffset_, content.size());
  EXPECT_TRUE(tempDir_->fileExists(filename));
  EXPECT_EQ(tempDir_->readFile(filename), "Hello");
}

TEST_F(TransferIntegrationTest, MultiChunk_TransferComplete) {
  const std::string filename = "chunked.bin";

  // 1. Send FileInfo
  FileInfo info;
  info.fileName = filename;
  info.fileSize = 10;
  receivePacket(info);

  // 2. Send first chunk
  FileChunk chunk1;
  chunk1.offset = 0;
  chunk1.data = {1, 2, 3, 4, 5};
  receivePacket(chunk1);

  // 3. Send second chunk
  FileChunk chunk2;
  chunk2.offset = 5;
  chunk2.data = {6, 7, 8, 9, 10};
  receivePacket(chunk2);

  // 4. Send FileDone
  FileDone done;
  done.fileSize = 10;
  receivePacket(done);

  // Verify
  EXPECT_TRUE(ackReceived_);
  EXPECT_EQ(lastAckOffset_, 10);
  EXPECT_TRUE(tempDir_->fileExists(filename));

  std::string content = tempDir_->readFile(filename);
  ASSERT_EQ(content.size(), 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(static_cast<uint8_t>(content[i]), i + 1);
  }
}

TEST_F(TransferIntegrationTest, NestedPath_CreatesDirectories) {
  const std::string filename = "subdir/nested/file.txt";
  const std::vector<uint8_t> content = {'x'};

  // Transfer
  FileInfo info;
  info.fileName = filename;
  info.fileSize = 1;
  receivePacket(info);

  FileChunk chunk;
  chunk.offset = 0;
  chunk.data = content;
  receivePacket(chunk);

  FileDone done;
  done.fileSize = 1;
  receivePacket(done);

  // Verify directories were created
  EXPECT_TRUE(ackReceived_);
  EXPECT_TRUE(tempDir_->fileExists(filename));
}

TEST_F(TransferIntegrationTest, PathTraversal_Rejected) {
  FileInfo info;
  info.fileName = "../../../etc/passwd";
  info.fileSize = 100;
  receivePacket(info);

  // Chunks should be ignored
  FileChunk chunk;
  chunk.offset = 0;
  chunk.data = {0x00};
  receivePacket(chunk);

  FileDone done;
  done.fileSize = 100;
  receivePacket(done);

  // No ack, file not created
  EXPECT_FALSE(ackReceived_);
  EXPECT_TRUE(receiver_->isRejected());
}

TEST_F(TransferIntegrationTest, SizeMismatch_NoAck) {
  const std::string filename = "mismatch.txt";

  // Transfer with wrong size
  FileInfo info;
  info.fileName = filename;
  info.fileSize = 100; // Says 100 bytes
  receivePacket(info);

  FileChunk chunk;
  chunk.offset = 0;
  chunk.data = {1, 2, 3}; // Only 3 bytes
  receivePacket(chunk);

  FileDone done;
  done.fileSize = 100; // Claims 100 bytes
  receivePacket(done);

  // File exists but ack not sent due to size mismatch
  EXPECT_FALSE(ackReceived_);
}

TEST_F(TransferIntegrationTest, EmptyFile_TransferComplete) {
  const std::string filename = "empty.txt";

  FileInfo info;
  info.fileName = filename;
  info.fileSize = 0;
  receivePacket(info);

  FileDone done;
  done.fileSize = 0;
  receivePacket(done);

  EXPECT_TRUE(ackReceived_);
  EXPECT_EQ(lastAckOffset_, 0);
  EXPECT_TRUE(tempDir_->fileExists(filename));
  EXPECT_EQ(tempDir_->readFile(filename), "");
}

// ============================================================================
// Serialization Round-Trip Integration Tests
// ============================================================================

TEST_F(TransferIntegrationTest, SerializationRoundTrip_AllPacketTypes) {
  // Test that every packet type can be serialized, built into a frame,
  // parsed, and deserialized correctly

  // Handshake
  {
    Handshake original;
    original.protocolVersion = 42;
    original.capabilities = 0xDEADBEEF;

    auto frame = buildFrame(original);
    auto parsed = parseFrame(frame);
    auto reconstructed = Handshake::deserialize(parsed.payload);

    EXPECT_EQ(reconstructed.protocolVersion, original.protocolVersion);
    EXPECT_EQ(reconstructed.capabilities, original.capabilities);
  }

  // Ack
  {
    Ack original;
    original.offset = 0x123456789ABCDEF0;

    auto frame = buildFrame(original);
    auto parsed = parseFrame(frame);
    auto reconstructed = Ack::deserialize(parsed.payload);

    EXPECT_EQ(reconstructed.offset, original.offset);
  }

  // Error
  {
    Error original;
    original.code = 500;
    original.message = "Internal error with special chars: <>&\"'";

    auto frame = buildFrame(original);
    auto parsed = parseFrame(frame);
    auto reconstructed = Error::deserialize(parsed.payload);

    EXPECT_EQ(reconstructed.code, original.code);
    EXPECT_EQ(reconstructed.message, original.message);
  }

  // FileInfo
  {
    FileInfo original;
    original.fileSize = std::numeric_limits<uint64_t>::max();
    original.fileName = "path/to/very_long_filename_with_many_chars.txt";

    auto frame = buildFrame(original);
    auto parsed = parseFrame(frame);
    auto reconstructed = FileInfo::deserialize(parsed.payload);

    EXPECT_EQ(reconstructed.fileSize, original.fileSize);
    EXPECT_EQ(reconstructed.fileName, original.fileName);
  }

  // FileChunk
  {
    FileChunk original;
    original.offset = 1024 * 1024;
    original.data.resize(1000);
    for (size_t i = 0; i < original.data.size(); ++i) {
      original.data[i] = static_cast<uint8_t>(i % 256);
    }

    auto frame = buildFrame(original);
    auto parsed = parseFrame(frame);
    auto reconstructed = FileChunk::deserialize(parsed.payload);

    EXPECT_EQ(reconstructed.offset, original.offset);
    EXPECT_EQ(reconstructed.data, original.data);
  }

  // FileDone
  {
    FileDone original;
    original.fileSize = 1024 * 1024 * 1024; // 1GB

    auto frame = buildFrame(original);
    auto parsed = parseFrame(frame);
    auto reconstructed = FileDone::deserialize(parsed.payload);

    EXPECT_EQ(reconstructed.fileSize, original.fileSize);
  }
}
