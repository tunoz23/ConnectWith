// Unit tests for FileReceiver using MockFileWriter
// Tests packet dispatch logic without real filesystem I/O

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cw/frame.h"
#include "cw/network/file_receiver.h"
#include "cw/protocol/packet/packet.h"
#include "mock_file_writer.h"

using namespace cw;
using namespace cw::network;
using namespace cw::packet;
using namespace cw::test;
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

// ============================================================================
// Test Fixture
// ============================================================================

class FileReceiverTest : public ::testing::Test {
protected:
  void SetUp() override {
    mockWriter_ = std::make_unique<StrictMock<test::MockFileWriter>>();

    // Create receiver with ack callback
    receiver_ =
        std::make_unique<FileReceiver>(*mockWriter_, [this](const Ack &ack) {
          ackReceived_ = true;
          lastAck_ = ack;
        });
  }

  // Helper: Build a ParsedFrame from a packet
  template <FrameBuildable P> ParsedFrame makeFrame(const P &pkt) {
    frameBuffer_ = buildFrame(pkt);
    return parseFrame(frameBuffer_);
  }

  std::unique_ptr<StrictMock<test::MockFileWriter>> mockWriter_;
  std::unique_ptr<FileReceiver> receiver_;
  std::vector<uint8_t> frameBuffer_;
  bool ackReceived_ = false;
  Ack lastAck_;
};

// ============================================================================
// onPacket(FileInfo) Tests
// ============================================================================

TEST_F(FileReceiverTest, OnFileInfo_CallsBeginFile) {
  FileInfo info;
  info.fileName = "test.txt";
  info.fileSize = 100;

  EXPECT_CALL(*mockWriter_, beginFile(std::string_view("test.txt"), 100))
      .WillOnce(Return(std::expected<void, file::FileWriterError>{}));

  receiver_->onPacket(makeFrame(info));

  EXPECT_FALSE(receiver_->isRejected());
}

TEST_F(FileReceiverTest, OnFileInfo_PathTraversal_SetsRejected) {
  FileInfo info;
  info.fileName = "../../../etc/passwd";
  info.fileSize = 100;

  EXPECT_CALL(*mockWriter_, beginFile(_, _))
      .WillOnce(Return(std::unexpected(file::FileWriterError::PathTraversal)));

  receiver_->onPacket(makeFrame(info));

  EXPECT_TRUE(receiver_->isRejected());
}

TEST_F(FileReceiverTest, OnFileInfo_OpenFailed_SetsRejected) {
  FileInfo info;
  info.fileName = "readonly.txt";
  info.fileSize = 100;

  EXPECT_CALL(*mockWriter_, beginFile(_, _))
      .WillOnce(Return(std::unexpected(file::FileWriterError::OpenFailed)));

  receiver_->onPacket(makeFrame(info));

  EXPECT_TRUE(receiver_->isRejected());
}

// ============================================================================
// onPacket(FileChunk) Tests
// ============================================================================

TEST_F(FileReceiverTest, OnFileChunk_CallsWriteChunk) {
  // First send FileInfo to open file
  FileInfo info;
  info.fileName = "test.txt";
  info.fileSize = 5;

  EXPECT_CALL(*mockWriter_, beginFile(_, _))
      .WillOnce(Return(std::expected<void, file::FileWriterError>{}));

  receiver_->onPacket(makeFrame(info));

  // Then send chunk
  FileChunk chunk;
  chunk.offset = 0;
  chunk.data = {0x01, 0x02, 0x03, 0x04, 0x05};

  EXPECT_CALL(*mockWriter_, writeChunk(0, _))
      .WillOnce(Return(std::expected<void, file::FileWriterError>{}));

  receiver_->onPacket(makeFrame(chunk));
}

TEST_F(FileReceiverTest, OnFileChunk_Skipped_WhenRejected) {
  // Send FileInfo that gets rejected
  FileInfo info;
  info.fileName = "bad.txt";
  info.fileSize = 100;

  EXPECT_CALL(*mockWriter_, beginFile(_, _))
      .WillOnce(Return(std::unexpected(file::FileWriterError::PathTraversal)));

  receiver_->onPacket(makeFrame(info));
  EXPECT_TRUE(receiver_->isRejected());

  // Chunk should be ignored - no expectation on mockWriter_
  FileChunk chunk;
  chunk.offset = 0;
  chunk.data = {0x01};

  // No EXPECT_CALL - StrictMock will fail if writeChunk is called
  receiver_->onPacket(makeFrame(chunk));
}

// ============================================================================
// onPacket(FileDone) Tests
// ============================================================================

TEST_F(FileReceiverTest, OnFileDone_CallsFinishFile) {
  // Setup: open file
  FileInfo info;
  info.fileName = "test.txt";
  info.fileSize = 100;

  EXPECT_CALL(*mockWriter_, beginFile(_, _))
      .WillOnce(Return(std::expected<void, file::FileWriterError>{}));

  receiver_->onPacket(makeFrame(info));

  // FileDone
  FileDone done;
  done.fileSize = 100;

  EXPECT_CALL(*mockWriter_, finishFile(100)).WillOnce(Return(true));

  receiver_->onPacket(makeFrame(done));

  EXPECT_TRUE(ackReceived_);
  EXPECT_EQ(lastAck_.offset, 100);
}

TEST_F(FileReceiverTest, OnFileDone_NoAck_WhenInvalid) {
  // Setup: open file
  FileInfo info;
  info.fileName = "test.txt";
  info.fileSize = 100;

  EXPECT_CALL(*mockWriter_, beginFile(_, _))
      .WillOnce(Return(std::expected<void, file::FileWriterError>{}));

  receiver_->onPacket(makeFrame(info));

  // FileDone with wrong size (finishFile returns false)
  FileDone done;
  done.fileSize = 100;

  EXPECT_CALL(*mockWriter_, finishFile(100))
      .WillOnce(Return(false)); // Integrity check failed

  receiver_->onPacket(makeFrame(done));

  EXPECT_FALSE(ackReceived_); // No ack sent
}

TEST_F(FileReceiverTest, OnFileDone_Skipped_WhenRejected) {
  // Send FileInfo that gets rejected
  FileInfo info;
  info.fileName = "bad.txt";
  info.fileSize = 100;

  EXPECT_CALL(*mockWriter_, beginFile(_, _))
      .WillOnce(Return(std::unexpected(file::FileWriterError::PathTraversal)));

  receiver_->onPacket(makeFrame(info));

  // FileDone should not call finishFile or send ack
  FileDone done;
  done.fileSize = 100;

  // No EXPECT_CALL on finishFile
  receiver_->onPacket(makeFrame(done));

  EXPECT_FALSE(ackReceived_);
}

// ============================================================================
// onPacket(Handshake) Tests
// ============================================================================

TEST_F(FileReceiverTest, OnHandshake_DoesNotCrash) {
  Handshake pkt;
  pkt.protocolVersion = 1;
  pkt.capabilities = 0;

  // No expectations - just verify it doesn't crash
  EXPECT_NO_THROW(receiver_->onPacket(makeFrame(pkt)));
}

// ============================================================================
// onPacket(Ack) Tests
// ============================================================================

TEST_F(FileReceiverTest, OnAck_DoesNotCrash) {
  Ack pkt;
  pkt.offset = 12345;

  EXPECT_NO_THROW(receiver_->onPacket(makeFrame(pkt)));
}

// ============================================================================
// onPacket(Error) Tests
// ============================================================================

TEST_F(FileReceiverTest, OnError_DoesNotCrash) {
  Error pkt;
  pkt.code = 500;
  pkt.message = "Something went wrong";

  EXPECT_NO_THROW(receiver_->onPacket(makeFrame(pkt)));
}

// ============================================================================
// onDisconnect Tests
// ============================================================================

TEST_F(FileReceiverTest, OnDisconnect_ClosesWriter) {
  EXPECT_CALL(*mockWriter_, close()).Times(1);

  receiver_->onDisconnect();
}

// ============================================================================
// Full Transfer Sequence
// ============================================================================

TEST_F(FileReceiverTest, FullTransfer_Success) {
  // 1. FileInfo
  FileInfo info;
  info.fileName = "complete.txt";
  info.fileSize = 10;

  EXPECT_CALL(*mockWriter_, beginFile(std::string_view("complete.txt"), 10))
      .WillOnce(Return(std::expected<void, file::FileWriterError>{}));

  receiver_->onPacket(makeFrame(info));

  // 2. First chunk
  FileChunk chunk1;
  chunk1.offset = 0;
  chunk1.data = {1, 2, 3, 4, 5};

  EXPECT_CALL(*mockWriter_, writeChunk(0, _))
      .WillOnce(Return(std::expected<void, file::FileWriterError>{}));

  receiver_->onPacket(makeFrame(chunk1));

  // 3. Second chunk
  FileChunk chunk2;
  chunk2.offset = 5;
  chunk2.data = {6, 7, 8, 9, 10};

  EXPECT_CALL(*mockWriter_, writeChunk(5, _))
      .WillOnce(Return(std::expected<void, file::FileWriterError>{}));

  receiver_->onPacket(makeFrame(chunk2));

  // 4. FileDone
  FileDone done;
  done.fileSize = 10;

  EXPECT_CALL(*mockWriter_, finishFile(10)).WillOnce(Return(true));

  receiver_->onPacket(makeFrame(done));

  // Verify ack was sent
  EXPECT_TRUE(ackReceived_);
  EXPECT_EQ(lastAck_.offset, 10);
}
