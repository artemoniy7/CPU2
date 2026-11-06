#include "vm/Cpu16Core.h"
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

void emit(std::vector<std::uint8_t>& code, std::uint8_t opcode,
          std::uint8_t aType = 1, std::uint16_t a = 0,
          std::uint8_t bType = 1, std::uint16_t b = 0) {
    code.push_back(opcode);
    code.push_back(aType);
    code.push_back(static_cast<std::uint8_t>(a & 0xFFU));
    code.push_back(static_cast<std::uint8_t>(a >> 8U));
    code.push_back(bType);
    code.push_back(static_cast<std::uint8_t>(b & 0xFFU));
    code.push_back(static_cast<std::uint8_t>(b >> 8U));
}

class PortDevice final : public IODevice {
public:
    bool read(std::uint16_t offset, std::uint16_t& value) override {
        if (offset != 0 || !readable) return false;
        value = stored;
        return true;
    }

    bool write(std::uint16_t offset, std::uint16_t value) override {
        if (offset != 0) return false;
        stored = value;
        wrote = true;
        return true;
    }

    std::uint16_t stored{0};
    bool readable{false};
    bool wrote{false};
};

} // namespace

int main() {
    try {
        MachineConfig config;
        VirtualMachine machine(config);
        auto output = std::make_shared<PortDevice>();
        require(machine.devices().attach(1, 1, output), "output device attach failed");

        std::vector<std::uint8_t> program;
        emit(program, 1, 0, 0, 1, 42); // MOV AX, 42
        emit(program, 18, 1, 1, 0, 0); // OUT 1, AX
        emit(program, 29); // HLT

        auto cpu = std::make_unique<Cpu16Core>();
        Cpu16Core* cpuView = cpu.get();
        require(cpuView->loadBytecode(machine.memory(), program), "valid bytecode load failed");
        require(machine.installCpu(std::move(cpu)), "CPU install failed");

        const RunResult run = machine.runFor(20);
        require(run.state == RunState::Halted, "guest HLT was not propagated by the VM");
        require(run.cyclesExecuted == 3, "unexpected CPU instruction count");
        require(cpuView->registerValue(0) == 42, "MOV did not update AX");
        require(output->wrote && output->stored == 42, "OUT did not reach the virtual device");
        require(machine.totalCycles() == 3, "VM cycle counter is incorrect");

        machine.reset();
        require(!cpuView->loadBytecode(machine.memory(), {1, 0, 0}),
                "malformed bytecode should be rejected");

        MachineConfig inputConfig;
        VirtualMachine inputMachine(inputConfig);
        std::vector<std::uint8_t> inputProgram;
        emit(inputProgram, 17, 0, 0, 1, 0); // IN AX, 0
        emit(inputProgram, 29); // HLT
        auto inputCpu = std::make_unique<Cpu16Core>();
        Cpu16Core* inputCpuView = inputCpu.get();
        require(inputCpuView->loadBytecode(inputMachine.memory(), inputProgram),
                "input program load failed");
        require(inputMachine.installCpu(std::move(inputCpu)), "input CPU install failed");

        RunResult waiting = inputMachine.runFor(5);
        require(waiting.state == RunState::Waiting && waiting.cyclesExecuted == 1,
                "unavailable input port should suspend execution");
        require(inputCpuView->instructionPointer() == 0,
                "waiting IN instruction must be retried without advancing IP");

        auto input = std::make_shared<PortDevice>();
        input->stored = 0x1234;
        input->readable = true;
        require(inputMachine.devices().attach(0, 1, input), "input device attach failed");
        const RunResult resumed = inputMachine.runFor(5);
        require(resumed.state == RunState::Halted, "CPU did not resume after input became available");
        require(inputCpuView->registerValue(0) == 0x1234, "IN did not read the virtual device value");

        std::cout << "CPU16 core tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "CPU16 core test failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
