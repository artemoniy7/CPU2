#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cpu16::vm {

// Guest RAM. All accesses are bounds-checked; out-of-range addresses never
// fall through to host memory or host files.
class Memory {
public:
    explicit Memory(std::size_t sizeBytes);

    std::size_t size() const noexcept;
    bool read8(std::uint64_t address, std::uint8_t& value) const noexcept;
    bool write8(std::uint64_t address, std::uint8_t value) noexcept;
    bool read16(std::uint64_t address, std::uint16_t& value) const noexcept;
    bool write16(std::uint64_t address, std::uint16_t value) noexcept;
    bool load(std::uint64_t address, const std::vector<std::uint8_t>& bytes) noexcept;
    void clear() noexcept;

private:
    std::vector<std::uint8_t> bytes_;
};

} // namespace cpu16::vm
