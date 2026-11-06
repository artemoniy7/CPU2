#pragma once

#include "vm/CpuCore.h"
#include "vm/DeviceBus.h"
#include "vm/MachineConfig.h"
#include "vm/Memory.h"

#include <cstdint>
#include <memory>

namespace cpu16::vm {

enum class RunState {
    BudgetExhausted,
    Halted,
    Waiting,
    Fault,
    CpuNotConfigured
};

struct RunResult {
    RunState state{RunState::CpuNotConfigured};
    std::uint64_t cyclesExecuted{0};
    const char* message{nullptr};
};

// One isolated emulated computer: its own RAM, I/O bus and CPU slot.
class VirtualMachine {
public:
    explicit VirtualMachine(MachineConfig config);

    const MachineConfig& config() const noexcept;
    Memory& memory() noexcept;
    const Memory& memory() const noexcept;
    DeviceBus& devices() noexcept;
    const DeviceBus& devices() const noexcept;

    bool installCpu(std::unique_ptr<CpuCore> cpu);
    bool hasCpu() const noexcept;
    void reset();
    RunResult runFor(std::uint64_t cycleBudget);

    std::uint64_t totalCycles() const noexcept;

private:
    MachineConfig config_;
    Memory memory_;
    DeviceBus devices_;
    std::unique_ptr<CpuCore> cpu_;
    std::uint64_t totalCycles_{0};
};

} // namespace cpu16::vm
