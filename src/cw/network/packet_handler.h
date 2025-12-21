#pragma once

#include "../frame.h"

namespace cw::network {

// Interface for handling parsed packets from a Connection.
// Implement this to define application-specific packet handling.
class IPacketHandler {
public:
  virtual ~IPacketHandler() = default;

  // Called when a complete packet has been received and parsed.
  virtual void onPacket(const packet::ParsedFrame &frame) = 0;

  // Called when the connection is closed (either normally or due to error).
  virtual void onDisconnect() = 0;
};

} // namespace cw::network
