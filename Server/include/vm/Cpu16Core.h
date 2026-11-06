#pragma once

#include "vm/CpuCore.h"

#include <array>
#include <cstdint>
#include <vector>

namespace cpu16::vm {

class Cpu16Core final : public CpuCore {
public:
    Cpu16Core();

    void reset() override;
    CpuStepResult step(Memory& memory, DeviceBus& devices) override;

    bool loadBytecode(Memory& memory, const std::vector<std::uint8_t>& code,
                      std::uint16_t entryPoint = 0,
                      std::uint16_t loadAddress = 0x0100);
    std::uint16_t registerValue(std::uint16_t registerId) const noexcept;
    std::uint16_t instructionPointer() const noexcept;
    bool isHalted() const noexcept;

    void setInterruptVector(Memory& memory, std::uint8_t vector,
                            std::uint16_t offset, std::uint16_t segment);
    void raiseKeyboardIRQ() noexcept;
    void raiseTimerIRQ() noexcept;

private:
    enum RegisterId : std::uint16_t {
        AX, BX, CX, DX, SP, BP, SI, DI, CS, DS, SS, ES
    };

    std::array<std::uint16_t, 12> registers_{};
    std::uint16_t ip_{0};
    bool zeroFlag_{false};
    bool carryFlag_{false};
    bool signFlag_{false};
    bool overflowFlag_{false};
    bool directionFlag_{false};
    bool interruptFlag_{true};
    bool keyboardIrqPending_{false};
    bool timerIrqPending_{false};
    bool bytecodeActive_{false};
    bool halted_{false};
    std::vector<std::uint8_t> bytecode_;
    std::array<std::uint8_t, 64 * 1024> biosRom_{};

    std::uint32_t physicalAddress(std::uint16_t segment, std::uint16_t offset) const noexcept;
    std::uint16_t flagsWord() const noexcept;
    void restoreFlags(std::uint16_t flags) noexcept;
    void pushWord(Memory& memory, std::uint16_t value);
    std::uint16_t popWord(Memory& memory);
    std::uint8_t readByte(const Memory& memory, std::uint32_t address) const noexcept;
    std::uint16_t readWord(const Memory& memory, std::uint32_t address) const noexcept;
    void writeWord(Memory& memory, std::uint32_t address, std::uint16_t value) noexcept;
    void setFlagsForResult(std::uint16_t result, std::uint16_t lhs,
                           std::uint16_t rhs, bool isAdd) noexcept;
    bool dispatchPendingIRQ(Memory& memory);
};

} // namespace cpu16::vm
