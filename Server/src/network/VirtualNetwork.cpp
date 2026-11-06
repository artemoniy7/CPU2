#include "network/VirtualNetwork.h"

#include <stdexcept>
#include <utility>

namespace cpu16::net {

bool VirtualNetworkHub::attach(IPv4Address address) {
    if (address.value == 0 || address == IPv4Address::broadcast()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return queues_.emplace(address.value, std::deque<NetworkPacket>{}).second;
}

void VirtualNetworkHub::detach(IPv4Address address) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    queues_.erase(address.value);
}

bool VirtualNetworkHub::transmit(const NetworkPacket& packet) {
    if (!packet.isValid()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    if (queues_.find(packet.source.value) == queues_.end()) return false;

    if (packet.destination == IPv4Address::broadcast()) {
        for (auto& endpoint : queues_) {
            if (endpoint.first != packet.source.value)
                endpoint.second.push_back(packet);
        }
    } else {
        const auto target = queues_.find(packet.destination.value);
        if (target == queues_.end()) return false;
        target->second.push_back(packet);
    }
    condition_.notify_all();
    return true;
}

bool VirtualNetworkHub::receive(IPv4Address address, NetworkPacket& packet,
                                std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    const auto ready = [this, address] {
        const auto it = queues_.find(address.value);
        return it == queues_.end() || !it->second.empty();
    };
    if (!condition_.wait_for(lock, timeout, ready)) return false;

    const auto it = queues_.find(address.value);
    if (it == queues_.end() || it->second.empty()) return false;
    packet = std::move(it->second.front());
    it->second.pop_front();
    return true;
}

std::size_t VirtualNetworkHub::endpointCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return queues_.size();
}

VirtualNetworkAdapter::VirtualNetworkAdapter(std::shared_ptr<VirtualNetworkHub> hub,
                                             IPv4Address address)
    : hub_(std::move(hub)), address_(address) {
    if (!hub_) throw std::invalid_argument("VirtualNetworkAdapter requires a network hub");
    attached_ = hub_->attach(address_);
    if (!attached_) throw std::invalid_argument("Virtual network address is invalid or already in use");
}

VirtualNetworkAdapter::~VirtualNetworkAdapter() {
    if (attached_) hub_->detach(address_);
}

IPv4Address VirtualNetworkAdapter::address() const noexcept {
    return address_;
}

bool VirtualNetworkAdapter::send(IPv4Address destination, IPProtocol protocol,
                                 const std::vector<std::uint8_t>& payload) {
    NetworkPacket packet{address_, destination, protocol, payload};
    return hub_->transmit(packet);
}

bool VirtualNetworkAdapter::receive(NetworkPacket& packet,
                                    std::chrono::milliseconds timeout) {
    return hub_->receive(address_, packet, timeout);
}

} // namespace cpu16::net
