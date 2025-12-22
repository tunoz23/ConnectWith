// Unit tests for cw::binary - endian conversion and serialization utilities
// Pure logic tests - no I/O, no mocks needed

#include <gtest/gtest.h>
#include <limits>
#include <vector>

#include "cw/binary.h"

using namespace cw::binary;

// ============================================================================
// toBigEndian / fromBigEndian Tests
// ============================================================================

TEST(BinaryTest, ToBigEndian_Uint8_IdentityOperation) {
  // Single byte values should be unchanged
  EXPECT_EQ(toBigEndian(uint8_t{0x00}), 0x00);
  EXPECT_EQ(toBigEndian(uint8_t{0xFF}), 0xFF);
  EXPECT_EQ(toBigEndian(uint8_t{0x42}), 0x42);
}

TEST(BinaryTest, ToBigEndian_Uint16_SwapsBytes) {
  if constexpr (std::endian::native == std::endian::little) {
    EXPECT_EQ(toBigEndian(uint16_t{0x1234}), 0x3412);
    EXPECT_EQ(toBigEndian(uint16_t{0xABCD}), 0xCDAB);
  } else {
    EXPECT_EQ(toBigEndian(uint16_t{0x1234}), 0x1234);
  }
}

TEST(BinaryTest, ToBigEndian_Uint32_SwapsBytes) {
  if constexpr (std::endian::native == std::endian::little) {
    EXPECT_EQ(toBigEndian(uint32_t{0x12345678}), 0x78563412);
  } else {
    EXPECT_EQ(toBigEndian(uint32_t{0x12345678}), 0x12345678);
  }
}

TEST(BinaryTest, ToBigEndian_Uint64_SwapsBytes) {
  if constexpr (std::endian::native == std::endian::little) {
    EXPECT_EQ(toBigEndian(uint64_t{0x0102030405060708ULL}),
              0x0807060504030201ULL);
  } else {
    EXPECT_EQ(toBigEndian(uint64_t{0x0102030405060708ULL}),
              0x0102030405060708ULL);
  }
}

TEST(BinaryTest, RoundTrip_AllTypes) {
  // toBigEndian(fromBigEndian(x)) and fromBigEndian(toBigEndian(x)) should
  // both be identity operations

  uint8_t v8 = 0xAB;
  EXPECT_EQ(fromBigEndian(toBigEndian(v8)), v8);

  uint16_t v16 = 0x1234;
  EXPECT_EQ(fromBigEndian(toBigEndian(v16)), v16);

  uint32_t v32 = 0xDEADBEEF;
  EXPECT_EQ(fromBigEndian(toBigEndian(v32)), v32);

  uint64_t v64 = 0x123456789ABCDEF0ULL;
  EXPECT_EQ(fromBigEndian(toBigEndian(v64)), v64);
}

TEST(BinaryTest, RoundTrip_EdgeValues) {
  // Test boundary values
  EXPECT_EQ(fromBigEndian(toBigEndian(uint16_t{0})), 0);
  EXPECT_EQ(fromBigEndian(toBigEndian(uint16_t{0xFFFF})), 0xFFFF);
  EXPECT_EQ(fromBigEndian(toBigEndian(uint32_t{0})), 0);
  EXPECT_EQ(fromBigEndian(toBigEndian(uint32_t{0xFFFFFFFF})), 0xFFFFFFFF);
  EXPECT_EQ(fromBigEndian(toBigEndian(uint64_t{0})), 0);
  EXPECT_EQ(fromBigEndian(toBigEndian(std::numeric_limits<uint64_t>::max())),
            std::numeric_limits<uint64_t>::max());
}

// ============================================================================
// writeBigEndian Tests
// ============================================================================

TEST(BinaryTest, WriteBigEndian_ToPointer_Uint16) {
  uint8_t buffer[2] = {0};
  writeBigEndian(buffer, uint16_t{0x1234});

  // Should be big-endian in memory
  EXPECT_EQ(buffer[0], 0x12);
  EXPECT_EQ(buffer[1], 0x34);
}

TEST(BinaryTest, WriteBigEndian_ToPointer_Uint32) {
  uint8_t buffer[4] = {0};
  writeBigEndian(buffer, uint32_t{0x12345678});

  EXPECT_EQ(buffer[0], 0x12);
  EXPECT_EQ(buffer[1], 0x34);
  EXPECT_EQ(buffer[2], 0x56);
  EXPECT_EQ(buffer[3], 0x78);
}

TEST(BinaryTest, WriteBigEndian_ToPointer_Uint64) {
  uint8_t buffer[8] = {0};
  writeBigEndian(buffer, uint64_t{0x0102030405060708ULL});

  EXPECT_EQ(buffer[0], 0x01);
  EXPECT_EQ(buffer[1], 0x02);
  EXPECT_EQ(buffer[2], 0x03);
  EXPECT_EQ(buffer[3], 0x04);
  EXPECT_EQ(buffer[4], 0x05);
  EXPECT_EQ(buffer[5], 0x06);
  EXPECT_EQ(buffer[6], 0x07);
  EXPECT_EQ(buffer[7], 0x08);
}

TEST(BinaryTest, WriteBigEndian_ToVector_AppendsCorrectly) {
  std::vector<uint8_t> buffer;

  writeBigEndian(buffer, uint16_t{0x1234});
  ASSERT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], 0x12);
  EXPECT_EQ(buffer[1], 0x34);

  // Append another value
  writeBigEndian(buffer, uint32_t{0xDEADBEEF});
  ASSERT_EQ(buffer.size(), 6);
  EXPECT_EQ(buffer[2], 0xDE);
  EXPECT_EQ(buffer[3], 0xAD);
  EXPECT_EQ(buffer[4], 0xBE);
  EXPECT_EQ(buffer[5], 0xEF);
}

TEST(BinaryTest, WriteBigEndian_ToVector_EmptyVector) {
  std::vector<uint8_t> buffer;
  writeBigEndian(buffer, uint64_t{0});

  ASSERT_EQ(buffer.size(), 8);
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(buffer[i], 0);
  }
}

// ============================================================================
// readBigEndian Tests
// ============================================================================

TEST(BinaryTest, ReadBigEndian_FromPointer_Uint16) {
  uint8_t buffer[] = {0x12, 0x34};
  uint16_t value = readBigEndian<uint16_t>(buffer);
  EXPECT_EQ(value, 0x1234);
}

TEST(BinaryTest, ReadBigEndian_FromPointer_Uint32) {
  uint8_t buffer[] = {0x12, 0x34, 0x56, 0x78};
  uint32_t value = readBigEndian<uint32_t>(buffer);
  EXPECT_EQ(value, 0x12345678);
}

TEST(BinaryTest, ReadBigEndian_FromPointer_Uint64) {
  uint8_t buffer[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  uint64_t value = readBigEndian<uint64_t>(buffer);
  EXPECT_EQ(value, 0x0102030405060708ULL);
}

TEST(BinaryTest, ReadBigEndian_FromSpan) {
  std::vector<uint8_t> buffer = {0xAB, 0xCD};
  uint16_t value = readBigEndian<uint16_t>(std::span<const uint8_t>(buffer));
  EXPECT_EQ(value, 0xABCD);
}

// ============================================================================
// Write/Read Round-Trip Tests
// ============================================================================

TEST(BinaryTest, WriteReadRoundTrip_AllTypes) {
  std::vector<uint8_t> buffer;

  uint16_t v16 = 0x1234;
  uint32_t v32 = 0xDEADBEEF;
  uint64_t v64 = 0x123456789ABCDEF0ULL;

  writeBigEndian(buffer, v16);
  writeBigEndian(buffer, v32);
  writeBigEndian(buffer, v64);

  ASSERT_EQ(buffer.size(), 2 + 4 + 8);

  const uint8_t *ptr = buffer.data();
  EXPECT_EQ(readBigEndian<uint16_t>(ptr), v16);
  EXPECT_EQ(readBigEndian<uint32_t>(ptr + 2), v32);
  EXPECT_EQ(readBigEndian<uint64_t>(ptr + 6), v64);
}

// ============================================================================
// Parameterized Tests for Multiple Values
// ============================================================================

class BinaryParameterizedTest : public ::testing::TestWithParam<uint64_t> {};

TEST_P(BinaryParameterizedTest, RoundTrip_Uint64) {
  uint64_t value = GetParam();
  EXPECT_EQ(fromBigEndian(toBigEndian(value)), value);
}

INSTANTIATE_TEST_SUITE_P(
    BoundaryValues, BinaryParameterizedTest,
    ::testing::Values(0ULL, 1ULL, 255ULL, 256ULL, 65535ULL, 65536ULL,
                      0xFFFFFFFFULL, 0x100000000ULL,
                      std::numeric_limits<uint64_t>::max() / 2,
                      std::numeric_limits<uint64_t>::max()));
