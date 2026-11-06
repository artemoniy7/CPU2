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

## CPU and disk compatibility

The intended guest ISA is the existing CPU2 instruction set. Do not invent opcodes, registers, interrupt numbers or I/O ports. Before implementing the VM CPU, inspect and extract the existing CPU execution model from the legacy emulator source into a reusable core. The existing virtual disk format is a compatibility candidate, but it must be inspected and given a documented block-device boundary before being reused.

## Development phases

1. **Architecture and network contract** — this change: document boundaries and provide a deterministic in-process virtual switch.
2. **LAN transport** — portable host socket transport and explicit network configuration; test between separate processes and physical hosts.
3. **VM hardware** — CPU core extraction, configurable RAM, bus and virtual devices.
4. **Boot and guest storage** — disk-image block device, bootloader and guest filesystem.
5. **Guest OS** — kernel, drivers, process/user/service managers and guest shell.
6. **Guest networking** — guest-side IPv4, ICMP, UDP/TCP and server services.
7. **Desktop integration** — CPU16 Desktop becomes a guest network client; do not couple the server kernel to desktop UI code.

## Compatibility and safety rules

- Keep the server VM headless-capable.
- Never share C++ object pointers across process boundaries; use a versioned byte protocol for IPC.
- Treat all LAN packets as untrusted input. Add size limits, validation, timeouts and opt-in listening before exposing a service.
- Do not silently bridge virtual traffic to the physical LAN.
- Do not label host-side shell commands or service placeholders as guest OS functionality.
- Keep the main desktop emulator entry point unchanged until the shared CPU-core extraction is a separately reviewed change.
