#include "vm/MachineConfig.h"

namespace cpu16::vm {

bool MachineConfig::isValid(std::string* error) const {
    const auto fail = [error](const char* message) {
        if (error) *error = message;
        return false;
    };

    if (name.empty()) return fail("Machine name must not be empty");
    if (cpuModel.empty()) return fail("CPU model must not be empty");
    if (ramSizeBytes < 64ULL * 1024ULL)
        return fail("RAM must be at least 64 KiB");
    if (ramSizeBytes > 1024ULL * 1024ULL * 1024ULL)
        return fail("RAM must not exceed 1 GiB in this VM version");
    if (cyclesPerSlice == 0 || cyclesPerSlice > 10000000ULL)
        return fail("Cycles per slice must be between 1 and 10000000");

    if (error) error->clear();
    return true;
}

} // namespace cpu16::vm
