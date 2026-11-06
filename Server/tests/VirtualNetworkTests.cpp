#include "network/VirtualNetwork.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace cpu16::net;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        auto hub = std::make_shared<VirtualNetworkHub>();
        VirtualNetworkAdapter server(hub, IPv4Address::fromOctets(10, 16, 0, 1));
        VirtualNetworkAdapter client(hub, IPv4Address::fromOctets(10, 16, 0, 2));
        VirtualNetworkAdapter peer(hub, IPv4Address::fromOctets(10, 16, 0, 3));

        const std::vector<std::uint8_t> payload{0x43, 0x50, 0x55, 0x31, 0x36};
        require(client.send(server.address(), IPProtocol::UDP, payload), "unicast send failed");

        NetworkPacket received;
        require(server.receive(received, std::chrono::milliseconds(10)), "unicast receive timed out");
        require(received.source == client.address(), "wrong unicast source");
        require(received.destination == server.address(), "wrong unicast destination");
        require(received.protocol == IPProtocol::UDP, "wrong unicast protocol");
        require(received.payload == payload, "unicast payload changed");
        require(!peer.receive(received, std::chrono::milliseconds(1)), "unicast leaked to another endpoint");

        require(server.send(IPv4Address::broadcast(), IPProtocol::Raw, payload), "broadcast send failed");
        require(client.receive(received, std::chrono::milliseconds(10)), "client missed broadcast");
        require(peer.receive(received, std::chrono::milliseconds(10)), "peer missed broadcast");
        require(received.destination == IPv4Address::broadcast(), "broadcast destination changed");

        require(!client.send(IPv4Address::fromOctets(10, 16, 0, 99), IPProtocol::UDP, payload),
                "send to unknown endpoint should fail");
        require(hub->endpointCount() == 3, "endpoint count is incorrect");

        std::cout << "Virtual network tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Virtual network test failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
