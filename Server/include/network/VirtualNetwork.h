#pragma once

#include "network/NetworkAdapter.h"

#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace cpu16::net {

class VirtualNetworkHub {
public:
    bool attach(IPv4Address address);
    void detach(IPv4Address address) noexcept;
    bool transmit(const NetworkPacket& packet);
    bool receive(IPv4Address address, NetworkPacket& packet,
                 std::chrono::milliseconds timeout);
    std::size_t endpointCount() const noexcept;

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::unordered_map<std::uint32_t, std::deque<NetworkPacket>> queues_;
};

class VirtualNetworkAdapter final : public NetworkAdapter {
public:
    VirtualNetworkAdapter(std::shared_ptr<VirtualNetworkHub> hub, IPv4Address address);
    ~VirtualNetworkAdapter() override;

    VirtualNetworkAdapter(const VirtualNetworkAdapter&) = delete;
    VirtualNetworkAdapter& operator=(const VirtualNetworkAdapter&) = delete;

    IPv4Address address() const noexcept override;
    bool send(IPv4Address destination, IPProtocol protocol,
              const std::vector<std::uint8_t>& payload) override;
    bool receive(NetworkPacket& packet,
                 std::chrono::milliseconds timeout) override;

private:
    std::shared_ptr<VirtualNetworkHub> hub_;
    IPv4Address address_;
    bool attached_{false};
};

} // namespace cpu16::net
