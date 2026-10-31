# memory_peek

[日本語版はこちら](README.ja.md)

`memory_peek` is an experimental, dedicated Atmosphère sysmodule for reading the memory of the currently running application process via an existing, pre-attached `dmnt:cht` session.

It listens on TCP port 6000 and provides a lightweight, read-only subset of sys-botbase's text command interface (one command per line). Existing tools designed for sys-botbase (such as NXCTF clients) can connect and read memory out of the box. However, all memory-write operations, controller simulations, process freezing, filesystem access, and generic command endpoints are completely omitted. Its only `dmnt:cht` IPC requests are `HasCheatProcess`, `GetCheatProcessMetadata`, and `ReadCheatProcessMemory`.

**Status:** Cross-compiled and statically audited only. Switch hardware boot, Atmosphère 1.12.1 runtime execution, LAN communication, and real in-game reads have not yet been tested on a physical device. Do not deploy until the target SD card is inventoried, the provisional Program ID is verified free of collisions, and a direct recovery path is prepared. The mere presence of an NSP file on the SD card does not verify that it booted or can successfully read game memory.

The sysmodule does not attach to running games itself. It can read process memory only when Atmosphère's `dmnt` cheat manager already holds an active session for that game. Staging an empty cheat file at `atmosphere/contents/<16-hex-title-id>/cheats/<16-hex-build-id-prefix>.txt` establishes this session at game launch without executing any cheat opcodes. The filename uses the **first eight bytes** of the main NSO build ID (16 hexadecimal digits), as documented by [Atmosphère 1.12.1](https://github.com/Atmosphere-NX/Atmosphere/blob/1.12.1/docs/features/cheats.md). Always verify this prefix against the exact running title and build before staging the file; do not infer it from an unverified binary or a different game version. Atmosphère checks the cheat toggle key at launch (default: **cheats are enabled when L is not held**); holding L suppresses cheat loading, leaving the reader unavailable even if the empty cheat file is present. If no active session exists, the module returns `ERR READER_UNAVAILABLE` without attempting to attach or pausing/stopping the game. The launch-time behavior of this configuration remains unverified on hardware.

## Command interface and protocol

Commands are sent as plain ASCII text lines terminated by LF or CRLF. Each response is a single line terminated by CRLF. You can interact with it programmatically or test it directly from a terminal using tools like `nc` (netcat).

The server processes one client connection at a time and automatically disconnects an idle client after a five-second timeout. `getVersion` returns `1.0.0`; `getIdentity` returns `memory_peek/1`.

| Command | Response |
| --- | --- |
| `getTitleID` | 16 uppercase hexadecimal digits |
| `getMainNsoBase` | 16 uppercase hexadecimal digits |
| `getBuildID` | First eight build ID bytes as 16 uppercase hexadecimal digits |
| `getHeapBase` | 16 uppercase hexadecimal digits |
| `isProgramRunning 0x<TITLE_ID>` | `1` or `0` indicating whether the specified title is currently running |
| `peekMain 0x<OFFSET> 0x<LENGTH>` | Uppercase hexadecimal bytes starting from Main NSO base + offset |
| `peekAbsolute 0x<ADDRESS> 0x<LENGTH>` | Uppercase hexadecimal bytes from absolute virtual memory address |
| `peekAbsoluteMulti 0x<ADDRESS> 0x<LENGTH> ...` | Concatenated uppercase hexadecimal bytes in requested order |

Lines are limited to 2,048 bytes; peek requests are limited to at most 32 ranges and 1 MiB in total. Numeric arguments accept decimal values or `0x`-prefixed hexadecimal. Malformed or unrecognized commands return `ERR INVALID_REQUEST`. Missing games, failed reads, and detected game changes return `ERR NO_GAME`, `ERR READ_FAILED`, and `ERR GAME_CHANGED`, respectively. An unattached `dmnt` session returns `ERR READER_UNAVAILABLE`.

The entire requested memory read is buffered in full before any success bytes are transmitted. There is no authentication or encryption: any client with reachable network access to the port can query application memory while the service is running.

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

`make switch` extracts generated NPDM permissions via `hactool`, verifies strict syscall and service allowlists, checks for zero filesystem permissions, scans source files and ELF symbols for forbidden APIs and ungranted SVCs, and confirms that the audited NPDM and NSO are the exact two members packaged inside the NSP. The audit rejects direct debug attach/read/write SVCs (`0x60`/`0x6A`/`0x6B`), target-process mutation capabilities, additional kernel capability categories, and any mutating or attaching `dmnt:cht` calls in the source code. These are static checks; they do not establish runtime hardware compatibility or communication confidentiality.

The provisional Program ID is `0x43004D5045454B01`. Confirm it is unused on the target SD card before installation. This project does not provide an installer or modify an SD card automatically. Device inventory, backup/rollback preparation, and hardware boot/read verification are separate deployment gates documented in `docs/superpowers/specs/2026-10-31-memory-peek-design.md`.

## CI and releases

[`integration.yaml`](.github/workflows/integration.yaml) executes host tests, performs a fresh Switch cross-compilation, and runs the artifact permission audit on pushes to `master` and pull requests. It also uploads the audited NSP and a generated `SHA256SUMS` as a short-lived workflow artifact.

[`deployment.yaml`](.github/workflows/deployment.yaml) triggers on pushed `vX.Y.Z` tags whose commits belong to `master`. It runs the same integration checks, validates the transferred NSP checksum, and publishes the NSP and `SHA256SUMS` to a GitHub Release. Re-running a tag on an existing release intentionally fails rather than overwriting release assets. The GitHub release version is independent of `getVersion`, which identifies the wire protocol version. This workflow publishes build assets to GitHub; it does not deploy files to a Switch or bypass the device verification steps described above.

## Provenance and license

This repository's implementation was written specifically for this project and is licensed under GPL-3.0-only; see `LICENSE`. The command syntax and response shapes were verified against the existing NXCTF read client and the GPL-3.0 sys-botbase 2.5 source snapshot used for protocol research. The metadata layout and three IPC command IDs were verified against Atmosphère 1.12.1's published `dmnt:cht` interface. No source files from sys-botbase or Atmosphère were copied into this repository; IPC calls are implemented directly against libnx. The build system utilizes libnx and devkitA64 from devkitPro, whose respective upstream licenses and terms apply.
