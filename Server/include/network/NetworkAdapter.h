#pragma once

#include "network/NetworkTypes.h"

#include <chrono>
#include <cstdint>
#include <vector>

namespace cpu16::net {

// Guest-facing datagram interface. Implementations may use an in-process
// virtual switch or a host LAN transport; guest networking code is transport-agnostic.
class NetworkAdapter {
public:
    virtual ~NetworkAdapter() = default;

    virtual IPv4Address address() const noexcept = 0;
    virtual bool send(IPv4Address destination, IPProtocol protocol,
                      const std::vector<std::uint8_t>& payload) = 0;
    virtual bool receive(NetworkPacket& packet,
                         std::chrono::milliseconds timeout) = 0;
};

} // namespace cpu16::net
