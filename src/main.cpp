#include <iostream>
#include <vector>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <asio.hpp>




#include "endian.h"
#include "packet/packet.h"
#include "Frame.h"
// -- - MOCKS FOR STANDALONE COMPILATION(Remove these if linking real files) -- -
// If you compile this file alone, keep these. If you link against your project, remove them.
// --- MOCKS FOR STANDALONE COMPILATION (Remove these if linking real files) ---
// If you compile this file alone, keep these. If you link against your project, remove them.
#ifndef CW_MOCKS_DEFINED
#define CW_MOCKS_DEFINED
// Mocking the headers for the sake of this file block generation
// In your real project, just include your headers.
#endif 
// ---------------------------------------------------------------------------

// Helper to visualize the buffer
void printHex(const std::vector<uint8_t>& data) {
	std::cout << "Buffer [" << data.size() << " bytes]:\n";
	std::cout << std::hex << std::setfill('0');
	for (size_t i = 0; i < data.size(); ++i) {
		std::cout << std::setw(2) << static_cast<int>(data[i]) << " ";
		if ((i + 1) % 16 == 0) std::cout << "\n";
	}
	std::cout << std::dec << "\n\n";
}

int main() {
	using namespace cw::packet;

	// 1. COMPILE-TIME CONTRACT CHECK
	static_assert(FrameBuildable<Ack>, "Ack struct does not satisfy FrameBuildable!");
	static_assert(FrameBuildable<FileInfo>, "FileInfo struct does not satisfy FrameBuildable!");

	std::cout << "=== Test 1: Ack Packet ===\n";
	{
		Ack ackPkt;
		ackPkt.offset = 0xDEADBEEF;

		// 1. SERIALIZE (Build Frame)
		std::vector<uint8_t> frame = buildFrame(ackPkt);

		printHex(frame);

		// 2. VERIFY WIRE FORMAT
		assert(frame.size() == 18);
		assert(frame[7] == 0x08); // Len

		// Check Packet Type (Dynamic)
		uint16_t typeInFrame = (static_cast<uint16_t>(frame[8]) << 8) | frame[9];
		assert(typeInFrame == static_cast<uint16_t>(PacketType::Ack));

		assert(frame[14] == 0xDE); // Offset High Byte

		std::cout << "Ack Wire Format Verified.\n";

		// 3. ROUND-TRIP DESERIALIZATION
		// Simulate the Network Layer stripping the Frame Header
		// Header = 8 bytes (Len) + 2 bytes (Type) = 10 bytes
		constexpr size_t HEADER_SIZE = 10;

		if (frame.size() < HEADER_SIZE) throw std::runtime_error("Frame too small");

		const uint8_t* payloadPtr = frame.data() + HEADER_SIZE;
		size_t payloadSize = frame.size() - HEADER_SIZE;

		// Attempt to reconstruct the object
		Ack decoded = Ack::deserialize(payloadPtr, payloadSize);

		// Verify Integrity
		assert(decoded.offset == ackPkt.offset);

		std::cout << "Ack Deserialization Verified (Offset: 0x" << std::hex << decoded.offset << ").\n\n";
	}

	std::cout << "=== Test 2: FileInfo Packet ===\n";
	{
		FileInfo infoPkt;
		infoPkt.fileSize = 1024; // 0x400
		infoPkt.fileName = "test.txt";

		// 1. SERIALIZE
		std::vector<uint8_t> frame = buildFrame(infoPkt);

		printHex(frame);

		// 2. VERIFY WIRE FORMAT
		assert(frame.size() == 30);
		assert(frame[7] == 0x14); // Len

		uint16_t typeInFrame = (static_cast<uint16_t>(frame[8]) << 8) | frame[9];
		assert(typeInFrame == static_cast<uint16_t>(PacketType::FileInfo));

		assert(frame[22] == 't'); // Filename start

		std::cout << "FileInfo Wire Format Verified.\n";

		// 3. ROUND-TRIP DESERIALIZATION
		constexpr size_t HEADER_SIZE = 10;
		const uint8_t* payloadPtr = frame.data() + HEADER_SIZE;
		size_t payloadSize = frame.size() - HEADER_SIZE;

		FileInfo decoded = FileInfo::deserialize(payloadPtr, payloadSize);

		// Verify Integrity
		assert(decoded.fileSize == infoPkt.fileSize);
		assert(decoded.fileName == infoPkt.fileName);

		std::cout << "FileInfo Deserialization Verified.\n";
		std::cout << "  File Size: " << std::dec << decoded.fileSize << "\n";
		std::cout << "  File Name: " << decoded.fileName << "\n";
	}

	return 0;
}