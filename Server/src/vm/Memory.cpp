#include "vm/Memory.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace cpu16::vm {

Memory::Memory(std::size_t sizeBytes) : bytes_(sizeBytes, 0) {
    if (sizeBytes == 0) throw std::invalid_argument("Guest RAM size must be non-zero");
}

std::size_t Memory::size() const noexcept {
    return bytes_.size();
}

bool Memory::read8(std::uint64_t address, std::uint8_t& value) const noexcept {
    if (address >= bytes_.size()) return false;
    value = bytes_[static_cast<std::size_t>(address)];
    return true;
}

bool Memory::write8(std::uint64_t address, std::uint8_t value) noexcept {
    if (address >= bytes_.size()) return false;
    bytes_[static_cast<std::size_t>(address)] = value;
    return true;
}

bool Memory::read16(std::uint64_t address, std::uint16_t& value) const noexcept {
    if (address >= bytes_.size() || bytes_.size() - static_cast<std::size_t>(address) < 2)
        return false;
    const auto pos = static_cast<std::size_t>(address);
    value = static_cast<std::uint16_t>(bytes_[pos]) |
            (static_cast<std::uint16_t>(bytes_[pos + 1]) << 8U);
    return true;
}

bool Memory::write16(std::uint64_t address, std::uint16_t value) noexcept {
    if (address >= bytes_.size() || bytes_.size() - static_cast<std::size_t>(address) < 2)
        return false;
    const auto pos = static_cast<std::size_t>(address);
    bytes_[pos] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes_[pos + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    return true;
}

bool Memory::load(std::uint64_t address, const std::vector<std::uint8_t>& bytes) noexcept {
    if (address > bytes_.size()) return false;
    const auto pos = static_cast<std::size_t>(address);
    if (bytes.size() > bytes_.size() - pos) return false;
    std::copy(bytes.begin(), bytes.end(), bytes_.begin() + pos);
    return true;
}

void Memory::clear() noexcept {
    std::fill(bytes_.begin(), bytes_.end(), 0);
}

} // namespace cpu16::vm
