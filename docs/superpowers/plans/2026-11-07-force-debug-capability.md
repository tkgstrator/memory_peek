# Running-process debug capability fix — implementation plan

> For an agentic implementer: use test-driven development and verify the real
> packaged NPDM before hardware deployment.

## Evidence and scope

On the active Switch, `getDiagnostics` returned `pm_rc=0`, PID `0x88`, and
`attach_rc=0xFA01`. The active NSP SHA-256 equals the local startup-retry build
`cf255e1525c3d455171ed1fe2198a5c35bc9aec7370f9833afa864e56c084817`.
Its ACID and ACI0 debug capability words are both `0x0004FFFF`
(`ForceDebugProd`). Atmosphère 1.12.1's `KDebugBase::Attach` returns
`ResultInvalidState` for a created or running target when the debugger has
`ForceDebugProd`. A probe built with `force_debug: true` instead has raw word
`0x0008FFFF`. Atmosphère's default kernel debug mode is enabled.

Change only the debug capability, artifact audit, tests, and current docs. Keep
the read-only API and exact SVC/service allowlists. Do not add cheat files,
write/control calls, or boot configuration changes.

## Task 1: Enforce the correct raw NPDM capability

Files: `tests/test_audit.py`, `tools/audit.py`, `memory_peek.json`.

1. Add failing tests for a synthetic NPDM with ACID and ACI0 capability arrays:
   both `0x0008FFFF` pass; old `0x0004FFFF`, mixed copies, missing/duplicate
   descriptors, combined flags, malformed magic, and out-of-range/truncated
   arrays fail. Add a test requiring the three source JSON debug booleans to be
   false/true/false in allow/force/prod order. Run the focused tests and observe
   the expected failure before implementation.
2. Parse META's ACI0 and ACID section offsets/sizes, then each section's
   kernel-capability offset/size. Bounds-check them and require exactly one
   debug descriptor in each array, equal to `0x0008FFFF`. The descriptor ID
   occupies the low 17 bits (`0x0FFFF` ID field plus bit 16 zero). Use the raw
   check as the authority; keep hactool for the other metadata fields, without
   relying on its version-dependent debug flag names.
3. Change `memory_peek.json` to `force_debug: true`,
   `force_debug_prod: false`, `allow_debug: false`. Run focused tests, then all
   host tests.

## Task 2: Build, document, and verify

Files: `README.md`, `README.ja.md`, `BUILD-MANIFEST.md`,
`BUILD-MANIFEST.ja.md`; historical design documents may be clearly marked
superseded rather than rewritten.

1. Update the current documentation to describe the raw `ForceDebug`
   capability, kernel debug-mode dependency, wider caller capability limited
   by the PID-only code path, and brief target suspension during attach.
   Remove stale instructions for empty cheat files.
2. Cleanly rebuild with the pinned devkitPro image, run `make switch` and the
   audit, and verify raw `0x0008FFFF` in both packaged NPDM sections. Record
   exact artifact hashes and test counts in the manifests. Confirm the NSO hash
   is unchanged by this metadata-only source change.
3. Obtain an independent read-only diff review. Resolve material findings and
   rerun affected checks.
4. Back up the currently deployed NSP, stage the audited NSP via the active
   Switch's FTP, compare transferred SHA-256, then ask the user for one full
   console reboot. On the running Splatoon 3 process, require `attach_rc=0`,
   title/build/main/heap metadata, and one 16-byte `peekMain` read. Verify the
   game remains responsive. Report any later-stage error separately.

## Review focus

- ACID and ACI0 offsets differ and must be interpreted relative to the correct
  section start.
- Reject duplicate or combined debug flags even if one good word is present.
- Hactool's displayed flag names must not override the raw capability audit.
- No write, control, or cheat permission may enter the NSP.
- A successful attach alone does not prove metadata lookup or memory reading.
