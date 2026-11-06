# CPU-16 Server OS

Separate server-side application for the CPU2 virtual computer project.

## Build

From this directory:

```sh
cmake -S . -B build
cmake --build build --config Release
```

The executable is named `cpu_server` (or `cpu_server.exe` on Windows).

## Run

```sh
cpu_server --standalone
cpu_server --attached
cpu_server --help
```

## Current status

This is the first shell/bootstrap milestone. It does not yet execute guest CPU instructions,
load guest programs, access the emulator's virtual disk, or establish IPC.

## Planned architecture

- CPU Core: shared CPU, memory, interrupts and virtual-disk library extracted from the emulator.
- Standalone host: owns a headless virtual machine instance.
- Attached host: connects to an emulator-owned instance through IPC.
- Process and service managers: guest-program lifecycle and background services.
- Network manager: host TCP transport and guest-facing network API.

The emulator and server must never share C++ object pointers across process boundaries.
