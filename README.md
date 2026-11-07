# memory_peek

[日本語版はこちら](README.ja.md)

`memory_peek` is an experimental, dedicated Atmosphère sysmodule that reads the memory of the currently running application process through a short-lived, read-only debugger attach.

It listens on TCP port 6000 and provides a lightweight, read-only subset of sys-botbase's text command interface (one command per line). Existing tools designed for sys-botbase (such as NXCTF clients) can connect and read memory out of the box. All memory-write operations, controller simulations, process freezing, pausing, filesystem access, and generic command endpoints are omitted, and no cheat service or API is used.

**Status:** Cross-compilation and static audit pass. Hardware deployment is underway, but a successful in-game memory read has not yet been verified. Before installation on another Switch, inventory the target SD, check that the provisional Program ID is unused, and prepare a rollback. An NSP on the SD alone does not establish runtime compatibility.

For each request that needs the game, the module asks `pm:dmnt` for the application process ID (command 4), calls `svcDebugActiveProcess`, and reads with `svcReadDebugProcessMemory`. The main module base and build ID come from `ldr:dmnt` `GetProcessModuleInfo` (module[1] when exactly two modules are reported, otherwise module[0], as sys-botbase does), the title ID from `pm:info` `GetProgramId`, and the heap base from `svcGetInfo(HeapRegionAddress)`. The debug handle is closed with `svcCloseHandle` on every failure path and, on success, before any network send or hex encoding; the server never holds the attach while it waits on a socket. Attaching a debugger can still disturb a title that detects debuggers, and while attached the game is subject to the kernel's debug-event semantics; only the attach/read/close calls above are made, with no write, control, query, or pause calls. With the pinned devkitPro toolchain, the JSON input uses `force_debug_prod` true and `force_debug` false; hactool reports the resulting effective `force_debug` flag as true. `allow_debug` stays false. If the process exists but any attach or lookup step fails, the module returns `ERR READER_UNAVAILABLE`.

## Command interface and protocol

Commands are sent as plain ASCII text lines terminated by LF or CRLF. Each response is a single line terminated by CRLF. You can interact with it programmatically or test it directly from a terminal using tools like `nc` (netcat).

The server processes one client connection at a time and automatically disconnects an idle client after a five-second timeout. `getVersion` returns `1.0.0`; `getIdentity` returns `memory_peek/1`.

| Command | Response |
| --- | --- |
| `getTitleID` | 16 uppercase hexadecimal digits |
| `getMainNsoBase` | 16 uppercase hexadecimal digits |
| `getBuildID` | First eight build ID bytes as 16 uppercase hexadecimal digits |
| `getHeapBase` | 16 uppercase hexadecimal digits |
| `getDiagnostics` | One line of fixed-order `key=value` hex fields for the read-only probe: `pm_rc`, `pid`, `attach_rc`, `ldr_rc`, `modules`, `info_rc`, `heap_rc`, `title`, `main`, `heap`, `ldr_init_rc`, `info_init_rc`, `ldr_ready`, `info_ready` (see below) |
| `isProgramRunning 0x<TITLE_ID>` | `1` or `0` indicating whether the specified title is currently running |
| `peekMain 0x<OFFSET> 0x<LENGTH>` | Uppercase hexadecimal bytes starting from Main NSO base + offset |
| `peekAbsolute 0x<ADDRESS> 0x<LENGTH>` | Uppercase hexadecimal bytes from absolute virtual memory address |
| `peekAbsoluteMulti 0x<ADDRESS> 0x<LENGTH> ...` | Concatenated uppercase hexadecimal bytes in requested order |

Lines are limited to 2,048 bytes; peek requests are limited to at most 32 ranges and 1 MiB in total. Numeric arguments accept decimal values or `0x`-prefixed hexadecimal. Malformed or unrecognized commands return `ERR INVALID_REQUEST`. Missing games, failed reads, and detected game changes return `ERR NO_GAME`, `ERR READ_FAILED`, and `ERR GAME_CHANGED`, respectively. A running application that cannot be attached or inspected returns `ERR READER_UNAVAILABLE`.

`getDiagnostics` does not require a running game. It runs the same probe, closes the debug handle, and reports raw 32-bit results (`pm_rc`, `attach_rc`, `ldr_rc`, `info_rc`, `heap_rc`; 8 hex digits) plus `pid`, `title`, `main`, `heap` (16 hex digits) and `modules` (hex count). Steps that were not reached, or that failed, report 0 for their values; no attach is attempted unless `pm_rc` is 0 and `pid` is nonzero.

`ldr:dmnt` and `pm:info` are initialized on demand after the listener starts; each failed initialization is retried separately on later requests, and successful services are never initialized twice. `ldr_init_rc` and `info_init_rc` are the raw results of the latest initialization attempt (`FFFFFFFF` if never attempted) and `ldr_ready` / `info_ready` are `1` when that service is usable. While either is not ready, no debug attach or `ldr:dmnt`/`pm:info` call is made, and requests that need a running game return `ERR READER_UNAVAILABLE` when an application is running. `pm:dmnt` and BSD socket initialization are retried for up to 30 seconds during early boot. If the listening socket cannot be created, bound or put in listen state, it is retried with a bounded delay (0.5 s up to 5 s) instead of exiting.

The entire requested memory read is buffered in full, and the debug attach is released, before any success bytes are transmitted. There is no authentication or encryption: any client with reachable network access to the port can query application memory while the service is running.

## Build and check

Host unit tests require a C compiler and Python 3:

```sh
make test
```

The pinned, locally cached devkitPro Docker image can cross-compile and audit the package. This matches the exact image digest used for the static release build:

```sh
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work -w /work \
  devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282 \
  make switch
```

The build artifacts are `build/switch/memory_peek.elf`, `.nso`, `.npdm`, and `.nsp`. Current hashes and verification results are documented in `BUILD-MANIFEST.md`.

`make switch` extracts generated NPDM permissions via `hactool`, verifies strict syscall and service allowlists, checks for zero filesystem permissions, scans source files and ELF symbols for forbidden APIs and ungranted SVCs, and confirms that the audited NPDM and NSO are the exact two members packaged inside the NSP. The audit allows exactly SVCs `0x60` (DebugActiveProcess) and `0x6A` (ReadDebugProcessMemory) beyond the base set, services `pm:dmnt`, `ldr:dmnt`, `pm:info` and `bsd:u`, and effective hactool debug flags `force_debug` true / `allow_debug` false. It rejects write/control debug SVCs, other process-mutation capabilities, additional kernel capability categories, any cheat service or API reference, and any service dispatch other than the `pm:dmnt` application-PID probe. These are static checks; they do not establish runtime hardware compatibility or communication confidentiality.

The provisional Program ID is `0x43004D5045454B01`. Confirm it is unused on the target SD card before installation. This project does not provide an installer or modify an SD card automatically. Device inventory, backup/rollback preparation, and hardware boot/read verification are separate deployment gates documented in `docs/superpowers/specs/2026-10-31-memory-peek-design.md`.

## CI and releases

[`integration.yaml`](.github/workflows/integration.yaml) checks C and Python formatting on pushes to `master` and pull requests. It also checks the commit messages in each pull request with commitlint. It does not build or publish the Switch package.

[`deployment.yaml`](.github/workflows/deployment.yaml) triggers on pushed `vX.Y.Z` tags whose commits belong to `master`. It runs host tests, performs a fresh Switch cross-compilation and artifact permission audit, validates the transferred NSP checksum, and publishes the NSP and `SHA256SUMS` to a GitHub Release. Re-running a tag on an existing release intentionally fails rather than overwriting release assets. The GitHub release version is independent of `getVersion`, which identifies the wire protocol version. This workflow publishes build assets to GitHub; it does not deploy files to a Switch or bypass the device verification steps described above.

## Provenance and license

This repository's implementation was written specifically for this project and is licensed under GPL-3.0-only; see `LICENSE`. The command syntax and response shapes were verified against the existing NXCTF read client and the GPL-3.0 sys-botbase 2.5 source snapshot used for protocol research. The module-selection rule follows sys-botbase; the `pm:dmnt` application-PID command ID was verified against Atmosphère 1.12.1. No source files from sys-botbase or Atmosphère were copied into this repository; IPC calls are implemented directly against libnx. The build system utilizes libnx and devkitA64 from devkitPro, whose respective upstream licenses and terms apply.
