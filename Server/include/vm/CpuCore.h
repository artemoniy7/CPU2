#pragma once

#include <cstdint>

namespace cpu16::vm {

class Memory;
class DeviceBus;

enum class CpuStepState {
    Continue,
    Halted,
    Waiting,
    Fault
};

struct CpuStepResult {
    CpuStepState state{CpuStepState::Continue};
    const char* message{nullptr};
};

// Contract for the real CPU2 execution core. The VM owns scheduling and
// hardware; the CPU core only executes one guest instruction against them.
class CpuCore {
public:
    virtual ~CpuCore() = default;
    virtual void reset() = 0;
    virtual CpuStepResult step(Memory& memory, DeviceBus& devices) = 0;
};

} // namespace cpu16::vm
