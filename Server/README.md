# CPU16 Server

CPU16 Server is being built as a complete emulated computer and guest operating system. The host executable provides virtual hardware; the guest OS will run as CPU16 machine code inside that hardware.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the system boundaries and implementation phases.

## Current status

- [x] Architecture baseline and guest/host separation documented.
- [x] Common network-adapter contract and in-process virtual switch.
- [x] Configurable guest RAM with bounds-checked access.
- [x] CPU16 compiled-bytecode core for the existing seven-byte instruction format.
- [x] Virtual I/O device contract and port bus.
- [x] Machine configuration, VM composition and deterministic instruction-cycle runner.
- [x] Automated tests for RAM, device routing, CPU instructions, guest I/O waits and VM scheduling.
- [x] Versioned persistent virtual disk image with bounded 512-byte sector I/O and block-device abstraction.
- [x] Standalone Server Manager CLI for creating, listing and inspecting VM definitions.
- [ ] Runtime disk-controller integration and guest filesystem.
- [ ] Default console, keyboard and timer devices.
- [ ] Boot ROM/loader and bootable guest Server OS.
- [ ] Guest filesystem, processes, users, services and network stack.
- [ ] Physical LAN transport between separate emulator processes / computers.
- [ ] CPU16 Desktop reuse of the same CPU core.

The server CPU executes the existing compiled CPU16 bytecode format. The desktop editor's source-line interpreter is not part of the server core, and the desktop application has not yet been switched to the shared core.

The VM does not yet boot an operating system: virtual storage, boot firmware/loader and guest OS are future stages. The current network switch is in-process only and does not provide physical LAN connectivity or a complete TCP/IP stack.

## Server Manager

Build a standalone server management executable:

```sh
cmake -S Server -B build-server -DBUILD_TESTING=ON
cmake --build build-server --config Release
./build-server/cpu16-server help
./build-server/cpu16-server create node01 "My Server" 64 512
./build-server/cpu16-server list
./build-server/cpu16-server info node01
```

On Windows, run `build-server\\Release\\cpu16-server.exe` when using a multi-configuration generator. The manager stores host-side machine metadata and opaque disk images in `cpu16-server-data/` under the working directory. Each newly provisioned VM gets a 64 MiB disk by default, with a unique image filename.

This is the first management interface (CLI). The graphical Server Manager window is a separate next step; the catalog and storage APIs are designed to be reused by it.

## Build and test

From the repository root:

```sh
cmake -S Server -B build-server -DBUILD_TESTING=ON
cmake --build build-server --config Release
ctest --test-dir build-server -C Release --output-on-failure
```

The server foundation uses standard C++17 and has no third-party dependencies.
