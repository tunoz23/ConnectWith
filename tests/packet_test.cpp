#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "cw/frame.h"
#include "cw/protocol/packet/packet.h"

using namespace cw::packet;

// ============================================================================
// Ack Packet Tests
// ============================================================================

TEST(AckPacketTest, SerializationRoundTrip) {
  Ack original;
  original.offset = 0xDEADBEEF;

  std::vector<uint8_t> frame = buildFrame(original);
  ParsedFrame view = parseFrame(frame);
  Ack reconstructed = Ack::deserialize(view.payload);

  EXPECT_EQ(frame.size(), 18); // Header(10) + Payload(8)
  EXPECT_EQ(view.type, PacketType::Ack);
  EXPECT_EQ(original.offset, reconstructed.offset);
}

// ============================================================================
// Handshake Packet Tests
// ============================================================================

TEST(HandshakePacketTest, SerializationRoundTrip) {
  Handshake original;
  original.protocolVersion = 1;
  original.capabilities = 0x12345678;

  std::vector<uint8_t> frame = buildFrame(original);
  ParsedFrame view = parseFrame(frame);
  Handshake reconstructed = Handshake::deserialize(view.payload);

  EXPECT_EQ(view.type, PacketType::Handshake);
  EXPECT_EQ(original.protocolVersion, reconstructed.protocolVersion);
  EXPECT_EQ(original.capabilities, reconstructed.capabilities);
}

// ============================================================================
// Frame Security Tests
// ============================================================================

TEST(FrameSecurityTest, ThrowsOnGarbageLength) {
  std::vector<uint8_t> maliciousFrame = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                         0xFF, 0xFF, 0xFF, 0x00, 0x01};

  EXPECT_THROW(parseFrame(maliciousFrame), std::runtime_error);
}

TEST(FrameSecurityTest, TryParseReturnsNulloptOnIncomplete) {
  std::vector<uint8_t> incompleteFrame = {0x00, 0x00, 0x00};

  auto result = tryParseFrame(incompleteFrame);
  EXPECT_FALSE(result.has_value());
}

// ============================================================================
// FileInfo Packet Tests
// ============================================================================

class FileInfoTest : public ::testing::Test {
protected:
  void SetUp() override {
    pkt.fileSize = 1024;
    pkt.fileName = "config.json";
  }

  FileInfo pkt;
};

TEST_F(FileInfoTest, CorrectFrameSize) {
  auto frame = buildFrame(pkt);
  // Header(10) + Size(8) + NameLen(4) + Name(11) = 33 bytes
  ASSERT_EQ(frame.size(), 33);
}

TEST_F(FileInfoTest, ContentIntegrity) {
  auto frame = buildFrame(pkt);
  auto view = parseFrame(frame);
  auto decoded = FileInfo::deserialize(view.payload);

  EXPECT_EQ(decoded.fileName, "config.json");
  EXPECT_EQ(decoded.fileSize, 1024);
}

// ============================================================================
// FileChunk Packet Tests
// ============================================================================

TEST(FileChunkTest, SerializationRoundTrip) {
  FileChunk original;
  original.offset = 4096;
  original.data = {0x01, 0x02, 0x03, 0x04, 0x05};

  auto frame = buildFrame(original);
  auto view = parseFrame(frame);
  auto decoded = FileChunk::deserialize(view.payload);

  EXPECT_EQ(decoded.offset, 4096);
  EXPECT_EQ(decoded.data, original.data);
}

// ============================================================================
// Path Traversal Security Tests (SPEC E2)
// ============================================================================

#include "cw/file/path_validator.h"
#include <filesystem>

namespace fs = std::filesystem;

class PathSecurityTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Use current directory as base
    baseDir = fs::current_path();
  }

  fs::path baseDir;
};

TEST_F(PathSecurityTest, AllowsSimpleFilename) {
  EXPECT_TRUE(cw::file::isPathSafe("test.txt", baseDir));
}

TEST_F(PathSecurityTest, AllowsSubdirectory) {
  EXPECT_TRUE(cw::file::isPathSafe("subdir/test.txt", baseDir));
}

TEST_F(PathSecurityTest, BlocksParentTraversal) {
  EXPECT_FALSE(cw::file::isPathSafe("../test.txt", baseDir));
}

TEST_F(PathSecurityTest, BlocksDeepTraversal) {
  EXPECT_FALSE(cw::file::isPathSafe("../../../etc/passwd", baseDir));
}

TEST_F(PathSecurityTest, BlocksHiddenTraversal) {
  EXPECT_FALSE(cw::file::isPathSafe("subdir/../../test.txt", baseDir));
}

TEST_F(PathSecurityTest, BlocksAbsolutePathOutsideBase) {
  // Construct a path that's definitely outside base
  fs::path outsidePath = baseDir.parent_path() / "outside.txt";
  EXPECT_FALSE(cw::file::isPathSafe(outsidePath, baseDir));
}
