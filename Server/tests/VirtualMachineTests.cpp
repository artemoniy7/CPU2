#include "vm/DeviceBus.h"
#include "vm/VirtualMachine.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace cpu16::vm;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class TestPortDevice final : public IODevice {
public:
    bool read(std::uint16_t offset, std::uint16_t& value) override {
        if (offset != 1) return false;
        value = stored;
        return true;
    }

    bool write(std::uint16_t offset, std::uint16_t value) override {
        if (offset != 1) return false;
        stored = value;
        return true;
    }

    std::uint16_t stored{0};
};

class TestCpu final : public CpuCore {
public:
    void reset() override { steps = 0; }

    CpuStepResult step(Memory& memory, DeviceBus& devices) override {
        ++steps;
        memory.write8(0, static_cast<std::uint8_t>(steps));
        std::uint16_t value = 0;
        if (devices.read(0x11, value)) lastPortValue = value;
        return steps == 3
            ? CpuStepResult{CpuStepState::Halted, "test CPU halted"}
            : CpuStepResult{CpuStepState::Continue, nullptr};
    }

    std::uint32_t steps{0};
    std::uint16_t lastPortValue{0};
};

} // namespace

int main() {
    try {
        Memory memory(128);
        std::uint8_t byte = 0;
        require(memory.write8(127, 0xA5), "last RAM byte should be writable");
        require(memory.read8(127, byte) && byte == 0xA5, "RAM byte round-trip failed");
        require(!memory.read8(128, byte), "out-of-range RAM read should fail");
        require(!memory.write8(128, 0xFF), "out-of-range RAM write should fail");
        require(memory.write16(10, 0x1234), "word write failed");
        std::uint16_t word = 0;
        require(memory.read16(10, word) && word == 0x1234, "little-endian word read failed");
        require(!memory.write16(127, 0xFFFF), "word write past RAM end should fail");
        require(memory.load(120, {1, 2, 3, 4, 5, 6, 7, 8}), "bounded image load failed");
        require(!memory.load(125, {1, 2, 3, 4}), "overflowing image load should fail");

        DeviceBus bus;
        auto portDevice = std::make_shared<TestPortDevice>();
        require(bus.attach(0x10, 4, portDevice), "device attach failed");
        require(!bus.attach(0x12, 2, std::make_shared<TestPortDevice>()),
                "overlapping device range should fail");
        require(bus.write(0x11, 0xBEEF), "device port write failed");
        require(bus.read(0x11, word) && word == 0xBEEF, "device port read failed");
        require(!bus.read(0x20, word), "unmapped port should fail");

        MachineConfig config;
        config.ramSizeBytes = 4096;
        require(!config.isValid(), "too-small RAM configuration should fail");
        config.ramSizeBytes = 128 * 1024;
        require(config.isValid(), "valid machine configuration rejected");

        VirtualMachine machine(config);
        require(!machine.hasCpu(), "CPU must not be fabricated when no core is installed");
        require(machine.runFor(10).state == RunState::CpuNotConfigured,
                "VM without a real CPU should report CpuNotConfigured");
        auto cpu = std::make_unique<TestCpu>();
        TestCpu* cpuView = cpu.get();
        require(machine.installCpu(std::move(cpu)), "CPU installation failed");
        require(!machine.installCpu(std::make_unique<TestCpu>()),
                "VM must reject replacing an installed CPU");
        auto vmPortDevice = std::make_shared<TestPortDevice>();
        require(machine.devices().attach(0x10, 4, vmPortDevice),
                "VM device attachment failed");

        RunResult run = machine.runFor(2);
        require(run.state == RunState::BudgetExhausted && run.cyclesExecuted == 2,
                "cycle budget was not respected");
        require(machine.totalCycles() == 2, "cycle counter mismatch");
        run = machine.runFor(5);
        require(run.state == RunState::Halted && run.cyclesExecuted == 1,
                "CPU halt state was not propagated");
        require(machine.totalCycles() == 3, "total cycle count mismatch");
        std::uint8_t finalByte = 0;
        require(machine.memory().read8(0, finalByte) && finalByte == 3,
                "CPU did not execute against VM-owned RAM");
        require(cpuView->lastPortValue == 0, "uninitialized device read should return zero");

        machine.reset();
        require(machine.totalCycles() == 0, "reset did not clear cycle count");
        require(machine.memory().read8(0, finalByte) && finalByte == 0,
                "reset did not clear guest RAM");

        std::cout << "Virtual machine hardware tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Virtual machine hardware test failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
