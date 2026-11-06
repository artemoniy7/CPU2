# CPU16 Server — architecture baseline

## Product definition

CPU16 Server is a **virtual computer**, not a host-native server application that exposes the host filesystem as its operating system. The host executable is only the emulator/runtime. The guest CPU executes guest machine code; the guest OS owns its virtual disks, filesystem, users, processes, services, configuration and logs.

The first development target is the server platform. CPU16 Desktop integration comes later and must use the same guest-facing hardware and network contracts as any other client.

## Runtime layers

```
Host process (cpu_server)
  ├─ VM runtime / scheduler
  │   ├─ CPU16 CPU core
  │   ├─ RAM and memory map
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
- **LAN network:** a host transport carries the same guest packet format between emulator instances on different physical computers. The LAN backend is not implemented yet.

Both modes must preserve the same guest-visible addressing and packet semantics. A future bridge mode may connect the virtual network to a physical LAN, with explicit configuration and safety controls.

The initial address type is IPv4. Network protocol identifiers use the IP protocol number space (for example ICMP=1, TCP=6, UDP=17). The first virtual switch delivers datagrams only; TCP/IP protocol implementations belong in the guest network stack and are not implied by this transport layer.

## Existing CPU2 compatibility findings

The desktop implementation remains in the root `main.cpp`. Its `CPU` class contains two execution paths: a source-line interpreter used by the editor and a seven-byte instruction bytecode engine used by compiled `.exe` images. The class also owns registers/flags, a fixed 1 MiB RAM vector, a 64 KiB firmware ROM, a small cache, and direct dependencies on the desktop `VirtualDisk` and terminal input/output.

The compiled bytecode format uses seven-byte instructions and the existing opcode/register identifiers. The server-side `Cpu16Core` now implements this compiled-bytecode execution path as a headless core, including its current arithmetic, control-flow, stack, memory, IN/OUT and interrupt behavior. It uses VM-owned RAM and routes port I/O through `DeviceBus`; it does not call the desktop `VirtualDisk` or GUI.

This is a **server-side extraction/port of the existing compiled-bytecode engine**, not yet a shared library used by both applications. The editor's source-line interpreter remains desktop-only, and root `main.cpp` has not been changed. A later compatibility step should make Desktop use the same core rather than maintain two copies.

The legacy compiled engine copied bytecode into RAM but fetched instructions from its bytecode vector using a 16-bit instruction pointer. The server core preserves that behavior for compatibility. Its bytecode image is limited to the addressable 16-bit instruction-offset range. A 64 KiB firmware window is mapped read-only at physical `0xF0000`, matching the existing desktop model.

The existing 10 MiB `VirtualDisk` format is a compatibility candidate, not yet a guest block device. It currently combines host-file persistence and a guest-style filesystem in one class. A later storage phase must separate an opaque host-backed disk image from the guest filesystem and expose only sector/block operations to the guest.

## Current VM hardware foundation

The server library now provides:

- per-machine configurable RAM (default 1 MiB), with bounds-checked byte/word access and bounded image loading;
- the CPU16 compiled-bytecode core with the existing register IDs, opcode IDs, flags, stack, interrupt vectors and firmware memory window;
- a port-mapped `DeviceBus` and `IODevice` contract with overlap checks and explicit attach/detach;
- a `MachineConfig` validator;
- a `VirtualMachine` composition object with isolated RAM, device bus, CPU and deterministic instruction-cycle budget;
- explicit run states for exhausted cycle budgets, halted CPU, waiting for guest input, CPU faults and missing CPU.

The VM can execute CPU16 bytecode when a `Cpu16Core` is installed and a program image is loaded. The tests use a separate fake CPU only to verify scheduler behavior; it is not shipped as a guest CPU implementation.

Not implemented yet: a disk controller/block device, persistent disk-image container, default console/keyboard devices, boot ROM/bootloader, a bootable Server OS, guest filesystem, guest process model, full guest TCP/IP stack, physical LAN transport, or Desktop reuse of the shared server core.

## Server Manager and persistent storage

The standalone `cpu16-server` host program is the first management entry point. It provisions machine records under a host-side data directory (`cpu16-server-data`) and creates an opaque CPU16 disk image for each new VM. The manager can list, create and inspect machine definitions. Machine records contain a machine ID, display name, CPU model, RAM capacity, cycle-slice budget and virtual disk capacity.

The disk image format has a 512-byte header and a raw sector area. Guest-facing storage is represented by a block-device interface; the host path is used only by the emulator to persist opaque sector bytes. The guest OS must implement its own partition/filesystem format on top of those sectors. Reads and writes are bounded by the image's sector count, and the image header is versioned.

This first manager is a command-line control surface, not yet the requested graphical desktop UI. It deliberately does not claim that machines can boot: the boot firmware/loader, device drivers and guest OS are still future work. A graphical CPU16 Server Manager should call the same catalog/runtime APIs rather than implement hardware itself.

## Development phases

1. **VM hardware and CPU16 bytecode core** — configurable RAM, existing compiled-bytecode execution semantics, device bus, cycle runner and tests.
2. **Virtual storage** — disk image/block-device boundary, sector I/O and compatibility tests for existing disk data.
3. **Host runtime** — headless server executable, machine configuration and device assembly.
4. **Boot and guest storage** — boot ROM/loader and a guest filesystem that runs inside the VM.
5. **Guest OS** — kernel, drivers, process/user/service managers and guest shell.
6. **Guest networking** — attach the existing network adapter to each VM, then implement guest-side IPv4, ICMP, UDP/TCP and server services.
7. **LAN transport** — portable host socket transport and explicit network configuration; test between separate processes and physical hosts.
8. **Desktop unification** — move Desktop to the same CPU core and guest-facing contracts; do not couple the server kernel to desktop UI code.

## Compatibility and safety rules

- Keep the server VM headless-capable.
- Never share C++ object pointers across process boundaries; use a versioned byte protocol for IPC.
- Treat all LAN packets as untrusted input. Add size limits, validation, timeouts and opt-in listening before exposing a service.
- Do not silently bridge virtual traffic to the physical LAN.
- Do not label host-side shell commands or service placeholders as guest OS functionality.
- Keep the main desktop emulator entry point unchanged until the shared-core integration is separately reviewed.
