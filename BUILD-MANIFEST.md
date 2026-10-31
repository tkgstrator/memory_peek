# Local build manifest — 2026-10-31

This is a development artifact built from the current local tree. If committed,
its source revision is the commit containing this manifest. Regenerate the
build and hashes after any source, configuration, or toolchain change. Nothing
here records a Switch installation or a successful runtime read.

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

`make test` passed the C parser test and thirteen Python audit/socket tests.
`make switch` cross-compiled the package and passed `tools/audit.py`: the NSP
contains exactly the audited NSO and NPDM, the NPDM has no filesystem access
or wildcard service grants. It excludes direct debug attach/read/write SVCs
(0x60/0x6a/0x6b) and process mutation syscalls. The source uses only three
read-only `dmnt:cht` IPC commands and does not attach to a running game.
A separately generated unsafe NPDM with 0x6b was rejected by the audit policy.

The NSP SHA-256 above identifies this local build. The first GitHub Actions
build from the same source packaged the identical NSO and NPDM in the opposite
PFS0 entry order, so its NSP hash differs. Each release generates its own
`SHA256SUMS` from the audited NSP it actually publishes; use that file to
verify a downloaded release asset.

Before a deployment, verify the live SD has no Program ID collision and
complete the inventory, backup, recovery, boot, and real read gates in the
design spec. Static build results do not establish compatibility with
Atmosphère 1.12.1 or the target game.
The empty cheat file at
`atmosphere/contents/<title-id>/cheats/<first-eight-build-id-bytes>.txt`
needed for a launch-time `dmnt` session has not been staged or tested. Its
title ID and 16-digit build ID prefix require live confirmation.
Atmosphère's launch-time cheat-enable key must also permit attachment; its
default is L not held.
