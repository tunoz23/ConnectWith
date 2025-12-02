#include <gtest/gtest.h>
#include <vector>
#include <string>

// Include your project headers
#include "../protocol/packet/packet.h"
#include "../Frame.h"

using namespace cw::packet;

// 1. SIMPLE TEST CASE
// Syntax: TEST(TestSuiteName, TestName)
TEST(AckPacketTest, SerializationRoundTrip) {
	// Arrange
	Ack original;
	original.offset = 0xDEADBEEF;

	// Act
	std::vector<uint8_t> frame = buildFrame(original);
	ParsedFrame view = parseFrame(frame);
	Ack reconstructed = Ack::deserialize(view.payload_view, view.size);

	// Assert
	// ASSERT_EQ stops the test if it fails. EXPECT_EQ continues.
	EXPECT_EQ(frame.size(), 18); // Header(10) + Payload(8)
	EXPECT_EQ(view.type, PacketType::Ack);
	EXPECT_EQ(original.offset, reconstructed.offset);
}

// 2. EXCEPTION TESTING (The "Garbage Data" Test)
TEST(FrameSecurityTest, ThrowsOnGarbageLength) {
	// [Length: Huge] [Type: Ack]
	std::vector<uint8_t> maliciousFrame = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x00, 0x01
	};

	// Verify that the code throws the specific exception
	EXPECT_THROW({
		parseFrame(maliciousFrame);
		}, std::runtime_error);
}

// 3. USING FIXTURES (Setup/Teardown logic)
// Useful if you need to setup a complex mock connection for every test
class FileInfoTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Runs before each test
		pkt.fileSize = 1024;
		pkt.fileName = "config.json";
	}

	void TearDown() override {
		// Runs after each test
	}

	FileInfo pkt;
};

// Note: Use TEST_F (F for Fixture) to access the class members
TEST_F(FileInfoTest, CorrectHeaderSize) {
	auto frame = buildFrame(pkt);
	// Header (10) + Size (8) + NameLen (4) + Name (11) = 33 bytes
	ASSERT_EQ(frame.size(), 33);
}

TEST_F(FileInfoTest, ContentIntegrity) {
	auto frame = buildFrame(pkt);
	auto view = parseFrame(frame);
	auto decoded = FileInfo::deserialize(view.payload_view, view.size);

	EXPECT_EQ(decoded.fileName, "config.json");
	EXPECT_EQ(decoded.fileSize, 1024);
}