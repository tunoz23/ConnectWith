#pragma once

#include "cw/network/i_sender.h"
#include <gmock/gmock.h>

namespace cw::test {

// Mock implementation of ISender for unit testing.
// Allows testing file transfer logic without real network I/O.
class MockSender : public network::ISender {
public:
  MOCK_METHOD(void, send, (const packet::Handshake &pkt), (override));
  MOCK_METHOD(void, send, (const packet::FileInfo &pkt), (override));
  MOCK_METHOD(void, send, (const packet::FileChunk &pkt), (override));
  MOCK_METHOD(void, send, (const packet::FileDone &pkt), (override));
  MOCK_METHOD(void, send, (const packet::Ack &pkt), (override));
  MOCK_METHOD(void, send, (const packet::Error &pkt), (override));
  MOCK_METHOD(bool, isCongested, (), (const, override));
};

} // namespace cw::test
