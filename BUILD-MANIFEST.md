# Local build manifest — 2026-10-31

[日本語版はこちら](BUILD-MANIFEST.ja.md)

This is a development artifact built from the current local tree. If committed, its source revision corresponds to the commit containing this manifest. Regenerate the build and hashes after any change to source files, configuration, or toolchains. Nothing recorded here confirms a successful Switch installation or live runtime execution.

| Item | Value |
| --- | --- |
| Toolchain image | `devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282` |
| Program ID | provisional `0x43004D5045454B01` |
| TCP port | `6000` |
| Protocol version | `1.0.0` |
| Identity | `memory_peek/1` |
| Commands | `getVersion`, `getIdentity`, `getTitleID`, `getMainNsoBase`, `getBuildID`, `getHeapBase`, `isProgramRunning`, `peekMain`, `peekAbsolute`, `peekAbsoluteMulti` |
| NSP SHA-256 | `f6ec62c47ed4ae658a1c2643105dea89227510dbd4c7e9836af15efb83d8b0e7` |
| NPDM SHA-256 | `b5894994a68c2544265b994f507b00455d3e7989f965586ba8acefb57f9ea245` |
| NSO SHA-256 | `05caefd612357906615a17d567c7bab15a93e60e65a544befed4c45c5e0083c0` |

`make test` successfully passed the C wire protocol parser test suite and 13 Python audit/socket tests.
`make switch` cross-compiled the package and passed `tools/audit.py`:
- The NSP contains exactly the audited NSO and NPDM members.
- The NPDM contains no filesystem permissions or wildcard service access grants.
- Direct debug attach/read/write SVCs (`0x60`/`0x6A`/`0x6B`) and process mutation syscalls are excluded.
- The source code invokes only three read-only `dmnt:cht` IPC commands and never attaches to a running process.
- A separately generated unsafe NPDM containing SVC `0x6B` was explicitly rejected by the audit policy.

The NSP SHA-256 hash above identifies this specific local build. The initial GitHub Actions build from the same source packaged the identical NSO and NPDM in the opposite PFS0 entry order, resulting in a distinct NSP archive hash. Each release generates its own `SHA256SUMS` from the actual audited NSP it publishes; verify downloaded release assets against that checksum file.

Before deploying to hardware, verify that the live SD card has no Program ID collisions and satisfy the inventory, backup, recovery, boot, and live-read gates detailed in the design specification (`docs/superpowers/specs/2026-10-31-memory-peek-design.md`). Static build verification does not establish runtime compatibility with Atmosphère 1.12.1 or the target game.

The empty cheat file at `atmosphere/contents/<title-id>/cheats/<first-eight-build-id-bytes>.txt` required for a launch-time `dmnt` session has not yet been staged or tested on hardware. Both the title ID and the 16-digit build ID prefix must be confirmed directly against the target game build. Furthermore, Atmosphère's launch-time cheat toggle key must permit cheat loading (by default, L is not held).
