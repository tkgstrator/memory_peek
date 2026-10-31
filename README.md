# memory_peek

`memory_peek` is an experimental, dedicated Atmosphère sysmodule for reading
the currently running application process through an already attached
`dmnt:cht` session. It listens on TCP port 6000 and offers a small read-only
subset of the sys-botbase line protocol. It has no RAM write, controller,
freeze, filesystem, or generic command endpoint. Its only `dmnt:cht` requests
are HasCheatProcess, GetCheatProcessMetadata, and ReadCheatProcessMemory.

**Status:** Cross-compiled and statically audited only. No Switch boot,
Atmosphère 1.12.1 runtime, LAN, or game read has been tested. Do not deploy
until the target SD card is inventoried, the provisional Program ID is checked
for collision, and a direct recovery path is ready. An NSP file on the SD card
does not prove that it booted or can read a game.

The module does not attach to a running game. It can read only when Atmosphère's
`dmnt` cheat manager already has an active session for that game. An empty
cheat file at
`atmosphere/contents/<16-hex-title-id>/cheats/<16-hex-build-id-prefix>.txt`
can arrange this at game launch without cheat opcodes. The filename uses the
**first eight bytes** of the main NSO build ID (16 hexadecimal digits), as
documented by [Atmosphère 1.12.1](https://github.com/Atmosphere-NX/Atmosphere/blob/1.12.1/docs/features/cheats.md).
Confirm that prefix from the exact running game/build before staging the file;
do not infer it from a different binary or game version. Atmosphère's cheat
enable key is checked at launch (default: **L is not held**); holding L can
leave the reader unavailable even with the empty file. If the session is
unavailable, the module returns `ERR READER_UNAVAILABLE` without attaching or
stopping the game. The launch-time behavior of this arrangement remains
unverified on the target device.

## Wire protocol

Send one ASCII command per LF- or CRLF-terminated line. A response is one CRLF-
terminated line. The server handles one client at a time and disconnects an
idle client after five seconds. `getVersion` returns `1.0.0`; `getIdentity`
returns `memory_peek/1`.

| Command | Response |
| --- | --- |
| `getTitleID` | 16 uppercase hexadecimal digits |
| `getMainNsoBase` | 16 uppercase hexadecimal digits |
| `getBuildID` | first eight build ID bytes, 16 uppercase hexadecimal digits |
| `getHeapBase` | 16 uppercase hexadecimal digits |
| `isProgramRunning 0x<TITLE_ID>` | `1` or `0` for the current application |
| `peekMain 0x<OFFSET> 0x<LENGTH>` | uppercase hex bytes |
| `peekAbsolute 0x<ADDRESS> 0x<LENGTH>` | uppercase hex bytes |
| `peekAbsoluteMulti 0x<ADDRESS> 0x<LENGTH> ...` | concatenated uppercase hex bytes in request order |

Each line is limited to 2048 bytes, each request to 32 ranges and 1 MiB total.
Numbers accept decimal or `0x`-prefixed hexadecimal. A malformed or unknown
command returns `ERR INVALID_REQUEST`; missing game, failed read, and detected
game switch return `ERR NO_GAME`, `ERR READ_FAILED`, and `ERR GAME_CHANGED`.
An unattached `dmnt` session returns `ERR READER_UNAVAILABLE`.
The complete read is buffered before success bytes are sent. There is no
authentication or encryption: any client on the same reachable network can
request game memory while the service is running.

## Build and check

Host tests need a C compiler and Python 3:

```sh
make test
```

The pinned, locally cached devkitPro image can cross-compile and audit the
package. It is the image digest used for the current static build:

```sh
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work -w /work \
  devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282 \
  make switch
```

The outputs are `build/switch/memory_peek.elf`, `.nso`, `.npdm`, and `.nsp`.
The current hashes and check results are recorded in `BUILD-MANIFEST.md`.
`make switch` extracts the generated NPDM permissions with `hactool`, checks
the exact syscall and service allowlists, checks filesystem permissions,
scans source and ELF symbols for forbidden APIs and ungranted SVCs, and
confirms that the audited NPDM and NSO are the exact two members inside the
NSP. The audit rejects direct debug attach/read/write SVCs (0x60/0x6a/0x6b),
target-process mutation capabilities, additional kernel capability categories,
and mutating or attach `dmnt:cht` calls in source.
These are static checks; they do not establish runtime compatibility or
confidentiality.

The provisional Program ID is `0x43004D5045454B01`. Confirm it is unused on
the target SD before installation. This project does not include an installer
or modify an SD card. The device inventory, rollback, and actual boot/read
validation are separate deployment gates documented in
`docs/superpowers/specs/2026-10-31-memory-peek-design.md`.

## Provenance and license

This repository's implementation is written for this project and licensed
under GPL-3.0-only; see `LICENSE`. The command vocabulary and response shapes
were checked against the existing NXCTF read client and the GPL-3.0
sys-botbase 2.5 source snapshot used for protocol research. The metadata
layout and three IPC command IDs were checked against Atmosphère 1.12.1's
published `dmnt:cht` interface. No sys-botbase or Atmosphère source file was
copied into this repository; the IPC calls are implemented here against libnx.
The build uses libnx and devkitA64 from devkitPro; their upstream licenses and
support terms remain their own.
