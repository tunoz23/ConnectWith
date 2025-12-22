// Comprehensive unit tests for all packet types in cw::packet
// Pure logic tests - no I/O, no mocks needed

#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

#include "cw/frame.h"
#include "cw/protocol/packet/packet.h"

using namespace cw::packet;

// ============================================================================
// Handshake Packet Tests
// ============================================================================

class HandshakeTest : public ::testing::Test {
protected:
  Handshake pkt;
};

TEST_F(HandshakeTest, PayloadSize_IsFixed) {
  EXPECT_EQ(pkt.payloadSize(), sizeof(uint16_t) + sizeof(uint32_t));
}

TEST_F(HandshakeTest, DefaultVersion_IsCurrent) {
  EXPECT_EQ(pkt.protocolVersion, Handshake::kCurrentVersion);
}

TEST_F(HandshakeTest, Serialize_ProducesCorrectBytes) {
  pkt.protocolVersion = 1;
  pkt.capabilities = 0x12345678;

  std::vector<uint8_t> out;
  pkt.serialize(out);

  ASSERT_EQ(out.size(), 6);
  // Big-endian version (0x0001)
  EXPECT_EQ(out[0], 0x00);
  EXPECT_EQ(out[1], 0x01);
  // Big-endian capabilities (0x12345678)
  EXPECT_EQ(out[2], 0x12);
  EXPECT_EQ(out[3], 0x34);
  EXPECT_EQ(out[4], 0x56);
  EXPECT_EQ(out[5], 0x78);
}

TEST_F(HandshakeTest, Deserialize_RecoversSameValues) {
  pkt.protocolVersion = 42;
  pkt.capabilities = 0xDEADBEEF;

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = Handshake::deserialize(out);
  EXPECT_EQ(decoded.protocolVersion, 42);
  EXPECT_EQ(decoded.capabilities, 0xDEADBEEF);
}

TEST_F(HandshakeTest, Deserialize_ThrowsOnTooSmall) {
  std::vector<uint8_t> tiny = {0x00, 0x01};
  EXPECT_THROW(Handshake::deserialize(tiny), std::runtime_error);
}

// ============================================================================
// Ack Packet Tests
// ============================================================================

class AckTest : public ::testing::Test {
protected:
  Ack pkt;
};

TEST_F(AckTest, PayloadSize_IsEightBytes) {
  EXPECT_EQ(pkt.payloadSize(), sizeof(uint64_t));
}

TEST_F(AckTest, Serialize_ProducesCorrectBytes) {
  pkt.offset = 0x0102030405060708ULL;

  std::vector<uint8_t> out;
  pkt.serialize(out);

  ASSERT_EQ(out.size(), 8);
  EXPECT_EQ(out[0], 0x01);
  EXPECT_EQ(out[7], 0x08);
}

TEST_F(AckTest, Deserialize_RecoversSameOffset) {
  pkt.offset = 0xCAFEBABEDEADBEEF;

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = Ack::deserialize(out);
  EXPECT_EQ(decoded.offset, pkt.offset);
}

TEST_F(AckTest, Deserialize_ThrowsOnTooSmall) {
  std::vector<uint8_t> tiny = {0x00, 0x01, 0x02, 0x03};
  EXPECT_THROW(Ack::deserialize(tiny), std::runtime_error);
}

TEST_F(AckTest, RoundTrip_EdgeValues) {
  for (uint64_t val :
       {uint64_t{0}, uint64_t{1}, std::numeric_limits<uint64_t>::max()}) {
    pkt.offset = val;
    std::vector<uint8_t> out;
    pkt.serialize(out);

    auto decoded = Ack::deserialize(out);
    EXPECT_EQ(decoded.offset, val);
  }
}

// ============================================================================
// Error Packet Tests
// ============================================================================

class ErrorTest : public ::testing::Test {
protected:
  Error pkt;
};

TEST_F(ErrorTest, PayloadSize_IncludesMessage) {
  pkt.code = 1;
  pkt.message = "test";
  EXPECT_EQ(pkt.payloadSize(), sizeof(uint16_t) + sizeof(uint32_t) + 4);
}

TEST_F(ErrorTest, Serialize_Deserialize_RoundTrip) {
  pkt.code = 500;
  pkt.message = "Internal server error";

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = Error::deserialize(out);
  EXPECT_EQ(decoded.code, 500);
  EXPECT_EQ(decoded.message, "Internal server error");
}

TEST_F(ErrorTest, Serialize_EmptyMessage) {
  pkt.code = 200;
  pkt.message = "";

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = Error::deserialize(out);
  EXPECT_EQ(decoded.code, 200);
  EXPECT_EQ(decoded.message, "");
}

TEST_F(ErrorTest, Serialize_ThrowsOnMessageTooLong) {
  pkt.code = 1;
  pkt.message = std::string(kMaxStringLength + 1, 'x');

  std::vector<uint8_t> out;
  EXPECT_THROW(pkt.serialize(out), std::length_error);
}

TEST_F(ErrorTest, Deserialize_ThrowsOnTooSmall) {
  std::vector<uint8_t> tiny = {0x00};
  EXPECT_THROW(Error::deserialize(tiny), std::runtime_error);
}

TEST_F(ErrorTest, Deserialize_ThrowsOnCorruptedLength) {
  // Header claims message is huge
  std::vector<uint8_t> corrupted = {
      0x00, 0x01,            // code
      0xFF, 0xFF, 0xFF, 0xFF // msgLen = max
  };
  EXPECT_THROW(Error::deserialize(corrupted), std::runtime_error);
}

// ============================================================================
// FileInfo Packet Tests
// ============================================================================

class FileInfoTest : public ::testing::Test {
protected:
  FileInfo pkt;
};

TEST_F(FileInfoTest, PayloadSize_IncludesFilename) {
  pkt.fileSize = 1024;
  pkt.fileName = "test.txt";
  EXPECT_EQ(pkt.payloadSize(), sizeof(uint64_t) + sizeof(uint32_t) + 8);
}

TEST_F(FileInfoTest, Serialize_Deserialize_RoundTrip) {
  pkt.fileSize = 1024 * 1024 * 100; // 100 MB
  pkt.fileName = "large_file.bin";

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = FileInfo::deserialize(out);
  EXPECT_EQ(decoded.fileSize, pkt.fileSize);
  EXPECT_EQ(decoded.fileName, pkt.fileName);
}

TEST_F(FileInfoTest, Serialize_ThrowsOnEmptyFilename) {
  pkt.fileSize = 100;
  pkt.fileName = "";

  std::vector<uint8_t> out;
  EXPECT_THROW(pkt.serialize(out), std::length_error);
}

TEST_F(FileInfoTest, Serialize_ThrowsOnFilenameTooLong) {
  pkt.fileSize = 100;
  pkt.fileName = std::string(kMaxStringLength + 1, 'x');

  std::vector<uint8_t> out;
  EXPECT_THROW(pkt.serialize(out), std::length_error);
}

TEST_F(FileInfoTest, Deserialize_ThrowsOnTooSmall) {
  std::vector<uint8_t> tiny = {0x00, 0x01};
  EXPECT_THROW(FileInfo::deserialize(tiny), std::runtime_error);
}

TEST_F(FileInfoTest, RoundTrip_MaxFilename) {
  pkt.fileSize = 0;
  pkt.fileName = std::string(kMaxStringLength, 'a');

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = FileInfo::deserialize(out);
  EXPECT_EQ(decoded.fileName.size(), kMaxStringLength);
}

TEST_F(FileInfoTest, RoundTrip_PathWithSlashes) {
  pkt.fileSize = 100;
  pkt.fileName = "path/to/nested/file.txt";

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = FileInfo::deserialize(out);
  EXPECT_EQ(decoded.fileName, "path/to/nested/file.txt");
}

// ============================================================================
// FileChunk Packet Tests
// ============================================================================

class FileChunkTest : public ::testing::Test {
protected:
  FileChunk pkt;
};

TEST_F(FileChunkTest, PayloadSize_IncludesData) {
  pkt.offset = 0;
  pkt.data = {1, 2, 3, 4, 5};
  EXPECT_EQ(pkt.payloadSize(), sizeof(uint64_t) + sizeof(uint32_t) + 5);
}

TEST_F(FileChunkTest, Serialize_Deserialize_RoundTrip) {
  pkt.offset = 4096;
  pkt.data = {0xDE, 0xAD, 0xBE, 0xEF};

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = FileChunk::deserialize(out);
  EXPECT_EQ(decoded.offset, 4096);
  EXPECT_EQ(decoded.data, pkt.data);
}

TEST_F(FileChunkTest, Serialize_EmptyData) {
  pkt.offset = 0;
  pkt.data = {};

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = FileChunk::deserialize(out);
  EXPECT_EQ(decoded.offset, 0);
  EXPECT_TRUE(decoded.data.empty());
}

TEST_F(FileChunkTest, Deserialize_ThrowsOnTooSmall) {
  std::vector<uint8_t> tiny = {0x00, 0x01, 0x02};
  EXPECT_THROW(FileChunk::deserialize(tiny), std::runtime_error);
}

TEST_F(FileChunkTest, Deserialize_ThrowsOnHugeDataLength) {
  // Header claims data is larger than max
  std::vector<uint8_t> corrupted(12, 0);
  // Set dataLen to something huge
  corrupted[8] = 0xFF;
  corrupted[9] = 0xFF;
  corrupted[10] = 0xFF;
  corrupted[11] = 0xFF;

  EXPECT_THROW(FileChunk::deserialize(corrupted), std::runtime_error);
}

TEST_F(FileChunkTest, RoundTrip_LargeOffset) {
  pkt.offset = std::numeric_limits<uint64_t>::max() - 1;
  pkt.data = {0x42};

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = FileChunk::deserialize(out);
  EXPECT_EQ(decoded.offset, pkt.offset);
}

// ============================================================================
// FileDone Packet Tests
// ============================================================================

class FileDoneTest : public ::testing::Test {
protected:
  FileDone pkt;
};

TEST_F(FileDoneTest, PayloadSize_IsEightBytes) {
  EXPECT_EQ(pkt.payloadSize(), sizeof(uint64_t));
}

TEST_F(FileDoneTest, Serialize_Deserialize_RoundTrip) {
  pkt.fileSize = 1024 * 1024;

  std::vector<uint8_t> out;
  pkt.serialize(out);

  auto decoded = FileDone::deserialize(out);
  EXPECT_EQ(decoded.fileSize, pkt.fileSize);
}

TEST_F(FileDoneTest, Deserialize_ThrowsOnTooSmall) {
  std::vector<uint8_t> tiny = {0x00, 0x01, 0x02, 0x03};
  EXPECT_THROW(FileDone::deserialize(tiny), std::runtime_error);
}

TEST_F(FileDoneTest, RoundTrip_EdgeValues) {
  for (uint64_t val :
       {uint64_t{0}, uint64_t{1}, std::numeric_limits<uint64_t>::max()}) {
    pkt.fileSize = val;
    std::vector<uint8_t> out;
    pkt.serialize(out);

    auto decoded = FileDone::deserialize(out);
    EXPECT_EQ(decoded.fileSize, val);
  }
}

// ============================================================================
// Parameterized Tests for FileInfo Edge Cases
// ============================================================================

struct FileInfoEdgeCase {
  std::string name;
  uint64_t size;
  bool shouldThrow;

  friend std::ostream &operator<<(std::ostream &os,
                                  const FileInfoEdgeCase &tc) {
    return os << "name=" << tc.name.size() << " bytes, size=" << tc.size;
  }
};

class FileInfoParameterizedTest
    : public ::testing::TestWithParam<FileInfoEdgeCase> {};

TEST_P(FileInfoParameterizedTest, SerializeDeserialize) {
  const auto &tc = GetParam();

  FileInfo pkt;
  pkt.fileName = tc.name;
  pkt.fileSize = tc.size;

  if (tc.shouldThrow) {
    std::vector<uint8_t> out;
    EXPECT_THROW(pkt.serialize(out), std::exception);
  } else {
    std::vector<uint8_t> out;
    ASSERT_NO_THROW(pkt.serialize(out));

    auto decoded = FileInfo::deserialize(out);
    EXPECT_EQ(decoded.fileName, tc.name);
    EXPECT_EQ(decoded.fileSize, tc.size);
  }
}

INSTANTIATE_TEST_SUITE_P(
    EdgeCases, FileInfoParameterizedTest,
    ::testing::Values(FileInfoEdgeCase{"a", 0, false},
                      FileInfoEdgeCase{"test.txt", 1, false},
                      FileInfoEdgeCase{"test.txt", UINT64_MAX, false},
                      FileInfoEdgeCase{std::string(4096, 'x'), 1, false},
                      FileInfoEdgeCase{std::string(4097, 'x'), 1, true},
                      FileInfoEdgeCase{"", 1, true}));
