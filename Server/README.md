# CPU-16 Server OS

Separate server-side application for CPU2.

## Build

From this directory:

```sh
cmake -S . -B build
cmake --build build --config Release
```

## Run

Run from the `Server/` directory so the default relative paths resolve there:

```sh
./build/cpu_server --standalone
./build/cpu_server --attached
./build/cpu_server --config ./config/server.ini
./build/cpu_server --help
```

The default configuration path is `config/server.ini`, relative to the process working directory.
Use `--config <path>` to select another file.

## Shell commands

- `HELP`, `STATUS`, `VERSION`, `HOSTNAME [name]`, `UPTIME`, `WHOAMI`
- `ECHO <text>`, `CLEAR`, `MACHINES`, `SERVICES`
- `SERVICE START|STOP|INFO <name>`
- `CONFIG SHOW|SAVE|RELOAD`
- `LOG [count]`, `LOG CLEAR`, `LOG PATH`
- `PROCESS`, `NETSTAT`, `SHUTDOWN`

## Configuration Manager

The INI-style configuration currently supports:
- `hostname`: 1-32 ASCII letters/digits, hyphen or underscore.
- `log_file`: path to the persistent event log.

Blank lines and lines beginning with `#` or `;` are ignored. Unknown keys and malformed lines are rejected.
Missing configuration files are initialized with defaults. `HOSTNAME` changes the live value;
run `CONFIG SAVE` to persist it. `CONFIG RELOAD` reads the file again.

Saving writes a temporary file and renames it into place. The fallback replacement path on
platforms that do not replace an existing file with rename is best-effort, not crash-proof.

## Persistent logging

Events are appended to `logs/server.log` by default. The latest 100 entries are retained in
memory for shell display; prior log lines are loaded on startup. `LOG [count]` prints recent
entries, `LOG PATH` reports the configured path, and `LOG CLEAR` truncates the file.
If the log cannot be opened or written, the server continues running and reports logging as
unavailable. There is currently no size-based rotation or inter-process log locking.

## Current implementation boundary

The shell, hostname, uptime counter, configuration manager, append-only persistent event log
and service-state registry are implemented. CLOCK is a built-in state entry. SPOOLER, HTTPD and
FTPD are placeholders; they do not launch guest programs or listen on network ports.

The process list, machine registry, CPU Core, IPC and network stack are not connected.
Attached mode currently selects a mode label only; it does not yet connect to cpu_emu.

## Architecture

- CPU Core: planned shared library extracted from the emulator.
- Standalone mode: will own a headless virtual machine.
- Attached mode: will connect to an emulator-owned machine through IPC.
- Service Manager: will launch and supervise guest programs.
- Network Manager: will provide host transport and guest-facing network APIs.

Never share C++ object pointers between separate processes. Use a defined IPC protocol.
