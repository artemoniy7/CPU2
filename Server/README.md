# CPU16 Server

CPU16 Server is a separate host-side virtual server platform. The server core is portable C++17; the first graphical management application is currently a native Windows desktop application.

## Command-line manager

Build the server core and CLI:

```sh
cmake -S Server -B build-server -DBUILD_TESTING=ON
cmake --build build-server --config Release
./build-server/cpu16-server help
./build-server/cpu16-server create node01 "My Server" 64 512
./build-server/cpu16-server list
./build-server/cpu16-server info node01
```

On Windows, run `build-server\\Release\\cpu16-server.exe` when using a multi-configuration generator.

## Graphical Server Manager (Windows)

The first native desktop dashboard is built on Windows as a separate executable:

```powershell
cmake -S Server -B build-server -DBUILD_TESTING=ON
cmake --build build-server --config Release --target cpu16-server-manager
.\\build-server\\Release\\cpu16-server-manager.exe
```

The dashboard uses the same `MachineCatalog` and host data directory as the CLI. It includes Overview, Virtual Machines, Storage, Networks and Event Log sections, a machine inventory, hardware capacity summaries and refresh. The initial UI is intentionally read-only: VM start/stop, provisioning dialogs, live utilization charts and remote host management will be connected after the VM lifecycle/runtime APIs exist.

## Current limitations

- VM definitions and persistent virtual disk images can be provisioned and inspected.
- The VM runtime does not yet attach the disk controller, boot firmware or a guest OS.
- The in-process virtual network is not a physical LAN transport or a full TCP/IP stack.
- The graphical manager is Windows-only in this first iteration; the portable CLI and server core remain cross-platform.
