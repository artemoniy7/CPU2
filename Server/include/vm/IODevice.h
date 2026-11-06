#pragma once

#include <cstdint>

namespace cpu16::vm {

// A guest-visible I/O device. Offsets are relative to the device's port range.
class IODevice {
public:
    virtual ~IODevice() = default;
    virtual bool read(std::uint16_t offset, std::uint16_t& value) = 0;
    virtual bool write(std::uint16_t offset, std::uint16_t value) = 0;
};

} // namespace cpu16::vm
