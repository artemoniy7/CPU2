# CPU-16 Server OS

Separate server-side host application for CPU2. This is currently a management shell and
host-side configuration/event-log foundation; it is not yet a guest operating system running
on an emulated CPU.

## Build

From this directory:

    cmake -S . -B build
    cmake --build build --config Release

Run the automated tests:

    ctest --test-dir build --build-config Release --output-on-failure

## Run

Run from the Server/ directory so the default relative paths resolve there:

    ./build/cpu_server --standalone
    ./build/cpu_server --attached
    ./build/cpu_server --config ./config/server.ini
    ./build/cpu_server --help

The default configuration path is config/server.ini, relative to the process working directory.
Use --config <path> to select another file.

## Shell commands

- HELP, STATUS, VERSION, HOSTNAME [name], UPTIME, WHOAMI
- ECHO <text>, CLEAR, MACHINES, SERVICES
- SERVICE START|STOP|INFO <name>
- CONFIG SHOW|SAVE|RELOAD
- LOG [count], LOG CLEAR, LOG PATH
- PROCESS, NETSTAT, SHUTDOWN

## Configuration Manager

The INI-style configuration currently supports:

- hostname: 1-32 ASCII letters/digits, hyphen or underscore.
- log_file: a 1-240 character path without control characters.

Blank lines and lines beginning with # or ; are ignored. Unknown keys, duplicate keys,
malformed lines and invalid values are rejected. A failed reload does not replace the active
hostname or log path. The configured log target must be readable when it already exists and
must be openable for appending before the new configuration is activated.

If the configuration file is missing, the server initializes the default log target and creates
a configuration file with the current defaults. HOSTNAME changes the live value; use
CONFIG SAVE to persist it. CONFIG RELOAD reads and validates the file again.

Saving writes and closes a temporary file before replacing the destination. Replacement is
performed without first deleting the existing configuration file: POSIX uses filesystem rename,
and Windows uses MoveFileEx with replacement/write-through flags. This protects the previous
configuration from a failed replacement operation. It is not a guarantee against every possible
power-loss or storage-device failure.

## Persistent logging

Events are appended to logs/server.log by default. The persistent file is not size-rotated;
the latest 100 entries are kept in memory and restored from the end of the file at startup.
LOG [count] displays entries currently retained in memory, LOG PATH reports the active path,
and LOG CLEAR truncates the file and writes a new "Event log cleared" audit entry.

A log path is activated only after its directory can be prepared, existing contents can be read,
and append access can be opened. If a later write fails, the server keeps the in-memory event
history, marks persistent logging unavailable and continues running. Selecting a working log
path or successfully clearing the current file can recover logging.

The tests cover configuration round trips, missing defaults, rejection without partial state
changes, persistent log restoration and clearing, and the shell commands.

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