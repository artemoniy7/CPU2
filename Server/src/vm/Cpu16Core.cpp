#include "vm/Cpu16Core.h"

#include "vm/DeviceBus.h"
#include "vm/Memory.h"

#include <algorithm>
#include <string>

namespace cpu16::vm {

Cpu16Core::Cpu16Core() {
    const std::string banner = "CPU16 BIOS 1.0";
    std::copy(banner.begin(), banner.end(), biosRom_.begin());
    reset();
}

void Cpu16Core::reset() {
    registers_.fill(0);
    registers_[SP] = 0xFFFE;
    ip_ = 0;
    zeroFlag_ = carryFlag_ = signFlag_ = overflowFlag_ = directionFlag_ = false;
    interruptFlag_ = true;
    keyboardIrqPending_ = timerIrqPending_ = false;
    bytecodeActive_ = false;
    halted_ = false;
    bytecode_.clear();
}

bool Cpu16Core::loadBytecode(Memory& memory, const std::vector<std::uint8_t>& code,
                             std::uint16_t entryPoint, std::uint16_t loadAddress) {
    if (code.empty() || code.size() % 7U != 0 ||
        code.size() > 65535U || entryPoint >= code.size())
        return false;
    if (!memory.load(loadAddress, code)) return false;

    bytecode_ = code;
    ip_ = entryPoint;
    bytecodeActive_ = true;
    halted_ = false;
    return true;
}

std::uint16_t Cpu16Core::registerValue(std::uint16_t registerId) const noexcept {
    return registerId < registers_.size() ? registers_[registerId] : 0;
}

std::uint16_t Cpu16Core::instructionPointer() const noexcept { return ip_; }
bool Cpu16Core::isHalted() const noexcept { return halted_; }

void Cpu16Core::setInterruptVector(Memory& memory, std::uint8_t vector,
                                  std::uint16_t offset, std::uint16_t segment) {
    const std::uint32_t address = static_cast<std::uint32_t>(vector) * 4U;
    memory.write16(address, offset);
    memory.write16(address + 2U, segment);
}

void Cpu16Core::raiseKeyboardIRQ() noexcept { keyboardIrqPending_ = true; }
void Cpu16Core::raiseTimerIRQ() noexcept { timerIrqPending_ = true; }

std::uint32_t Cpu16Core::physicalAddress(std::uint16_t segment,
                                         std::uint16_t offset) const noexcept {
    return (static_cast<std::uint32_t>(segment) << 4U) + offset;
}

std::uint16_t Cpu16Core::flagsWord() const noexcept {
    return (carryFlag_ ? 0x0001U : 0U) | (zeroFlag_ ? 0x0040U : 0U) |
           (signFlag_ ? 0x0080U : 0U) | (directionFlag_ ? 0x0400U : 0U) |
           (overflowFlag_ ? 0x0800U : 0U);
}

void Cpu16Core::restoreFlags(std::uint16_t flags) noexcept {
    carryFlag_ = (flags & 0x0001U) != 0;
    zeroFlag_ = (flags & 0x0040U) != 0;
    signFlag_ = (flags & 0x0080U) != 0;
    directionFlag_ = (flags & 0x0400U) != 0;
    overflowFlag_ = (flags & 0x0800U) != 0;
}

void Cpu16Core::pushWord(Memory& memory, std::uint16_t value) {
    registers_[SP] = static_cast<std::uint16_t>(registers_[SP] - 2U);
    memory.write16(physicalAddress(registers_[SS], registers_[SP]), value);
}

std::uint16_t Cpu16Core::popWord(Memory& memory) {
    std::uint16_t value = 0;
    memory.read16(physicalAddress(registers_[SS], registers_[SP]), value);
    registers_[SP] = static_cast<std::uint16_t>(registers_[SP] + 2U);
    return value;
}

std::uint8_t Cpu16Core::readByte(const Memory& memory, std::uint32_t address) const noexcept {
    if (address >= 0xF0000U && address < 0x100000U)
        return biosRom_[address - 0xF0000U];
    std::uint8_t value = 0;
    memory.read8(address, value);
    return value;
}

std::uint16_t Cpu16Core::readWord(const Memory& memory, std::uint32_t address) const noexcept {
    return static_cast<std::uint16_t>(readByte(memory, address)) |
           (static_cast<std::uint16_t>(readByte(memory, address + 1U)) << 8U);
}

void Cpu16Core::writeWord(Memory& memory, std::uint32_t address,
                          std::uint16_t value) noexcept {
    if (address < 0xF0000U || address >= 0x100000U)
        memory.write8(address, static_cast<std::uint8_t>(value & 0xFFU));
    if (address + 1U < 0xF0000U || address + 1U >= 0x100000U)
        memory.write8(address + 1U, static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void Cpu16Core::setFlagsForResult(std::uint16_t result, std::uint16_t lhs,
                                  std::uint16_t rhs, bool isAdd) noexcept {
    zeroFlag_ = result == 0;
    signFlag_ = (result & 0x8000U) != 0;
    if (isAdd) {
        overflowFlag_ = ((lhs & 0x8000U) == (rhs & 0x8000U)) &&
                        ((result & 0x8000U) != (lhs & 0x8000U));
        carryFlag_ = result < lhs || result < rhs;
    } else {
        overflowFlag_ = ((lhs & 0x8000U) != (rhs & 0x8000U)) &&
                        ((result & 0x8000U) != (lhs & 0x8000U));
        carryFlag_ = lhs < rhs;
    }
}

bool Cpu16Core::dispatchPendingIRQ(Memory& memory) {
    if (!interruptFlag_ || !bytecodeActive_) return false;
    const std::uint8_t vector = keyboardIrqPending_ ? 0x09 :
                                timerIrqPending_ ? 0x08 : 0;
    if (vector == 0) return false;

    keyboardIrqPending_ = false;
    timerIrqPending_ = false;
    const std::uint32_t address = static_cast<std::uint32_t>(vector) * 4U;
    const std::uint16_t offset = readWord(memory, address);
    const std::uint16_t segment = readWord(memory, address + 2U);
    if (offset == 0 && segment == 0) return false;

    pushWord(memory, flagsWord());
    pushWord(memory, registers_[CS]);
    pushWord(memory, ip_);
    interruptFlag_ = false;
    registers_[CS] = segment;
    ip_ = offset;
    return true;
}

CpuStepResult Cpu16Core::step(Memory& memory, DeviceBus& devices) {
    if (halted_) return {CpuStepState::Halted, "CPU is halted"};
    if (!bytecodeActive_) return {CpuStepState::Waiting, "No executable is loaded"};

    dispatchPendingIRQ(memory);
    if (static_cast<std::size_t>(ip_) + 7U > bytecode_.size()) {
        halted_ = true;
        bytecodeActive_ = false;
        return {CpuStepState::Fault, "Invalid CPU16 bytecode instruction pointer"};
    }

    const std::uint8_t opcode = bytecode_[ip_++];
    const std::uint8_t operandAType = bytecode_[ip_++];
    const std::uint16_t operandA = static_cast<std::uint16_t>(bytecode_[ip_]) |
                                   (static_cast<std::uint16_t>(bytecode_[ip_ + 1U]) << 8U);
    ip_ += 2U;
    const std::uint8_t operandBType = bytecode_[ip_++];
    const std::uint16_t operandB = static_cast<std::uint16_t>(bytecode_[ip_]) |
                                   (static_cast<std::uint16_t>(bytecode_[ip_ + 1U]) << 8U);
    ip_ += 2U;

    const auto value = [this](std::uint8_t type, std::uint16_t raw) {
        return type == 0 ? registerValue(raw) : raw;
    };
    const auto put = [this](std::uint8_t type, std::uint16_t raw, std::uint16_t v) {
        if (type == 0 && raw < registers_.size()) registers_[raw] = v;
    };
    const std::uint16_t a = value(operandAType, operandA);
    const std::uint16_t b = value(operandBType, operandB);

    switch (opcode) {
        case 1: put(operandAType, operandA, b); break; // MOV
        case 3: pushWord(memory, a); break;             // PUSH
        case 4: put(operandAType, operandA, popWord(memory)); break; // POP
        case 5: {
            const std::uint16_t result = static_cast<std::uint16_t>(a + b);
            put(operandAType, operandA, result);
            setFlagsForResult(result, a, b, true);
            break;
        }
        case 6: {
            const std::uint16_t result = static_cast<std::uint16_t>(a - b);
            put(operandAType, operandA, result);
            setFlagsForResult(result, a, b, false);
            break;
        }
        case 7: setFlagsForResult(static_cast<std::uint16_t>(a - b), a, b, false); break;
        case 15: put(operandAType, operandA, readWord(memory, physicalAddress(registers_[DS], b))); break;
        case 16: writeWord(memory, physicalAddress(registers_[DS], b), a); break;
        case 17: { // IN
            std::uint16_t input = 0;
            if (!devices.read(b, input)) {
                if (b == 0 || b == 2) {
                    ip_ = static_cast<std::uint16_t>(ip_ - 7U);
                    return {CpuStepState::Waiting, "Waiting for guest input"};
                }
            }
            put(operandAType, operandA, input);
            break;
        }
        case 18: // OUT
            devices.write(a, b);
            break;
        case 29:
            halted_ = true;
            bytecodeActive_ = false;
            return {CpuStepState::Halted, "Guest program halted"};
        case 30: ip_ = a; break; // JMP
        case 31: case 32: if (zeroFlag_) ip_ = a; break;
        case 33: case 34: if (!zeroFlag_) ip_ = a; break;
        case 35: if (!zeroFlag_ && signFlag_ == overflowFlag_) ip_ = a; break;
        case 36: if (signFlag_ != overflowFlag_) ip_ = a; break;
        case 37: pushWord(memory, ip_); ip_ = a; break; // CALL
        case 38: ip_ = popWord(memory); break;          // RET
        case 39:
            registers_[CX] = static_cast<std::uint16_t>(registers_[CX] - 1U);
            if (registers_[CX] != 0) ip_ = a;
            break;
        case 40: { // INT
            const std::uint8_t vector = static_cast<std::uint8_t>(a);
            const std::uint32_t address = static_cast<std::uint32_t>(vector) * 4U;
            const std::uint16_t offset = readWord(memory, address);
            const std::uint16_t segment = readWord(memory, address + 2U);
            pushWord(memory, flagsWord());
            pushWord(memory, registers_[CS]);
            pushWord(memory, ip_);
            interruptFlag_ = false;
            if (offset != 0 || segment != 0) {
                ip_ = offset;
                registers_[CS] = segment;
            } else {
                const std::uint16_t returnIp = popWord(memory);
                const std::uint16_t returnCs = popWord(memory);
                const std::uint16_t returnFlags = popWord(memory);
                ip_ = returnIp;
                registers_[CS] = returnCs;
                restoreFlags(returnFlags);
            }
            break;
        }
        case 41:
            ip_ = popWord(memory);
            registers_[CS] = popWord(memory);
            restoreFlags(popWord(memory));
            interruptFlag_ = true;
            break;
        default:
            halted_ = true;
            bytecodeActive_ = false;
            return {CpuStepState::Fault, "Unsupported CPU16 bytecode opcode"};
    }

    return {CpuStepState::Continue, nullptr};
}

} // namespace cpu16::vm
