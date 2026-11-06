# CPU16 Server

CPU16 Server is intended to emulate a complete server computer and its guest operating system. The host executable provides virtual hardware; the guest OS will run as CPU16 machine code inside that hardware.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the system boundaries and implementation phases.

## Current status

- [x] Architecture baseline and guest/host separation documented.
- [x] Common network-adapter contract defined.
- [x] In-process virtual switch with unicast and broadcast delivery.
- [ ] Physical LAN transport between separate emulator processes / computers.
- [ ] CPU16 CPU core integration.
- [ ] Virtual RAM, device bus and disk controller.
- [ ] Bootloader and guest Server OS.
- [ ] Guest filesystem, processes, users, services and network stack.
- [ ] CPU16 Desktop client integration.

The virtual switch in this first foundation build is an in-process transport only. It does **not** yet provide LAN connectivity, TCP/IP, a guest OS, or a bootable VM.

## Build and test

From the repository root:

```sh
cmake -S Server -B build-server
cmake --build build-server --config Release
ctest --test-dir build-server --output-on-failure
```

The network foundation is standard C++17 and has no third-party dependencies.
