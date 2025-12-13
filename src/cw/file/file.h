#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <algorithm> // Required for std::replace

// Adjust these includes to match your project structure
#include "cw/network/Connection.h"
#include "cw/protocol/packet/packet.h"

namespace cw {
	namespace fs = std::filesystem;

	inline void sendFile(std::shared_ptr<cw::network::Connection> conn, const std::string& filePath, const std::string& remoteFileName = "") {

		// 1. VALIDATE FILE
		fs::path path(filePath);
		if (!fs::exists(path)) {
			std::cerr << "File not found: " << filePath << "\n";
			return;
		}

		uint64_t fileSize = fs::file_size(path);

		// --- LOGIC CHANGE START ---
		std::string nameToSend;
		if (!remoteFileName.empty()) {
			nameToSend = remoteFileName;
		}
		else {
			// Use generic_string() to prefer forward slashes, but we force sanitize below anyway
			nameToSend = path.filename().generic_string();
		}

		// CRITICAL FIX: Normalize path separators to forward slashes '/'
		// This ensures Windows clients can send to Linux servers correctly.
		std::replace(nameToSend.begin(), nameToSend.end(), '\\', '/');
		// --- LOGIC CHANGE END ---

		std::cout << "[Client] Sending " << nameToSend << " (" << fileSize << " bytes)...\n";

		// 2. SEND HEADER (FileInfo)
		cw::packet::FileInfo infoPkt;
		infoPkt.fileName = nameToSend;
		infoPkt.fileSize = fileSize;
		conn->send(infoPkt);

		// 3. THE SLICER LOOP
		std::ifstream file(filePath, std::ios::binary);
		constexpr size_t CHUNK_SIZE = 4096;

		uint64_t offset = 0;

		while (file) {
			// --- BACKPRESSURE CHECK ---
			while (conn->isCongested()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			// --- OPTIMIZED READ ---
			cw::packet::FileChunk chunkPkt;
			chunkPkt.offset = offset;

			// Resize creates zero-initialized elements. 
			// This is safe, though slightly slower than reserve+push_back, it's cleaner for file.read
			chunkPkt.data.resize(CHUNK_SIZE);

			file.read(reinterpret_cast<char*>(chunkPkt.data.data()), CHUNK_SIZE);
			size_t bytesRead = file.gcount();

			if (bytesRead == 0) break;

			if (bytesRead < CHUNK_SIZE) {
				chunkPkt.data.resize(bytesRead);
			}

			conn->send(chunkPkt);

			offset += bytesRead;
		}

		// 4. SEND FOOTER (FileDone)
		cw::packet::FileDone donePkt;
		donePkt.fileSize = fileSize;
		conn->send(donePkt);
		std::cout << "[Client] Upload Complete. Sent " << offset << " bytes.\n";
	}
}