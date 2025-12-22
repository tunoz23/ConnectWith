#pragma once

#include "../protocol/packet/packet.h"

namespace cw::network {

// Interface for sending packets over a connection.
// This abstracts the transport layer, allowing file transfer logic
// to be decoupled from the specific Connection implementation.
class ISender {
public:
  virtual ~ISender() = default;

  // Send a handshake packet
  virtual void send(const packet::Handshake &pkt) = 0;

  // Send a file info packet
  virtual void send(const packet::FileInfo &pkt) = 0;

  // Send a file chunk packet
  virtual void send(const packet::FileChunk &pkt) = 0;

  // Send a file done packet
  virtual void send(const packet::FileDone &pkt) = 0;

  // Send an ack packet
  virtual void send(const packet::Ack &pkt) = 0;

  // Send an error packet
  virtual void send(const packet::Error &pkt) = 0;

  // Check if send queue is congested (for backpressure)
  [[nodiscard]] virtual bool isCongested() const = 0;
};

} // namespace cw::network
