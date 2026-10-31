# memory_peek Implementation Plan

[日本語版はこちら](2026-10-31-memory-peek.ja.md)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a dedicated read-only game-memory sysmodule, then prepare a reversible Atmosphère 1.12.1 SD configuration once the target device is accessible.

**Architecture:** A compact Switch C sysmodule listening on TCP port 6000 accepts only the read requests listed in the design spec, matching NXCTF read-client response formats. Protocol parsing is implemented in pure C and verified with host unit tests; socket integration tests use Atmosphère's `dmnt:cht` metadata and memory-read IPC only when `dmnt` attached prior to game start. Memory reads never attach to or pause a running game. SD staging is a separate operational phase requiring live device inventory and the target game's confirmed build ID.

**Tech Stack:** C, libnx, devkitA64 Docker image, Python verification suites, Atmosphère 1.12.1.

**Spec:** `docs/superpowers/specs/2026-10-31-memory-peek-design.md`

## Global Constraints

- Expose no endpoints for RAM writes, controller inputs, process freezing, filesystem mutation, or arbitrary command execution.
- Restrict request lines to 2,048 bytes, at most 32 ranges, and 1 MiB total, guarding against integer overflow; explicitly reject unknown commands.
- Use provisional Program ID `0x43004D5045454B01`, `getVersion=1.0.0`, and `getIdentity=memory_peek/1`; match spec-defined NXCTF read commands and response formats.
- Keep official Atmosphère 1.12.1 downloads unmodified and verify their published SHA-256 checksums.
- Never treat SD file staging as proof of runtime execution; target device identity and recovery paths must be established prior to deployment.

## Review Focus

- Any `poke` or `freeze` request returns an explicit rejection line and performs neither memory reads nor writes.
- Oversized and malformed request lines cannot overflow parser buffers.
- Address-plus-length arithmetic overflow and oversized read requests fail cleanly before dispatching a `dmnt:cht` IPC call.
- Memory read failures never emit partial byte streams framed as success.
- The compiled NPDM excludes direct target-process debug/write capabilities and grants only required services and local syscalls; source and binary artifacts contain no references to write, pause, freeze, or controller APIs. Since `dmnt:cht` is a broader IPC service, audit the exact methods invoked and document the trust boundary clearly.

---

### Task 1: Bounded read-only command protocol

**Files:** Create `source/protocol.c`, `source/protocol.h`, `tests/test_protocol.c`, `source/server.c`, `source/server.h`, `tests/test_server.py`, and `Makefile` host test targets.

**Interfaces:** `parse_request(const char *line, size_t length, struct request *out)` returns an operation code and bounded numeric arguments or an error; only identity, metadata, and memory read operations are recognized.

- [x] Write host tests for accepted read commands and exact response shapes; explicit rejection of writes and controller commands; malformed, fragmented, and oversized lines; idle disconnect; size/range limits; overflow; no game; mid-read failure and game switch. The socket test uses a fake memory backend.
- [x] Run host tests and observe expected failures prior to implementation.
- [x] Implement the parser and verify all host tests pass green.

### Task 2: Switch read adapter and package

**Files:** Create `source/main.c`, `source/memory.c`, `source/memory.h`, `memory_peek.json`, build `Makefile`, `README.md`, and `LICENSE` as needed.

**Interfaces:** A single TCP service accepts the Task 1 protocol, reads current application metadata and bounded memory ranges, and returns one complete response per request.

- [x] Add a build artifact check that rejects any unapproved NPDM kernel capability, direct target-process debug/write/process-control syscall, unexpected service grant, or source/binary reference to write, pause, freeze, or controller methods; prove the audited NPDM is inside the NSP.
- [x] Confirm the check fails on a deliberately unsafe fixture, then make the minimal module and NPDM satisfy it.
- [x] Build the NSP with pinned cached devkitPro image digest `sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`; inspect generated NPDM/ELF and run host and socket tests again. Check that a read with no dmnt handle returns unavailable without any attach or pause call. Check pre-launch attachment and continuous reading on the actual device before claiming runtime success.

### Task 3: Integration and deployment readiness

**Files:** Create a release manifest and SD staging guide in this repository; make any required host client adaptation in NXCTF only after its protocol behavior is verified.

- [ ] Record source revision, toolchain image, NSP/NPDM hashes, program ID, port, supported commands, and static verification results.
- [ ] Inventory the intended Switch and SD (MAC, firmware, Atmosphère, fusee/Hekate boot path, `contents`, `exefs_patches`, `kip_patches`, `nro_patches`, FTP/sys-botbase modules, boot2 flags, recovery path) before any device write.
- [ ] Stage a reversible change that disables stock sys-botbase, game overrides, external IPS, and HBL while retaining FTP and memory_peek. For the selected title/build, stage only an empty cheat file if needed for dmnt's pre-launch handle, and confirm its contents contain no opcodes.
- [ ] After controlled reboot, verify boot, Album, FTP, stock sys-botbase absence, and one actual read. Record any unverified runtime condition precisely.
