#include "vm/DeviceBus.h"

#include <cstddef>

namespace cpu16::vm {

bool DeviceBus::attach(std::uint16_t basePort, std::uint32_t portCount,
                       std::shared_ptr<IODevice> device) {
    if (!device || portCount == 0) return false;
    const std::uint32_t end = static_cast<std::uint32_t>(basePort) + portCount;
    if (end > 65536U) return false;

    for (std::uint32_t port = basePort; port < end; ++port) {
        if (ports_.find(static_cast<std::uint16_t>(port)) != ports_.end())
            return false;
    }

    for (std::uint32_t port = basePort; port < end; ++port) {
        ports_.emplace(static_cast<std::uint16_t>(port),
                       Binding{basePort, device});
    }
    return true;
}

void DeviceBus::detach(std::uint16_t basePort, std::uint32_t portCount) noexcept {
    const std::uint32_t end = static_cast<std::uint32_t>(basePort) + portCount;
    if (end > 65536U) return;
    for (std::uint32_t port = basePort; port < end; ++port)
        ports_.erase(static_cast<std::uint16_t>(port));
}

bool DeviceBus::read(std::uint16_t port, std::uint16_t& value) {
    const auto it = ports_.find(port);
    if (it == ports_.end()) return false;
    return it->second.device->read(
        static_cast<std::uint16_t>(port - it->second.basePort), value);
}

bool DeviceBus::write(std::uint16_t port, std::uint16_t value) {
    const auto it = ports_.find(port);
    if (it == ports_.end()) return false;
    return it->second.device->write(
        static_cast<std::uint16_t>(port - it->second.basePort), value);
}

std::size_t DeviceBus::attachedPortCount() const noexcept {
    return ports_.size();
}

} // namespace cpu16::vm
