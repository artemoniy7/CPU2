# CPU-16 Server OS

Separate server-side application for CPU2.

## Build

From this directory:

```sh
cmake -S . -B build
cmake --build build --config Release
```

## Run

```sh
cpu_server --standalone
cpu_server --attached
cpu_server --help
```

## Shell commands

`HELP`, `STATUS`, `VERSION`, `HOSTNAME [name]`, `UPTIME`, `WHOAMI`,
`ECHO`, `CLEAR`, `MACHINES`, `SERVICES`, `SERVICE START|STOP|INFO <name>`,
`PROCESS`, `NETSTAT`, `LOG`, `SHUTDOWN`.

## Current implementation boundary

The shell, hostname, uptime counter, bounded in-memory event log and service-state
registry are implemented. CLOCK is a built-in state entry. SPOOLER, HTTPD and FTPD
are placeholders; they do not launch guest programs or listen on network ports.

The process list, machine registry, CPU Core, IPC and network stack are not connected.
Attached mode currently selects a mode label only; it does not yet connect to cpu_emu.

## Architecture

- CPU Core: planned shared library extracted from the emulator.
- Standalone mode: will own a headless virtual machine.
- Attached mode: will connect to an emulator-owned machine through IPC.
- Service Manager: will launch and supervise guest programs.
- Network Manager: will provide host transport and guest-facing network APIs.

Never share C++ object pointers between separate processes. Use a defined IPC protocol.
