#pragma once

#include <asio.hpp>
#include <vector>
#include <memory>
#include <deque>
#include <iostream>
#include <array>

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


		template<typename PacketT>
		void send(const PacketT& packet)
		{
			// 1. Serialize immediately on the calling thread
			auto frame = cw::packet::buildFrame(packet);

			// 2. Post to the IO context
			// This ensures doWrite always runs on the correct thread.
			auto self = shared_from_this();
			asio::post(m_socket.get_executor(),
				[this, self, msg = std::move(frame)]()
				{
					doWrite(std::move(msg));
				});
		}
		
		void start()
		{
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
					else std::cerr << "[Connection] Error: " << ec.message() << "\n";

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
					// We need this to know how much to delete later.
					// Header is always 10 bytes (8 Len + 2 Type)
					constexpr size_t HEADER_SIZE = 10;
					size_t totalFrameSize = HEADER_SIZE + view.size;

					// C. Handle the Packet
					dispatchPacket(view);

					// D. Consume Data (The Shift)
					// Remove the processed bytes from the front of the vector.
					m_incomingBuffer.erase(m_incomingBuffer.begin(),
						m_incomingBuffer.begin() + totalFrameSize);

					// E. Exit if empty
					if (m_incomingBuffer.empty()) break;
				}
				catch (const std::runtime_error&)
				{
					// FRAGMENTATION DETECTED
					// parseFrame threw an exception because we don't have the full packet yet.
					// This is NORMAL behavior in TCP.
					// We break the loop and wait for the next doRead() to append missing data.
					break;
				}
			}
		}

		void doWrite(std::vector<uint8_t> frame)
		{
			m_writeQueue.push_back(std::move(frame));

			// If we were already writing, the existing callback will pick this up.
			// If queue size was 0, it's now 1, so we must kickstart the chain.
			if (m_writeQueue.size() > 1)
				return;

			writeQueueFront();
		}

		// The actual Async Write call
		void writeQueueFront()
		{
			auto self = shared_from_this();

			asio::async_write(m_socket,
				asio::buffer(m_writeQueue.front()),
				[this, self](std::error_code ec, std::size_t /*length*/)
				{
					if (!ec)
					{
						// 1. Remove the sent packet
						m_writeQueue.pop_front();

						// 2. Check if more items appeared while we were busy
						if (!m_writeQueue.empty())
						{
							writeQueueFront(); // Continue the chain
						}
					}
					else
					{
						std::cerr << "[Connection] Write Error: " << ec.message() << "\n";
						m_socket.close();
					}
				});
		}
		// 4. THE ROUTER
		// Takes a view, deserializes it to a Struct, and handles it.
		void dispatchPacket(const cw::packet::ParsedFrame& view)
		{
			using namespace cw::packet;

			switch (view.type)
			{
			case PacketType::Ack:
			{
				// Deserialize bytes -> Struct
				auto pkt = Ack::deserialize(view.payload_view, view.size);
				std::cout << "[Recv] Ack Packet (Offset: " << pkt.offset << ")\n";
				break;
			}
			case PacketType::FileInfo:
			{
				auto pkt = FileInfo::deserialize(view.payload_view, view.size);
				std::cout << "[Recv] FileInfo: " << pkt.fileName << " (" << pkt.fileSize << " bytes)\n";
				break;
			}
			case PacketType::Error:
			{
				auto pkt = Error::deserialize(view.payload_view, view.size);
				std::cerr << "[Recv] Error: " << pkt.message << "\n";
				break;
			}
			case PacketType::FileChunk:
			{
				auto pkt = FileChunk::deserialize(view.payload_view, view.size);
				std::cout << "[Recv] FileChunk: " << pkt.offset << "\n";
				break;
			}
			case PacketType::FileDone:
			{
				auto pkt = FileDone::deserialize(view.payload_view, view.size);
				std::cout << "[Recv] FileDone: " << pkt.fileSize << "\n";
				break;
			}
			// Add other cases (FileChunk, etc.) here
			//TO DO:
			
			default:
				std::cout << "[Recv] Unknown Packet Type: " << (int)view.type << "\n";
			}
		}

		Connection(asio::io_context& io) :
			m_socket(io)
		{

			m_incomingBuffer.reserve(8192);
		}


	private:
		asio::ip::tcp::socket m_socket;
		//its ok for L1, L2 cache
		std::array<uint8_t, 8192> m_tempBuffer;

		std::vector<uint8_t> m_incomingBuffer;

		std::deque<std::vector<uint8_t>> m_writeQueue;
	};
}