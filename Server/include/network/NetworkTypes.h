#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cpu16::net {

struct IPv4Address {
    // Stored in network byte order, e.g. 0x0A100001 represents 10.16.0.1.
    std::uint32_t value{0};

    static IPv4Address fromOctets(std::uint8_t a, std::uint8_t b,
                                  std::uint8_t c, std::uint8_t d) noexcept;
    static IPv4Address broadcast() noexcept;
    std::string toString() const;

    friend bool operator==(IPv4Address lhs, IPv4Address rhs) noexcept {
        return lhs.value == rhs.value;
    }
    friend bool operator!=(IPv4Address lhs, IPv4Address rhs) noexcept {
        return !(lhs == rhs);
    }
};

enum class IPProtocol : std::uint8_t {
    ICMP = 1,
    TCP = 6,
    UDP = 17,
    Raw = 255
};

struct NetworkPacket {
    static constexpr std::size_t MaxPayloadSize = 65507;

    IPv4Address source;
    IPv4Address destination;
    IPProtocol protocol{IPProtocol::Raw};
    std::vector<std::uint8_t> payload;

    bool isValid() const noexcept;
};

} // namespace cpu16::net
