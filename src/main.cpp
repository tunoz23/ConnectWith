#include <iostream>
#include <vector>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <asio.hpp>




#include "cw/endian.h"
#include "packet/packet.h"
#include "cw/Frame.h"


// -- - VISUALIZATION HELPER-- -
void printHex(const std::string & label, const std::vector<uint8_t>&data) {
	std::cout << label << " [" << data.size() << " bytes]: ";
	std::cout << std::hex << std::setfill('0');
	for (auto b : data) {
		std::cout << std::setw(2) << static_cast<int>(b) << " ";
	}
	std::cout << std::dec << "\n";
}

// --- GENERIC TEST RUNNER ---
// This template handles the "Build -> Parse -> Deserialize -> Verify" pipeline
// to avoid code repetition.
template<typename PacketT>
void run_round_trip_test(const PacketT& original, const std::string& testName) {
	std::cout << "[TEST] " << testName << "... ";

	// 1. Serialize & Frame (The Sender)
	std::vector<uint8_t> frame = cw::packet::buildFrame(original);

	// 2. Parse Frame (The Receiver - Zero Copy Layer)
	// This verifies the Frame Header (Length + Type) is correct
	auto parsedView = cw::packet::parseFrame(frame);

	// Verify Routing Info matches the Packet Type
	assert(parsedView.type == PacketT::type);

	// Verify Zero-Copy Logic:
	// The payload pointer MUST be inside the 'frame' vector.
	const uint8_t* frameStart = frame.data();
	const uint8_t* payloadPtr = parsedView.payload_view;
	std::ptrdiff_t offset = payloadPtr - frameStart;

	// Header is always 10 bytes (8 Len + 2 Type)
	if (offset != 10) {
		std::cerr << "FAILED: Zero Copy offset mismatch. Expected 10, got " << offset << "\n";
		exit(1);
	}

	// 3. Deserialize (The Application Layer)
	// Construct a new object from the raw view
	PacketT reconstructed = PacketT::deserialize(parsedView.payload_view, parsedView.size);

	// 4. Verify Integrity (Member-by-Member check)
	// We unfortunately cannot simply do "original == reconstructed" without overloading operator==
	// so we assume the caller has set specific recognizable values to check manually,
	// OR we serialize the reconstructed one and compare bytes.

	std::vector<uint8_t> originalBytes;
	std::vector<uint8_t> reconstructedBytes;

	// We manually invoke serialization on both to compare byte-for-byte equality
	// This is the ultimate truth test.
	original.serialize(originalBytes);
	reconstructed.serialize(reconstructedBytes);

	if (originalBytes != reconstructedBytes) {
		std::cerr << "FAILED: Data corruption during round trip.\n";
		printHex("Original Payload     ", originalBytes);
		printHex("Reconstructed Payload", reconstructedBytes);
		exit(1);
	}

	std::cout << "PASSED.\n";
}

// --- SPECIFIC TESTS ---

void Test_Ack() {
	cw::packet::Ack pkt;
	pkt.offset = 0xDEADBEEFCAFEBABE; // 64-bit test pattern

	run_round_trip_test(pkt, "Ack Packet Round Trip");
}

void Test_FileInfo() {
	cw::packet::FileInfo pkt;
	pkt.fileSize = 1024 * 1024 * 500; // 500 MB
	pkt.fileName = "super_secret_plans.pdf";

	run_round_trip_test(pkt, "FileInfo Packet Round Trip");
}

void Test_FileChunk() {
	cw::packet::FileChunk pkt;
	pkt.offset = 12345;
	// Fill with pattern 0x00, 0x01, 0x02...
	for (int i = 0; i < 256; ++i) pkt.data.push_back(static_cast<uint8_t>(i));

	run_round_trip_test(pkt, "FileChunk (256 bytes) Round Trip");
}

void Test_Error() {
	cw::packet::Error pkt;
	pkt.code = 404;
	pkt.message = "File not found on server";

	run_round_trip_test(pkt, "Error Packet Round Trip");
}

// --- FRAMING ROBUSTNESS TESTS ---

void Test_Fragmentation() {
	std::cout << "[TEST] Fragmentation Robustness... ";

	cw::packet::Ack pkt;
	pkt.offset = 100;
	std::vector<uint8_t> frame = cw::packet::buildFrame(pkt);

	// Case 1: Header Incomplete
	// We slice the frame to be only 5 bytes long (Header needs 10)
	std::vector<uint8_t> partialHeader(frame.begin(), frame.begin() + 5);
	try {
		cw::packet::parseFrame(partialHeader);
		std::cerr << "FAILED: Did not detect incomplete header.\n";
		exit(1);
	}
	catch (const std::runtime_error& e) {
		// Pass
	}

	// Case 2: Body Incomplete
	// We slice the frame to include the Header but cut off 1 byte of payload
	std::vector<uint8_t> partialBody(frame.begin(), frame.end() - 1);
	try {
		cw::packet::parseFrame(partialBody);
		std::cerr << "FAILED: Did not detect incomplete body.\n";
		exit(1);
	}
	catch (const std::runtime_error& e) {
		// Pass
	}

	std::cout << "PASSED.\n";
}

void Test_GarbageData() {
	std::cout << "[TEST] Garbage Length Protection... ";

	// Simulate a malicious packet saying it has 5 Petabytes of data
	// [Length: 0xFFFFFFFFFFFFFFFF] [Type: Ack]
	std::vector<uint8_t> maliciousFrame = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Length (Huge)
		0x00, 0x01 // Type (Ack)
	};

	try {
		cw::packet::parseFrame(maliciousFrame);
		std::cerr << "FAILED: parseFrame tried to accept a frame larger than buffer.\n";
		exit(1);
	}
	catch (const std::runtime_error& e) {
		// Should throw "Incomplete Frame Body" because the buffer size (10) < Header + HugeLength
		// This proves we check bounds before trusting the length.
	}

	std::cout << "PASSED.\n";
}

int main() {
	std::cout << "=== RUNNING INTEGRATION SUITE ===\n\n";

	// 1. Logic Tests (Serialization + Deserialization)
	Test_Ack();
	Test_FileInfo();
	Test_FileChunk();
	Test_Error();

	std::cout << "\n";

	// 2. Network Layer Tests (Framing, Fragmentation, Security)
	Test_Fragmentation();
	Test_GarbageData();

	std::cout << "\n=== ALL SYSTEM TESTS PASSED ===\n";
	return 0;
}