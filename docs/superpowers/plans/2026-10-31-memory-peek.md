# memory_peek Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a dedicated read-only game-memory sysmodule, then prepare a reversible Atmosphère 1.12.1 SD change once the target device is available.

**Architecture:** A small Switch C sysmodule on TCP 6000 accepts only the read requests listed in the spec, preserving NXCTF read-client response shapes. Protocol parsing is pure C and host-tested; a socket test uses Atmosphère's `dmnt:cht` metadata and read IPC only if dmnt attached before the game started. Reads never attach or pause a running game. SD staging is a separate operation requiring the live inventory and the chosen game's build ID.

**Tech Stack:** C, libnx, devkitA64 Docker image, Python verification, Atmosphère 1.12.1.

**Spec:** `docs/superpowers/specs/2026-10-31-memory-peek-design.md`

## Global Constraints

- No RAM-write, controller, freeze, filesystem mutation, or arbitrary command execution endpoints.
- Bound requests to 2048 bytes, at most 32 ranges and 1 MiB total, and all arithmetic; reject unknown commands explicitly.
- Use provisional program ID `0x43004D5045454B01`, `getVersion=1.0.0`, and `getIdentity=memory_peek/1`; match the spec's NXCTF read commands and response formats.
- Keep the official Atmosphère 1.12.1 downloads intact and verify their published SHA-256.
- Never claim SD placement is runtime activation; device identity and recovery must be established before deployment.

## Review Focus

- A `poke` or `freeze` request returns an explicit rejection and performs no read or write.
- Oversized and malformed request lines cannot overflow the parser buffer.
- Address plus length overflow and a read beyond the maximum fail before a `dmnt:cht` read request is sent.
- A read failure cannot produce a success response containing partial data.
- The built NPDM excludes direct target-process debug/write capabilities, grants only expected services and local syscalls, and the source/binary have no write, pause, freeze, or controller call references. `dmnt:cht` is a broader IPC service, so audit the actual methods used and state that trust boundary.

---

### Task 1: Bounded read-only protocol

**Files:** Create `source/protocol.c`, `source/protocol.h`, `tests/test_protocol.c`, `source/server.c`, `source/server.h`, `tests/test_server.py`, `Makefile` host test target.

**Interfaces:** `parse_request(const char *line, size_t length, struct request *out)` returns an operation and bounded numeric arguments or an error; only identity, metadata, and memory read operations are recognized.

- [ ] Write host tests for accepted read commands and exact response shapes; explicit rejection of writes and controller commands; malformed, fragmented, and oversized lines; idle disconnect; size/range limits; overflow; no game; mid-read failure and game switch. The socket test uses a fake memory backend.
- [ ] Run the host tests and observe the expected failures before implementation.
- [ ] Implement the parser and run the complete host tests to green.

### Task 2: Switch read adapter and package

**Files:** Create `source/main.c`, `source/memory.c`, `source/memory.h`, `memory_peek.json`, build `Makefile`, `README.md`, `LICENSE` as needed.

**Interfaces:** A single TCP service accepts the Task 1 protocol, reads current application metadata and bounded memory ranges, and returns one complete response per request.

- [ ] Add a build artifact check that rejects any unapproved NPDM kernel capability, direct target-process debug/write/process-control syscall, unexpected service grant, or source/binary reference to write, pause, freeze, or controller methods; prove the audited NPDM is inside the NSP.
- [ ] Confirm the check fails on a deliberately unsafe fixture, then make the minimal module and NPDM satisfy it.
- [ ] Build the NSP with pinned cached devkitPro image digest `sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`; inspect generated NPDM/ELF and run host and socket tests again. Check that a read with no dmnt handle returns unavailable without any attach or pause call. Check pre-launch attachment and continuous reading on the actual device before claiming runtime success.

### Task 3: Integration and deployment readiness

**Files:** Create a release manifest and SD staging guide in this repository; make any required host client adaptation in NXCTF only after its protocol behavior is verified.

- [ ] Record source revision, toolchain image, NSP/NPDM hashes, program ID, port, supported commands, and static verification results.
- [ ] Inventory the intended Switch and SD (MAC, firmware, Atmosphère, fusee/Hekate boot path, `contents`, `exefs_patches`, `kip_patches`, `nro_patches`, FTP/sys-botbase modules, boot2 flags, recovery path) before any device write.
- [ ] Stage a reversible change that disables stock sys-botbase, game overrides, external IPS, and HBL while retaining FTP and memory_peek. For the selected title/build, stage only an empty cheat file if needed for dmnt's pre-launch handle, and confirm its contents contain no opcodes.
- [ ] After controlled reboot, verify boot, Album, FTP, stock sys-botbase absence, and one actual read. Record any unverified runtime condition precisely.
