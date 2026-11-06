# CPU16 Server

CPU16 Server is intended to emulate a complete server computer and its guest operating system. The host executable provides virtual hardware; the guest OS will run as CPU16 machine code inside that hardware.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the system boundaries and implementation phases.

## Current status

- [x] Architecture baseline and guest/host separation documented.
- [x] Common network-adapter contract and in-process virtual switch.
- [x] Configurable guest RAM with bounds-checked access.
- [x] Virtual I/O device contract and port bus.
- [x] Machine configuration, VM composition and deterministic cycle-budget runner.
- [x] CPU-core interface for later integration of the real CPU2 execution engine.
- [ ] Real CPU2 execution core extracted from the desktop emulator and installed in the VM.
- [ ] Virtual disk image and block-device controller.
- [ ] Bootloader and guest Server OS.
- [ ] Guest filesystem, processes, users, services and network stack.
- [ ] Physical LAN transport between separate emulator processes / computers.
- [ ] CPU16 Desktop client integration.

**Important:** the VM hardware foundation is not yet a bootable virtual computer. Until the real CPU2 core is extracted and installed, `VirtualMachine::runFor()` correctly reports `CpuNotConfigured`. Test-only CPU implementations are used only to test the VM scheduler and are not guest CPUs.

The current network switch is in-process only. It does **not** yet provide LAN connectivity or a complete TCP/IP stack.

## Build and test

From the repository root:

```sh
cmake -S Server -B build-server -DBUILD_TESTING=ON
cmake --build build-server --config Release
ctest --test-dir build-server -C Release --output-on-failure
```

The server foundation uses standard C++17 and has no third-party dependencies.
