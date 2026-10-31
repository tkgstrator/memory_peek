"""Audit the generated permission metadata and the actual ExeFS package."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

PROGRAM_ID = 0x43004D5045454B01
SERVICES = {"pm:dmnt", "dmnt:cht", "bsd:u"}
SYSCALLS = {0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x08, 0x09,
            0x0A, 0x0B, 0x15, 0x16, 0x18, 0x1A, 0x1B, 0x1C,
            0x1D, 0x1F, 0x21, 0x26, 0x28, 0x29, 0x52}
FORBIDDEN_APIS = {
    "svcDebugActiveProcess", "svcReadDebugProcessMemory", "svcWriteDebugProcessMemory",
    "svcBreakDebugProcess", "svcTerminateDebugProcess",
    "svcSetDebugThreadContext", "svcSetProcessMemoryPermission", "svcMapProcessMemory",
    "svcMapProcessCodeMemory", "svcTerminateProcess", "svcStartProcess",
    "svcCreateProcess", "hiddbgInitialize", "hidInitialize", "fsInitialize",
    "fsdevMountSdmc", "fopen", "system",
}


def audit_metadata(metadata):
    for field in ("title_id", "title_id_range_min", "title_id_range_max"):
        if int(metadata.get(field, "0"), 0) != PROGRAM_ID:
            raise ValueError(f"unexpected {field}")
    if int(metadata.get("filesystem_access", {}).get("permissions", "-1"), 0) != 0:
        raise ValueError("filesystem permission granted")
    if set(metadata.get("service_access", [])) != SERVICES or metadata.get("service_host") != []:
        raise ValueError("service grants changed")
    capabilities = metadata.get("kernel_capabilities", [])
    by_type = {entry.get("type"): entry.get("value") for entry in capabilities}
    if len(capabilities) != 5 or set(by_type) != {
        "kernel_flags", "syscalls", "min_kernel_version", "handle_table_size", "debug_flags"
    }:
        raise ValueError("unexpected kernel capability category")
    if by_type["kernel_flags"] != {
        "highest_thread_priority": 63, "lowest_thread_priority": 24,
        "lowest_cpu_id": 3, "highest_cpu_id": 3
    } or int(by_type["min_kernel_version"], 0) != 0x30 or by_type["handle_table_size"] != 128:
        raise ValueError("unexpected kernel capability settings")
    if by_type["debug_flags"] != {"allow_debug": False, "force_debug": False}:
        raise ValueError("debug flags granted")
    values = list(by_type["syscalls"].values())
    if len(values) != len(SYSCALLS) or {int(value, 0) for value in values} != SYSCALLS:
        raise ValueError("unexpected syscall grant")
    if 0x6B in {int(value, 0) for value in values}:
        raise ValueError("debug write syscall granted")


def audit_symbols(metadata, symbols):
    granted = set()
    for entry in metadata.get("kernel_capabilities", []):
        if entry.get("type") == "syscalls":
            granted.update(entry["value"])
    linked = set(re.findall(r"^[0-9a-fA-F]+ [TtUu] (svc[A-Za-z0-9_]+)$", symbols, re.MULTILINE))
    missing = linked - granted
    if missing:
        raise ValueError(f"linked SVC lacks NPDM grant: {', '.join(sorted(missing))}")


def audit_source_text(text):
    forbidden = re.compile(r"\b(?:" + "|".join(re.escape(name) for name in FORBIDDEN_APIS) + r")\b")
    if match := forbidden.search(text):
        raise ValueError(f"forbidden source API {match.group()}")
    calls = re.findall(r"\bserviceDispatch[A-Za-z0-9_]*\s*\(", text)
    parsed = re.findall(
        r"\b(serviceDispatch[A-Za-z0-9_]*)\s*\(\s*([^,()]+),\s*([^,\s)]+)", text
    )
    if len(calls) != len(parsed):
        raise ValueError("unparsed service dispatch call")
    allowed = {("serviceDispatchOut", "65000"), ("serviceDispatchOut", "65002"),
               ("serviceDispatchIn", "65102")}
    for function, _session, command in parsed:
        if (function, command) not in allowed:
            raise ValueError(f"forbidden dmnt:cht call {function} {command}")


def extract_pfs0(data):
    if len(data) < 16:
        raise ValueError("short PFS0 header")
    magic, count, names_length, reserved = struct.unpack_from("<4sIII", data)
    if magic != b"PFS0" or count > 16 or reserved != 0:
        raise ValueError("invalid PFS0 header")
    names_start = 16 + count * 24
    content_start = names_start + names_length
    if content_start > len(data):
        raise ValueError("truncated PFS0 name table")
    names = data[names_start:content_start]
    members = {}
    for index in range(count):
        offset, size, name_offset, entry_reserved = struct.unpack_from("<QQII", data, 16 + index * 24)
        if entry_reserved or name_offset >= len(names):
            raise ValueError("invalid PFS0 entry")
        terminator = names.find(b"\0", name_offset)
        if terminator < 0 or offset > len(data) - content_start or size > len(data) - content_start - offset:
            raise ValueError("truncated PFS0 member")
        name = names[name_offset:terminator].decode("ascii")
        if name in members:
            raise ValueError("duplicate PFS0 member")
        members[name] = data[content_start + offset:content_start + offset + size]
    return members


def audit_artifacts(directory, hactool, nm):
    directory = Path(directory)
    npdm = (directory / "memory_peek.npdm").read_bytes()
    nso = (directory / "memory_peek.nso").read_bytes()
    nsp = (directory / "memory_peek.nsp").read_bytes()
    members = extract_pfs0(nsp)
    if members != {"main": nso, "main.npdm": npdm}:
        raise ValueError("NSP contents differ from audited NSO/NPDM")
    with tempfile.TemporaryDirectory() as temporary:
        path = Path(temporary) / "permissions.json"
        subprocess.run([hactool, "-t", "npdm", f"--json={path}", str(directory / "memory_peek.npdm")],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        metadata = json.loads(path.read_text())
        audit_metadata(metadata)
    source_root = Path(__file__).resolve().parent.parent / "source"
    for path in source_root.glob("*.[ch]"):
        try:
            audit_source_text(path.read_text())
        except ValueError as error:
            raise ValueError(f"{path}: {error}") from error
    symbols = subprocess.run([nm, "-a", str(directory / "memory_peek.elf")], check=True,
                             capture_output=True, text=True).stdout
    forbidden = re.compile(r"\b(?:" + "|".join(re.escape(name) for name in FORBIDDEN_APIS) + r")\b")
    if match := forbidden.search(symbols):
        raise ValueError(f"forbidden ELF symbol {match.group()}")
    audit_symbols(metadata, symbols)
    print("artifact audit passed: packaged NPDM and NSO match; restricted SVC, service, and filesystem grants")
    for filename, contents in (("memory_peek.nsp", nsp), ("memory_peek.npdm", npdm),
                               ("memory_peek.nso", nso)):
        print(f"{filename} sha256 {hashlib.sha256(contents).hexdigest()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory")
    parser.add_argument("--hactool", required=True)
    parser.add_argument("--nm", required=True)
    arguments = parser.parse_args()
    audit_artifacts(arguments.directory, arguments.hactool, arguments.nm)
