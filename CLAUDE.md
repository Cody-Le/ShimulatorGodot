# Space FSW Simulation (Shimulator) — Claude Code Context

## Project
Flight software simulation platform — "Unity for FSW." Intercepts Linux hardware
syscalls at the kernel level, routes them to a Godot C++ GDExtension backend over
TCP, letting real unmodified FSW binaries run against virtualized hardware. Built
by Cody Le, Flight Software Co-Lead at SSRL (Small Satellite Research Laboratory),
University of Georgia.

**V1 (LD_PRELOAD shim) is frozen legacy. V2 (custom Linux kernel driver) is the
sole active target. Never touch or extend v1.**

## Architecture (v2)
- Custom in-tree kernel driver (`drivers/sim/`) compiled into a `bzImage`, runs in
  a QEMU VM (Ubuntu 22.04 userspace, persistent `jammy-resized.img`)
- Driver registers native kernel subsystems: `gpiochip_add_data`, `uart_add_one_port`,
  `spi_register_controller`, plus 1-Wire sysfs and V4L2 via `video_register_device`
- Operation callbacks forward over TCP to Godot host at `10.0.2.2:7777` (QEMU SLIRP
  user networking gateway)
- Godot project: `godot-fsw-tcp` / `libsim`, C++ GDExtension backend

## Wire Protocol (D049 + D050 — authoritative)
Outer header: `simcall_header_t`, 24 bytes, **little-endian / native byte order**,
`version = 4`. Fields: `version(u16)`, `cmd_id(u8)`, `type(hdwi_type_t u8)`,
`device_index(u8, retired/0)`, `reserved[3]`, `time_ns(u64 @offset 8)`, `data_len(u32)`,
4 bytes implicit trailing pad. Struct is NOT packed.

Per-type natural identifiers live in the **inner** peripheral header (D050):
- GPIO: `chip_index(u8)` + `action(u8)`
- SPI: `bus_index(u8)` + `chip_select(u8)` + `action(u8)` + `_pad`
- UART: `port_index(u8)` + `action(u8)`
- 1-Wire: `sensor_index(u8)` + `action(u8)`

All inner header structs are `__packed`. CMD_SYNC is time-seeding only; no
device-registration handshake needed (D050 eliminates it).

## Active Peripheral Set (summer scope)
GPIO, UART (HASP gondola downlink/uplink at 4800 baud 8N1), SPI (MS5803 pressure
sensors), 1-Wire (DS18B20 temp sensors via sysfs), V4L2 (USB camera). I2C deferred.

## Build Environment
WSL2 (Ubuntu 24.04) on Windows host. Kernel build:
```
make KCONFIG_CONFIG=simconfig -j$(nproc)
```
Output: `kernel/arch/x86/boot/bzImage`. VM launched via `run.sh`.
Key kernel config: `CONFIG_GPIOLIB=y`, `CONFIG_GPIO_CDEV=y`, `CONFIG_GPIO_CDEV_V1=y`
(all built-in `y`, not modules).

## Raspberry Pi Target
Pi 3B+, `/boot/firmware/config.txt`:
```
dtoverlay=spi0-2cs,cs0_pin=12,cs1_pin=16
```
libgpiod v2.2 built from source at `/usr/local/lib/libgpiod.so`.

## Documentation System (REQUIRED — read before any architecture discussion)
Live LaTeX doc repo managed via FSWAutoDocServer MCP at `https://fswautodoc.fly.dev/sse`.
Always-on, no cold starts.

**At the start of every architecture conversation:**
1. Call `FSWAutoDocServer:list_docs`
2. Fetch `architecture_overview.tex`, `decisions_log.tex`, `technical_roadmap.tex`
3. Check next available D/S/Q IDs before logging anything

**Mid-conversation workflow:**
- Call edit tools as decisions are made — never queue or batch
- `add_architecture_decision` / `add_strategic_decision` / `add_open_question` for new entries
- `str_replace_in_doc` for edits — always `get_doc` first to get exact strings
- `compile_doc` after each file edit; `compile_all` as final sweep
- `git_commit_push` exactly **once** at the end of the session

**LaTeX escaping:** all `_`, `&`, `#`, `>` must be escaped in MCP tool inputs
(`\_`, `\&`, `\#`, `\textgreater`). Raw identifiers in tabularx cells break
compilation with "Extra alignment tab" errors.

**Supersession pattern:** add `\textit{[SUPERSEDED by DXX --- Month Year]}` at
start of decision cell; retain historical rationale below.

**Decision ID format:** `D`-prefix architecture, `S`-prefix strategic, `Q`-prefix
open questions. Next IDs: check `decisions_log.tex` directly.

Authoritative editing procedure: `autodoc_system.tex` (not `editing_procedure.tex`).

## Key Learned Constraints
- Kernel TCP: `kernel_connect` / `kernel_sendmsg` / `kernel_recvmsg` with `struct kvec`
- `sock_create_kern` in-tree only (not loadable `.ko`)
- Kernel char devices must be heap-allocated — stack-local `cdev`/class causes kernel
  panic with `0xaa` poison bytes
- Wire structs: send header + payload as separate kvec iovecs, never `sizeof(struct)`
  (ships the pointer field)
- `kernel_recvmsg` returning 0 = EOF/disconnect, not success
- `StreamPeerTCP` must not be accessed from two threads simultaneously (D048)
- `gc->label` for GPIO chip identity; `gc.base = -1` for auto-assigned chip number

## Godot-Side (GDExtension / C++)
- Working directory: `d:\Documents\Programming\SSRL\GodotShimulator`
- GDExtension source under `src/`, SCons build system
- `src/HDWI/` — hardware device interface layer (GPIO, SPI, UART, etc.)
- `src/IPC/` — inter-process communication (packet encoding, TCP transport)
- Active branch: `spi` — SPI peripheral support in progress

## Style / Approach
Direct and technically dense. YAGNI discipline — defer generalizations until concretely
needed. Flag gaps and ambiguities before committing documentation. Ship-first decisions.
No unnecessary comments in code; only add when the WHY is non-obvious.
