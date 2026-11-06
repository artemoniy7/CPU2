#include "network/NetworkTypes.h"

#include <sstream>

namespace cpu16::net {

IPv4Address IPv4Address::fromOctets(std::uint8_t a, std::uint8_t b,
                                    std::uint8_t c, std::uint8_t d) noexcept {
    return { (static_cast<std::uint32_t>(a) << 24U) |
             (static_cast<std::uint32_t>(b) << 16U) |
             (static_cast<std::uint32_t>(c) << 8U) |
              static_cast<std::uint32_t>(d) };
}

IPv4Address IPv4Address::broadcast() noexcept {
    return {0xFFFFFFFFU};
}

std::string IPv4Address::toString() const {
    std::ostringstream out;
    out << ((value >> 24U) & 0xFFU) << '.'
        << ((value >> 16U) & 0xFFU) << '.'
        << ((value >> 8U) & 0xFFU) << '.'
        << (value & 0xFFU);
    return out.str();
}

bool NetworkPacket::isValid() const noexcept {
    return source.value != 0 && destination.value != 0 &&
           payload.size() <= MaxPayloadSize;
}

} // namespace cpu16::net
