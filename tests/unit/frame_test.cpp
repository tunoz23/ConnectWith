// Unit tests for cw::packet::Frame - frame building and parsing
// Pure logic tests - no I/O, no mocks needed

#include <gtest/gtest.h>
#include <limits>
#include <vector>

#include "cw/frame.h"
#include "cw/protocol/packet/packet.h"

using namespace cw::packet;

// ============================================================================
// ParsedFrame Tests
// ============================================================================

TEST(ParsedFrameTest, Size_ReturnsPayloadSize) {
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  ParsedFrame frame{.payload = payload, .type = PacketType::Ack};

  EXPECT_EQ(frame.size(), 5);
}

TEST(ParsedFrameTest, TotalSize_IncludesHeader) {
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  ParsedFrame frame{.payload = payload, .type = PacketType::Ack};

  EXPECT_EQ(frame.totalSize(), kFrameHeaderSize + 5);
}

TEST(ParsedFrameTest, HeaderSize_IsTenBytes) {
  EXPECT_EQ(kFrameHeaderSize, 10);
}

// ============================================================================
// buildFrame Tests
// ============================================================================

TEST(BuildFrameTest, Ack_CorrectSize) {
  Ack pkt;
  pkt.offset = 0x12345678;

  auto frame = buildFrame(pkt);

  // Header (10) + offset (8) = 18 bytes
  EXPECT_EQ(frame.size(), 18);
}

TEST(BuildFrameTest, Ack_CorrectHeader) {
  Ack pkt;
  pkt.offset = 0;

  auto frame = buildFrame(pkt);

  // Payload length = 8 (uint64_t offset)
  // First 8 bytes = big-endian length
  EXPECT_EQ(frame[0], 0x00);
  EXPECT_EQ(frame[7], 0x08);

  // Next 2 bytes = packet type (Ack = 5)
  EXPECT_EQ(frame[8], 0x00);
  EXPECT_EQ(frame[9], 0x05);
}

TEST(BuildFrameTest, Handshake_CorrectSize) {
  Handshake pkt;
  pkt.protocolVersion = 1;
  pkt.capabilities = 0;

  auto frame = buildFrame(pkt);

  // Header (10) + version (2) + capabilities (4) = 16 bytes
  EXPECT_EQ(frame.size(), 16);
}

TEST(BuildFrameTest, FileInfo_CorrectSize) {
  FileInfo pkt;
  pkt.fileSize = 1024;
  pkt.fileName = "test.txt";

  auto frame = buildFrame(pkt);

  // Header (10) + fileSize (8) + nameLen (4) + name (8) = 30
  EXPECT_EQ(frame.size(), 30);
}

TEST(BuildFrameTest, FileChunk_CorrectSize) {
  FileChunk pkt;
  pkt.offset = 0;
  pkt.data = {1, 2, 3, 4, 5};

  auto frame = buildFrame(pkt);

  // Header (10) + offset (8) + dataLen (4) + data (5) = 27
  EXPECT_EQ(frame.size(), 27);
}

TEST(BuildFrameTest, FileDone_CorrectSize) {
  FileDone pkt;
  pkt.fileSize = 0;

  auto frame = buildFrame(pkt);

  // Header (10) + fileSize (8) = 18
  EXPECT_EQ(frame.size(), 18);
}

TEST(BuildFrameTest, Error_CorrectSize) {
  Error pkt;
  pkt.code = 1;
  pkt.message = "test error";

  auto frame = buildFrame(pkt);

  // Header (10) + code (2) + msgLen (4) + message (10) = 26
  EXPECT_EQ(frame.size(), 26);
}

// ============================================================================
// tryParseFrame Tests
// ============================================================================

TEST(TryParseFrameTest, EmptyBuffer_ReturnsNullopt) {
  std::vector<uint8_t> buffer;
  auto result = tryParseFrame(buffer);
  EXPECT_FALSE(result.has_value());
}

TEST(TryParseFrameTest, IncompleteHeader_ReturnsNullopt) {
  // Less than 10 bytes
  std::vector<uint8_t> buffer = {0x00, 0x00, 0x00, 0x00, 0x00};
  auto result = tryParseFrame(buffer);
  EXPECT_FALSE(result.has_value());
}

TEST(TryParseFrameTest, HeaderOnly_NoPayload_ReturnsNullopt) {
  // Header says payload is 8 bytes, but no payload present
  std::vector<uint8_t> buffer = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, // len = 8
      0x00, 0x05                                      // type = Ack
  };
  auto result = tryParseFrame(buffer);
  EXPECT_FALSE(result.has_value());
}

TEST(TryParseFrameTest, ValidFrame_ReturnsFrame) {
  Ack original;
  original.offset = 12345;
  auto data = buildFrame(original);

  auto result = tryParseFrame(data);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->type, PacketType::Ack);
  EXPECT_EQ(result->payload.size(), 8);
}

TEST(TryParseFrameTest, ExactSize_Works) {
  Ack pkt;
  pkt.offset = 0;
  auto data = buildFrame(pkt);

  // Exactly the right number of bytes
  auto result = tryParseFrame(data);
  ASSERT_TRUE(result.has_value());
}

TEST(TryParseFrameTest, ExtraBytes_Works) {
  Ack pkt;
  pkt.offset = 0;
  auto data = buildFrame(pkt);

  // Add extra bytes (next frame start)
  data.push_back(0x00);
  data.push_back(0x00);

  auto result = tryParseFrame(data);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->totalSize(), 18); // Original frame size only
}

TEST(TryParseFrameTest, HugePayloadLength_ReturnsNullopt) {
  // Payload length > kMaxPayloadSize (1GB)
  std::vector<uint8_t> buffer = {
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // huge length
      0x00, 0x05                                      // type
  };
  auto result = tryParseFrame(buffer);
  EXPECT_FALSE(result.has_value());
}

TEST(TryParseFrameTest, ZeroPayload_Works) {
  // Valid frame with 0-byte payload
  std::vector<uint8_t> buffer = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // len = 0
      0x00, 0x00                                      // type = Handshake
  };
  auto result = tryParseFrame(buffer);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->payload.size(), 0);
}

// ============================================================================
// parseFrame Tests (throwing version)
// ============================================================================

TEST(ParseFrameTest, ValidFrame_Works) {
  Ack original;
  original.offset = 999;
  auto data = buildFrame(original);

  ParsedFrame frame = parseFrame(data);

  EXPECT_EQ(frame.type, PacketType::Ack);
  EXPECT_EQ(frame.payload.size(), 8);
}

TEST(ParseFrameTest, EmptyBuffer_Throws) {
  std::vector<uint8_t> buffer;
  EXPECT_THROW(parseFrame(buffer), std::runtime_error);
}

TEST(ParseFrameTest, IncompleteHeader_Throws) {
  std::vector<uint8_t> buffer = {0x00, 0x00, 0x00};
  EXPECT_THROW(parseFrame(buffer), std::runtime_error);
}

TEST(ParseFrameTest, HugePayloadLength_Throws) {
  std::vector<uint8_t> buffer = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                 0xFF, 0xFF, 0xFF, 0x00, 0x05};
  EXPECT_THROW(parseFrame(buffer), std::runtime_error);
}

TEST(ParseFrameTest, IncompletePayload_Throws) {
  std::vector<uint8_t> buffer = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, // len = 16
      0x00, 0x05,                                     // type
      0x01, 0x02                                      // only 2 bytes of payload
  };
  EXPECT_THROW(parseFrame(buffer), std::runtime_error);
}

// ============================================================================
// Round-Trip Tests (build + parse)
// ============================================================================

TEST(FrameRoundTripTest, Ack) {
  Ack original;
  original.offset = 0xDEADBEEFCAFEBABE;

  auto frame = buildFrame(original);
  auto parsed = parseFrame(frame);
  auto reconstructed = Ack::deserialize(parsed.payload);

  EXPECT_EQ(parsed.type, PacketType::Ack);
  EXPECT_EQ(reconstructed.offset, original.offset);
}

TEST(FrameRoundTripTest, Handshake) {
  Handshake original;
  original.protocolVersion = 42;
  original.capabilities = 0x12345678;

  auto frame = buildFrame(original);
  auto parsed = parseFrame(frame);
  auto reconstructed = Handshake::deserialize(parsed.payload);

  EXPECT_EQ(parsed.type, PacketType::Handshake);
  EXPECT_EQ(reconstructed.protocolVersion, original.protocolVersion);
  EXPECT_EQ(reconstructed.capabilities, original.capabilities);
}

TEST(FrameRoundTripTest, FileInfo) {
  FileInfo original;
  original.fileSize = 1024 * 1024;
  original.fileName = "path/to/file.dat";

  auto frame = buildFrame(original);
  auto parsed = parseFrame(frame);
  auto reconstructed = FileInfo::deserialize(parsed.payload);

  EXPECT_EQ(parsed.type, PacketType::FileInfo);
  EXPECT_EQ(reconstructed.fileSize, original.fileSize);
  EXPECT_EQ(reconstructed.fileName, original.fileName);
}

TEST(FrameRoundTripTest, FileChunk) {
  FileChunk original;
  original.offset = 4096;
  original.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

  auto frame = buildFrame(original);
  auto parsed = parseFrame(frame);
  auto reconstructed = FileChunk::deserialize(parsed.payload);

  EXPECT_EQ(parsed.type, PacketType::FileChunk);
  EXPECT_EQ(reconstructed.offset, original.offset);
  EXPECT_EQ(reconstructed.data, original.data);
}

TEST(FrameRoundTripTest, FileDone) {
  FileDone original;
  original.fileSize = std::numeric_limits<uint64_t>::max();

  auto frame = buildFrame(original);
  auto parsed = parseFrame(frame);
  auto reconstructed = FileDone::deserialize(parsed.payload);

  EXPECT_EQ(parsed.type, PacketType::FileDone);
  EXPECT_EQ(reconstructed.fileSize, original.fileSize);
}

TEST(FrameRoundTripTest, Error) {
  Error original;
  original.code = 404;
  original.message = "File not found";

  auto frame = buildFrame(original);
  auto parsed = parseFrame(frame);
  auto reconstructed = Error::deserialize(parsed.payload);

  EXPECT_EQ(parsed.type, PacketType::Error);
  EXPECT_EQ(reconstructed.code, original.code);
  EXPECT_EQ(reconstructed.message, original.message);
}
