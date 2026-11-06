#include "vm/VirtualMachine.h"

#include <stdexcept>
#include <utility>

namespace cpu16::vm {

VirtualMachine::VirtualMachine(MachineConfig config)
    : config_(std::move(config)),
      memory_(static_cast<std::size_t>(config_.ramSizeBytes)) {
    std::string error;
    if (!config_.isValid(&error)) throw std::invalid_argument(error);
}

const MachineConfig& VirtualMachine::config() const noexcept { return config_; }
Memory& VirtualMachine::memory() noexcept { return memory_; }
const Memory& VirtualMachine::memory() const noexcept { return memory_; }
DeviceBus& VirtualMachine::devices() noexcept { return devices_; }
const DeviceBus& VirtualMachine::devices() const noexcept { return devices_; }

bool VirtualMachine::installCpu(std::unique_ptr<CpuCore> cpu) {
    if (!cpu || cpu_) return false;
    cpu_ = std::move(cpu);
    return true;
}

bool VirtualMachine::hasCpu() const noexcept { return static_cast<bool>(cpu_); }

void VirtualMachine::reset() {
    memory_.clear();
    totalCycles_ = 0;
    if (cpu_) cpu_->reset();
}

RunResult VirtualMachine::runFor(std::uint64_t cycleBudget) {
    if (!cpu_) return {RunState::CpuNotConfigured, 0, "No CPU core is installed"};
    if (cycleBudget == 0) return {RunState::BudgetExhausted, 0, nullptr};

    std::uint64_t executed = 0;
    while (executed < cycleBudget) {
        const CpuStepResult step = cpu_->step(memory_, devices_);
        if (step.state != CpuStepState::Continue) {
            ++executed;
            totalCycles_ += executed;
            switch (step.state) {
                case CpuStepState::Halted: return {RunState::Halted, executed, step.message};
                case CpuStepState::Waiting: return {RunState::Waiting, executed, step.message};
                case CpuStepState::Fault: return {RunState::Fault, executed, step.message};
                case CpuStepState::Continue: break;
            }
        }
        ++executed;
    }

    totalCycles_ += executed;
    return {RunState::BudgetExhausted, executed, nullptr};
}

std::uint64_t VirtualMachine::totalCycles() const noexcept { return totalCycles_; }

} // namespace cpu16::vm
