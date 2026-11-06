# CPU16 Server — architecture baseline

## Product definition

CPU16 Server is a **virtual computer**, not a host-native server application that exposes the host filesystem as its operating system. The host executable is only the emulator/runtime. The guest CPU executes guest machine code; the guest OS owns its virtual disks, filesystem, users, processes, services, configuration and logs.

The first development target is the server platform. CPU16 Desktop integration comes later and must use the same guest-facing network contract as any other client.

## Runtime layers

```
Host process (cpu_server)
  ├─ VM runtime / scheduler
  │   ├─ CPU16 CPU core
  │   ├─ RAM and memory bus
  │   ├─ virtual disk controllers + disk images
  │   └─ virtual device bus
  ├─ network adapter(s)
  │   └─ network backend
  │       ├─ VirtualNetwork (same host)
  │       └─ LAN transport (different hosts)
  └─ diagnostics / host launch configuration

Guest machine
  └─ bootloader
      └─ CPU16 Server OS
          ├─ kernel and drivers
          ├─ guest filesystem on virtual disks
          ├─ process / user / service managers
          └─ guest network stack and network services
```

Host diagnostics and launch configuration are not guest OS facilities. Host paths may store opaque disk-image files and emulator diagnostics; guest programs must never receive direct access to host paths.

## Network model

A guest machine sees a network interface and sends/receives guest packets through a stable adapter contract. The adapter is independent of the transport:

- **Virtual network:** an in-process virtual switch connects multiple VMs or emulated endpoints on one physical computer.
- **LAN network:** a host transport carries the same guest packet format between emulator instances on different physical computers. The LAN backend is a later implementation step; the current code provides the transport boundary, not a claim of LAN connectivity.

Both modes must preserve the same guest-visible addressing and packet semantics. A future bridge mode may connect the virtual network to a physical LAN, with explicit configuration and safety controls.

The initial address type is IPv4. Network protocol identifiers use the IP protocol number space (for example ICMP=1, TCP=6, UDP=17). The first virtual switch delivers datagrams only; TCP/IP protocol implementations belong in the guest network stack and are not implied by this transport layer.

## Existing CPU2 compatibility findings

The current desktop implementation is still in the root `main.cpp`. Its `CPU` class contains two execution paths: a source-line interpreter used by the editor and a seven-byte instruction bytecode engine used by compiled `.exe` images. The class currently owns a fixed 1 MiB RAM array, a 64 KiB firmware ROM, CPU registers/flags/cache, and direct dependencies on the desktop `VirtualDisk` and terminal input/output.

The existing I/O behavior includes the documented terminal, keyboard and disk ports, plus software interrupt behavior. Disk operations currently call the desktop `VirtualDisk` directly. The compiled-bytecode path and memory representation also need a careful compatibility review before extraction; the bytecode instruction pointer currently indexes the bytecode vector, while instructions are also copied into RAM.

**Do not replace this CPU with a newly invented ISA.** The server VM now defines a hardware boundary and a `CpuCore` contract, but the real CPU2 implementation is not yet connected to it. The next CPU step is a separate extraction/refactor: preserve the existing registers, opcodes, flags, interrupt semantics and port numbers while moving disk/terminal effects behind virtual devices. The root `main.cpp` remains unchanged in this phase.

The existing 10 MiB `VirtualDisk` format is a compatibility candidate, not yet a guest block device. It currently owns host-file persistence and a guest-style filesystem in one class. A later storage phase must separate an opaque host-backed disk image from the guest filesystem and expose only sector/block operations to the guest.

## Current VM hardware foundation

The server library now provides:

- per-machine configurable RAM allocation (default 1 MiB), with bounds-checked byte/word access and bounded image loading;
- a port-mapped `DeviceBus` with overlap checks and explicit attach/detach;
- an `IODevice` contract for virtual hardware;
- a `MachineConfig` validator;
- a `VirtualMachine` composition object with isolated RAM, device bus, CPU slot and deterministic instruction-cycle budget;
- a `CpuCore` interface so a real CPU implementation can be installed without making the VM depend on the desktop UI.

The VM deliberately reports `CpuNotConfigured` until a real CPU core is installed. Test-only fake CPUs verify scheduling and isolation; they are not shipped as a guest CPU implementation. There is not yet a disk controller, bootable machine, guest OS, or actual CPU2 core integration.

## Development phases

1. **VM hardware boundary** — configurable RAM, device bus, CPU contract, deterministic run loop and tests.
2. **CPU2 core extraction** — move the real compiled-bytecode execution engine into a reusable headless core; isolate legacy source-line interpretation and route I/O through devices without changing the ISA.
3. **Virtual storage** — disk image/block-device boundary, sector I/O and compatibility tests for existing disk data.
4. **Boot and guest storage** — boot ROM/loader and a guest filesystem that runs inside the VM.
5. **Guest OS** — kernel, drivers, process/user/service managers and guest shell.
6. **Guest networking** — attach the existing network adapter to each VM, then implement guest-side IPv4, ICMP, UDP/TCP and server services.
7. **LAN transport** — portable host socket transport and explicit network configuration; test between separate processes and physical hosts.
8. **Desktop integration** — CPU16 Desktop becomes a guest network client; do not couple the server kernel to desktop UI code.

## Compatibility and safety rules

- Keep the server VM headless-capable.
- Never share C++ object pointers across process boundaries; use a versioned byte protocol for IPC.
- Treat all LAN packets as untrusted input. Add size limits, validation, timeouts and opt-in listening before exposing a service.
- Do not silently bridge virtual traffic to the physical LAN.
- Do not label host-side shell commands or service placeholders as guest OS functionality.
- Keep the main desktop emulator entry point unchanged until the shared CPU-core extraction is a separately reviewed change.
