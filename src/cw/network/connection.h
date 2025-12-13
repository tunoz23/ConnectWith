#pragma once

#include <asio.hpp>
#include <vector>
#include <memory>
#include <deque>
#include <atomic>
#include <iostream>
#include <array>
#include <fstream> // Added for file I/O

// Project Headers
#include "../Frame.h"
#include "../protocol/packet/packet.h"

namespace cw::network {

	using asio::ip::tcp;

	class Connection : public std::enable_shared_from_this<Connection>
	{

	public:
		static std::shared_ptr<Connection> create(asio::io_context& io)
		{
			return std::shared_ptr<Connection>(new Connection(io));
		}

		tcp::socket& socket() { return m_socket; }

		bool isCongested() const
		{
			// If we have more than 1MB pending in RAM, tell the file reader to wait.
			return m_queueSize > 1024 * 1024;
		}
		template<typename PacketT>
		void send(const PacketT& packet)
		{
			auto frame = cw::packet::buildFrame(packet);

			auto self = shared_from_this();
			asio::post(m_socket.get_executor(),
				[this, self, msg = std::move(frame)]()
				{
					doWrite(std::move(msg));
				});
		}

		void start()
		{
			std::cout << "[Connection] Client Handshake Complete. Ready.\n";
			doRead();
		}

	private:
		void doRead()
		{
			auto self = shared_from_this();

			m_socket.async_read_some(asio::buffer(m_tempBuffer),
				[this, self](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						m_incomingBuffer.insert(m_incomingBuffer.end(),
							m_tempBuffer.begin(),
							m_tempBuffer.begin() + length);

						processBuffer();

						doRead();
					}
					else {
						// Socket closed or error
						std::cout << "[Connection] Disconnected: " << ec.message() << "\n";
					}

				});
		}

		void processBuffer()
		{
			using namespace cw::packet;

			while (true)
			{
				try
				{
					// A. Attempt Zero-Copy Parse
					// If buffer is too small (header or body), this THROWS.
					ParsedFrame view = parseFrame(m_incomingBuffer);

					// B. Calculate Total Size (Header + Payload)
					constexpr size_t HEADER_SIZE = 10;
					size_t totalFrameSize = HEADER_SIZE + view.size;

					// C. Handle the Packet
					dispatchPacket(view);

					// D. Consume Data (The Shift)
					m_incomingBuffer.erase(m_incomingBuffer.begin(),
						m_incomingBuffer.begin() + totalFrameSize);

					// E. Exit if empty
					if (m_incomingBuffer.empty()) break;
				}
				catch (const std::runtime_error&)
				{
					// FRAGMENTATION DETECTED
					break;
				}
			}
		}

		void doWrite(std::vector<uint8_t> frame)
		{
			m_queueSize += frame.size();

			m_writeQueue.push_back(std::move(frame));

			if (m_writeQueue.size() > 1) return;
			writeQueueFront();

		}

		// The actual Async Write call
		void writeQueueFront()
		{
			auto self = shared_from_this();
			asio::async_write(m_socket,
				asio::buffer(m_writeQueue.front()),
				[this, self](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						// TRACKING: Subtract size (packet sent)
						m_queueSize -= m_writeQueue.front().size();

						m_writeQueue.pop_front();

						if (!m_writeQueue.empty()) writeQueueFront();
					}

					else
					{
						std::cerr << "[Connection] Write Error: " << ec.message() << "\n";
						m_socket.close();
					}
				});
		}

		// 4. THE ROUTER (Business Logic)
		void dispatchPacket(const cw::packet::ParsedFrame& view)
		{
			using namespace cw::packet;

			switch (view.type)
			{
			case PacketType::Ack:
			{
				auto pkt = Ack::deserialize(view.payload_view, view.size);
				std::cout << "[Recv] Ack (Offset: " << pkt.offset << ")\n";
				break;
			}
			case PacketType::FileInfo:
			{
				auto pkt = FileInfo::deserialize(view.payload_view, view.size);
				std::cout << "[Recv] Starting Download: " << pkt.fileName << " (" << pkt.fileSize << " bytes)\n";

				// 1. Prepare File
				// We prefix with "downloads_" to prevent overwriting critical system files
				std::string targetPath = "downloads_" + pkt.fileName;

				m_outFile.open(targetPath, std::ios::binary);
				if (!m_outFile.is_open()) {
					std::cerr << "[Error] Could not open file for writing: " << targetPath << "\n";
					return;
				}

				m_expectedSize = pkt.fileSize;
				m_receivedBytes = 0;
				break;
			}
			case PacketType::FileChunk:
			{
				// 2. Write Chunk
				if (!m_outFile.is_open()) return;

				auto pkt = FileChunk::deserialize(view.payload_view, view.size);

				// Using seekp handles out-of-order packets if we add parallel sending later
				m_outFile.seekp(pkt.offset);
				m_outFile.write(reinterpret_cast<const char*>(pkt.data.data()), pkt.data.size());

				m_receivedBytes += pkt.data.size();
				break;
			}
			case PacketType::FileDone:
			{
				// 3. Finish
				auto pkt = FileDone::deserialize(view.payload_view, view.size);
				if (m_outFile.is_open()) {
					m_outFile.close();
					std::cout << "[Recv] File Download Complete.\n";
				}

				if (m_receivedBytes == pkt.fileSize) {
					std::cout << "[Check] Integrity Validated (" << m_receivedBytes << " bytes).\n";

					// Send Ack back to client
					Ack ack;
					ack.offset = m_receivedBytes;
					send(ack);
				}
				else {
					std::cerr << "[Check] CORRUPTION DETECTED! Expected " << pkt.fileSize << " but got " << m_receivedBytes << "\n";
				}
				break;
			}
			case PacketType::Error:
			{
				auto pkt = Error::deserialize(view.payload_view, view.size);
				std::cerr << "[Recv] Error: " << pkt.message << "\n";
				break;
			}
			default:
				std::cout << "[Recv] Unknown Packet Type: " << (int)view.type << "\n";
			}
		}

		Connection(asio::io_context& io) :
			m_socket(io)
		{
			// std::array is POD, no need to init explicitly but {} is fine
			m_tempBuffer = {};
			m_incomingBuffer.reserve(8192);
		}


	private:
		asio::ip::tcp::socket m_socket;

		std::array<uint8_t, 8192> m_tempBuffer;
		std::vector<uint8_t> m_incomingBuffer;
		std::deque<std::vector<uint8_t>> m_writeQueue;
		std::atomic<size_t> m_queueSize = 0;
		// --- File Transfer State ---
		std::ofstream m_outFile;
		std::uint64_t m_expectedSize = 0;
		std::uint64_t m_receivedBytes = 0;
	};
}