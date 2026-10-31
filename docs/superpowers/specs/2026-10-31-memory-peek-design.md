# memory_peek design

The user wants an Atmosphère 1.12.1 setup that runs only the required FTP and game-memory reading custom sysmodules. Album must open normally, game LayeredFS and external ExeFS/IPS modifications must be disabled, and the stock sys-botbase must not run. The target Switch and SD card are not currently accessible, so this repository first delivers a locally built, reviewable memory reader; installation and runtime claims require a later verified SD inventory and device test.

## memory_peek boundary

- A dedicated Switch sysmodule reads the currently running application process memory and exposes only bounded read and read-only metadata operations over TCP. It has no controller, file-write, RAM-write, freeze, or generic command execution operations.
- Use TCP 6000 after disabling stock sys-botbase so existing NXCTF read clients can connect. Keep their read command spellings and uppercase hexadecimal line responses: `getVersion`, `getTitleID`, `getMainNsoBase`, `getBuildID` (first 8 bytes), `getHeapBase`, `isProgramRunning`, `peekMain`, `peekAbsolute`, and `peekAbsoluteMulti`. `getVersion` returns the numeric protocol version `1.0.0`; `getIdentity` returns `memory_peek/1` to identify the actual module. Every other command receives an error line.
- Accept at most 32 ranges and 1 MiB total per request, matching the NXCTF client bound. The complete read must succeed before any data line is sent. Cap the line at 2048 bytes and apply an idle timeout. One client is handled at a time.
- Use provisional program ID `0x43004D5045454B01` and reject deployment if the live SD already uses it. This ID is distinct from stock sys-botbase; the final collision check is a deployment gate.
- Read through Atmosphère 1.12.1's `dmnt:cht` metadata and memory-read IPC only when dmnt has already attached at application launch. The reader must never attach a debugger, call `ForceOpenCheatProcess`, or call pause, resume, write, freeze, or cheat-toggle IPC methods. If dmnt is not attached, return an unavailable error without changing the running game. For a chosen title/build, an empty cheat file can make dmnt attach before the game starts; it contains no opcodes and must be verified on the device. A read must not stop a running game.
- Grant only the services and local-process syscalls needed by the adapter. Exclude all direct target-process debug, write, and process-control syscalls from the NPDM. `dmnt:cht` itself exposes write methods to clients, so this is a source and binary enforced read-only command surface, not a kernel-enforced restriction on that service. The artifact audit must reject unrecognized kernel-capability entries, additional services, and write/pause/freeze call references.
- The wire parser must reject unknown commands, malformed numbers, oversized lines, oversized reads, and arithmetic overflow without side effects. A failed read returns a clear error, never partial bytes presented as success.
- Keep source provenance and licenses explicit. Do not copy sys-botbase source without GPL-3.0 attribution. TCP 6000 exposes game memory to other clients on the same LAN; do not claim confidentiality or authentication.

## SD integration boundary

- Keep the official Atmosphère ZIP and fusee binary unchanged as verified inputs. A ZIP overlay alone leaves old `contents` and `exefs_patches` active.
- After live inventory, back up existing SD files, retain only confirmed FTP and memory_peek custom sysmodules under active `atmosphere/contents`, and move external game overrides and `exefs_patches` outside active paths. A narrowly scoped, empty per-title/build cheat file may be needed to establish dmnt's pre-launch read handle; it is not a LayeredFS or ExeFS override and must contain no opcodes.
- Use explicit `override_config.ini` entries to stop Album and app HBL override. Keep a direct SD recovery path because an FTP failure after reboot would otherwise prevent remote rollback.
- Validate archive hashes, resulting SD layout, actual boot, Album, FTP, and one real read from the intended game. File readback alone is not runtime activation.
