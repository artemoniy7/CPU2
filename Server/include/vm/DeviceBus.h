#pragma once

#include "vm/IODevice.h"

#include <cstdint>
#include <map>
#include <memory>

namespace cpu16::vm {

// Routes guest IN/OUT operations to attached virtual devices.
class DeviceBus {
public:
    bool attach(std::uint16_t basePort, std::uint32_t portCount,
                std::shared_ptr<IODevice> device);
    void detach(std::uint16_t basePort, std::uint32_t portCount) noexcept;

    bool read(std::uint16_t port, std::uint16_t& value);
    bool write(std::uint16_t port, std::uint16_t value);
    std::size_t attachedPortCount() const noexcept;

private:
    struct Binding {
        std::uint16_t basePort;
        std::shared_ptr<IODevice> device;
    };

    std::map<std::uint16_t, Binding> ports_;
};

} // namespace cpu16::vm
