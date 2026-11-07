# Local build manifest — 2026-11-07

[日本語版はこちら](BUILD-MANIFEST.ja.md)

This manifest records the audited local package built from the current source tree. The package was staged on the active Switch SD and loaded after a full console reboot. The previously installed package (`cf255e1525c3d455171ed1fe2198a5c35bc9aec7370f9833afa864e56c084817`) reached the TCP listener but returned `attach_rc=0xFA01` for a running Splatoon 3 process; its raw debug capability was `ForceDebugProd` (`0x0004FFFF`). The new package changes that capability to `ForceDebug` (`0x0008FFFF`).

| Item | Value |
| --- | --- |
| Toolchain image | `devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282` |
| Program ID | provisional `0x43004D5045454B01` |
| TCP port / identity | `6000` / `memory_peek/1` |
| NSP SHA-256 | `1b383be72aaa3fda197a83cd1f2cf760033b89b326899c86a10d83e31931ed93` |
| NPDM SHA-256 | `4cbed322a3028ae336cc45706fadf3a4b82cb976863f725468d5031ca85178cf` |
| NSO SHA-256 | `18c5c1b23201bbe95403740c6b6a476bf71fb06a2c2633ef6c59b0642d7ae533` |

`make test` passed two C test binaries and 24 Python tests. `make switch` in the pinned image cross-compiled the package and passed `tools/audit.py`. The audit verified:

- The NSP contains exactly the audited NSO and NPDM.
- Both ACID and ACI0 kernel capability arrays contain exactly one raw `ForceDebug` descriptor (`0x0008FFFF`), with no `ForceDebugProd` or combined debug flags.
- The exact SVC and service allowlists, zero filesystem permissions, source and ELF symbol restrictions, and no cheat, write, or debugger-control APIs.
- The NSO SHA-256 is unchanged from the preceding startup-retry package; only NPDM permission metadata changed.

The active SD's `atmosphere/package3` SHA-256 matches the official Atmosphère 1.12.1 release package. After the reboot, the active NSP hash matched this manifest. On running Splatoon 3, `getDiagnostics` returned `attach_rc=0`, title ID `0100C2500FC20000`, and nonzero main and heap bases. `getBuildID` returned `28C4287AEE36F749`, and `peekMain 0x0 0x10` returned 16 bytes. The user confirmed that the game remained responsive and controllable after the read. Keep the previous NSP backup for rollback. No cheat file is required or permitted for this build.
