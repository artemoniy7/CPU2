#pragma once

#include <cstdint>
#include <string>

namespace cpu16::vm {

struct MachineConfig {
    std::string name{"CPU16 Server"};
    std::string cpuModel{"cpu16"};
    std::uint64_t ramSizeBytes{1024ULL * 1024ULL};
    std::uint64_t cyclesPerSlice{1000};

    bool isValid(std::string* error = nullptr) const;
};

} // namespace cpu16::vm
