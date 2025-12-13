#pragma once

#include <fstream>
#include <filesystem>
#include <vector>
#include <iostream>
#include <thread> // Required for sleep
#include <chrono> // Required for milliseconds

#include "cw/network/Connection.h"
#include "cw/protocol/packet/packet.h"

namespace cw {

	namespace fs = std::filesystem;

	inline void sendFile(std::shared_ptr<cw::network::Connection> conn, const std::string& filePath) {

		// 1. VALIDATE FILE
		fs::path path(filePath);
		if (!fs::exists(path)) {
			std::cerr << "File not found: " << filePath << "\n";
			return;
		}

		uint64_t fileSize = fs::file_size(path);
		std::string fileName = path.filename().string();

		std::cout << "[Client] Sending " << fileName << " (" << fileSize << " bytes)...\n";

		// 2. SEND HEADER (FileInfo)
		cw::packet::FileInfo infoPkt;
		infoPkt.fileName = fileName;
		infoPkt.fileSize = fileSize;
		conn->send(infoPkt);

		// 3. THE SLICER LOOP
		std::ifstream file(filePath, std::ios::binary);
		constexpr size_t CHUNK_SIZE = 4096; // 4KB

		uint64_t offset = 0;

		while (file) {
			// --- BACKPRESSURE CHECK ---
			// If the connection has too much pending data (e.g. > 1MB), 
			// we sleep to let the network catch up. 
			// This prevents RAM from spiking to 4GB on large file transfers.
			while (conn->isCongested()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			// --- OPTIMIZED READ ---
			// Prepare the packet
			cw::packet::FileChunk chunkPkt;
			chunkPkt.offset = offset;

			// Allocate memory directly in the packet vector to avoid an extra copy
			chunkPkt.data.resize(CHUNK_SIZE);

			// Read directly into the packet's memory
			file.read(reinterpret_cast<char*>(chunkPkt.data.data()), CHUNK_SIZE);
			size_t bytesRead = file.gcount();

			if (bytesRead == 0) break;

			// If we read less than 4KB (EOF), shrink the vector
			if (bytesRead < CHUNK_SIZE) {
				chunkPkt.data.resize(bytesRead);
			}

			// Send
			conn->send(chunkPkt);

			// Advance
			offset += bytesRead;
		}

		// 4. SEND FOOTER (FileDone)
		cw::packet::FileDone donePkt;
		donePkt.fileSize = fileSize;
		conn->send(donePkt);

		std::cout << "[Client] Upload Complete. Sent " << offset << " bytes.\n";
	}
}