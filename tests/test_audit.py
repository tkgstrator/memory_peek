import copy
import json
import struct
import unittest
from pathlib import Path

from tools.audit import (
    audit_metadata,
    audit_npdm_debug_capability,
    audit_source_text,
    audit_symbols,
    extract_pfs0,
)

SAFE = {
    "title_id": "0x43004D5045454B01",
    "title_id_range_min": "0x43004D5045454B01",
    "title_id_range_max": "0x43004D5045454B01",
    "filesystem_access": {"permissions": "0x0"},
    "service_access": ["pm:dmnt", "ldr:dmnt", "pm:info", "bsd:u"],
    "service_host": [],
    "kernel_capabilities": [
        {
            "type": "kernel_flags",
            "value": {
                "highest_thread_priority": 63,
                "lowest_thread_priority": 24,
                "lowest_cpu_id": 3,
                "highest_cpu_id": 3,
            },
        },
        {
            "type": "syscalls",
            "value": {
                "svcSetHeapSize": "0x01",
                "svcSetMemoryPermission": "0x02",
                "svcSetMemoryAttribute": "0x03",
                "svcUnmapMemory": "0x05",
                "svcQueryMemory": "0x06",
                "svcExitProcess": "0x07",
                "svcCreateThread": "0x08",
                "svcStartThread": "0x09",
                "svcExitThread": "0x0a",
                "svcSleepThread": "0x0b",
                "svcCreateTransferMemory": "0x15",
                "svcCloseHandle": "0x16",
                "svcWaitSynchronization": "0x18",
                "svcArbitrateLock": "0x1a",
                "svcArbitrateUnlock": "0x1b",
                "svcWaitProcessWideKeyAtomic": "0x1c",
                "svcSignalProcessWideKey": "0x1d",
                "svcConnectToNamedPort": "0x1f",
                "svcSendSyncRequest": "0x21",
                "svcBreak": "0x26",
                "svcReturnFromException": "0x28",
                "svcGetInfo": "0x29",
                "svcUnmapTransferMemory": "0x52",
                "svcDebugActiveProcess": "0x60",
                "svcReadDebugProcessMemory": "0x6A",
            },
        },
        {"type": "min_kernel_version", "value": "0x0030"},
        {"type": "handle_table_size", "value": 128},
        {
            "type": "debug_flags",
            "value": {
                "allow_debug": False,
                "force_debug": True,
                "force_debug_prod": False,
            },
        },
    ],
}


FORCE_DEBUG = 0x0008FFFF
FORCE_DEBUG_PROD = 0x0004FFFF


def build_npdm(aci0=(FORCE_DEBUG,), acid=(FORCE_DEBUG,), magic=b"META"):
    """Synthetic NPDM: META, then ACI0 (0x40 header) and ACID (0x240 header)."""
    aci0_kac = struct.pack(f"<{len(aci0)}I", *aci0)
    acid_kac = struct.pack(f"<{len(acid)}I", *acid)
    aci0_header = bytearray(0x40)
    aci0_header[0:4] = b"ACI0"
    struct.pack_into("<II", aci0_header, 0x30, 0x40, len(aci0_kac))
    aci0_data = bytes(aci0_header) + aci0_kac
    acid_header = bytearray(0x240)
    acid_header[0x200:0x204] = b"ACID"
    struct.pack_into("<II", acid_header, 0x230, 0x240, len(acid_kac))
    acid_data = bytes(acid_header) + acid_kac
    meta = bytearray(0x80)
    meta[0:4] = magic
    aci0_offset = 0x80
    acid_offset = aci0_offset + len(aci0_data)
    struct.pack_into(
        "<IIII", meta, 0x70, aci0_offset, len(aci0_data), acid_offset, len(acid_data)
    )
    return bytes(meta) + aci0_data + acid_data


class NpdmDebugCapabilityTests(unittest.TestCase):
    def test_accepts_force_debug_in_both_sections(self):
        audit_npdm_debug_capability(build_npdm())
        audit_npdm_debug_capability(
            build_npdm(aci0=(0x1, FORCE_DEBUG, 0x2), acid=(FORCE_DEBUG, 0x3))
        )

    def test_rejects_wrong_missing_duplicate_or_combined_descriptors(self):
        for kwargs in (
            {"aci0": (FORCE_DEBUG_PROD,), "acid": (FORCE_DEBUG_PROD,)},
            {"aci0": (FORCE_DEBUG,), "acid": (FORCE_DEBUG_PROD,)},
            {"aci0": (FORCE_DEBUG_PROD,), "acid": (FORCE_DEBUG,)},
            {"aci0": (), "acid": (FORCE_DEBUG,)},
            {"aci0": (FORCE_DEBUG,), "acid": (0x1,)},
            {"aci0": (FORCE_DEBUG, FORCE_DEBUG), "acid": (FORCE_DEBUG,)},
            {"aci0": (FORCE_DEBUG,), "acid": (FORCE_DEBUG, FORCE_DEBUG_PROD)},
            {"aci0": (0x000CFFFF,), "acid": (FORCE_DEBUG,)},
            {"aci0": (FORCE_DEBUG,), "acid": (0x000EFFFF,)},
            {"aci0": (FORCE_DEBUG,), "acid": (0x0009FFFF,)},
        ):
            with self.assertRaises(ValueError, msg=str(kwargs)):
                audit_npdm_debug_capability(build_npdm(**kwargs))

    def test_rejects_malformed_or_truncated_npdm(self):
        good = build_npdm()
        broken_cases = []
        broken_cases.append(build_npdm(magic=b"ATEM"))
        broken_cases.append(good[:0x70])
        broken_cases.append(good[:-1])
        broken_cases.append(good[:-4])
        for field in (0x70, 0x78):  # section offset out of range
            broken = bytearray(good)
            struct.pack_into("<I", broken, field, 0xFFFFFFF0)
            broken_cases.append(bytes(broken))
        for field in (0x74, 0x7C):  # section size beyond the file
            broken = bytearray(good)
            struct.pack_into("<I", broken, field, len(good))
            broken_cases.append(bytes(broken))
        broken = bytearray(good)  # wrong ACI0 magic
        broken[0x80:0x84] = b"XXXX"
        broken_cases.append(bytes(broken))
        broken = bytearray(good)  # KAC array exceeds its section
        struct.pack_into("<I", broken, 0x80 + 0x34, 0x1000)
        broken_cases.append(bytes(broken))
        broken = bytearray(good)  # KAC size is not a whole number of words
        struct.pack_into("<I", broken, 0x80 + 0x34, 3)
        broken_cases.append(bytes(broken))
        broken = bytearray(good)  # KAC offset points outside its section
        struct.pack_into("<I", broken, 0x80 + 0x30, 0x1000)
        broken_cases.append(bytes(broken))
        for index, case in enumerate(broken_cases):
            with self.assertRaises(ValueError, msg=f"case {index}"):
                audit_npdm_debug_capability(case)

    def test_rejects_capability_arrays_inside_section_headers(self):
        for section_name, section_offset, kac_field, overlapping_offset in (
            ("ACI0", 0x80, 0x30, 0x20),
            ("ACID", 0xC4, 0x230, 0x00),
        ):
            broken = bytearray(build_npdm())
            struct.pack_into(
                "<I", broken, section_offset + overlapping_offset, FORCE_DEBUG
            )
            struct.pack_into(
                "<I", broken, section_offset + kac_field, overlapping_offset
            )
            with self.subTest(section=section_name), self.assertRaises(ValueError):
                audit_npdm_debug_capability(bytes(broken))

    def test_rejects_top_level_section_inside_meta_header(self):
        broken = bytearray(build_npdm())
        aci0_size = struct.unpack_from("<I", broken, 0x74)[0]
        broken[0x20 : 0x20 + aci0_size] = broken[0x80 : 0x80 + aci0_size]
        struct.pack_into("<I", broken, 0x70, 0x20)
        with self.assertRaises(ValueError):
            audit_npdm_debug_capability(bytes(broken))

    def test_raw_capability_is_authoritative_over_hactool_flag_names(self):
        # hactool's displayed names are version-dependent: the metadata check
        # accepts whatever labels it shows, and the raw descriptors decide.
        for labels in (
            {"allow_debug": False, "force_debug": True, "force_debug_prod": False},
            {"allow_debug": False, "force_debug": False, "force_debug_prod": True},
            {"force_debug": True},
            {},
        ):
            fixture = copy.deepcopy(SAFE)
            fixture["kernel_capabilities"][4]["value"] = labels
            audit_metadata(fixture)
        with self.assertRaises(ValueError):
            audit_npdm_debug_capability(
                build_npdm(aci0=(FORCE_DEBUG_PROD,), acid=(FORCE_DEBUG_PROD,))
            )

    def test_source_json_debug_flags(self):
        capabilities = json.loads(Path("memory_peek.json").read_text())[
            "kernel_capabilities"
        ]
        flags = next(c["value"] for c in capabilities if c["type"] == "debug_flags")
        self.assertEqual(
            [flags["allow_debug"], flags["force_debug"], flags["force_debug_prod"]],
            [False, True, False],
        )


class AuditTests(unittest.TestCase):
    def test_rejects_unsafe_npdm_grants(self):
        audit_metadata(SAFE)
        syscalls = lambda x: x["kernel_capabilities"][1]["value"]
        for mutation in (
            lambda x: syscalls(x).update({"svcWriteDebugProcessMemory": "0x6b"}),
            lambda x: syscalls(x).update({"svcBreakDebugProcess": "0x61"}),
            lambda x: syscalls(x).update({"svcTerminateDebugProcess": "0x62"}),
            lambda x: syscalls(x).update({"svcGetDebugEvent": "0x63"}),
            lambda x: syscalls(x).update({"svcContinueDebugEvent": "0x64"}),
            lambda x: syscalls(x).update({"svcGetProcessList": "0x65"}),
            lambda x: syscalls(x).update({"svcSetDebugThreadContext": "0x6d"}),
            lambda x: syscalls(x).pop("svcDebugActiveProcess"),
            lambda x: syscalls(x).pop("svcReadDebugProcessMemory"),
            lambda x: x["service_access"].append("*"),
            lambda x: x["service_access"].append("dmnt:cht"),
            lambda x: x["service_access"].append("pm:bm"),
            lambda x: x["service_access"].remove("ldr:dmnt"),
            lambda x: x["service_access"].remove("pm:info"),
            lambda x: x["service_host"].append("*"),
            lambda x: x["filesystem_access"].update({"permissions": "0xffff"}),
            lambda x: x.update({"title_id": "0x1"}),
        ):
            fixture = copy.deepcopy(SAFE)
            mutation(fixture)
            with self.assertRaises(ValueError):
                audit_metadata(fixture)

    def test_extracts_only_packaged_members(self):
        names = b"main\0main.npdm\0"
        first = struct.pack("<QQII", 0, 3, 0, 0)
        second = struct.pack("<QQII", 3, 4, 5, 0)
        data = (
            struct.pack("<4sIII", b"PFS0", 2, len(names), 0)
            + first
            + second
            + names
            + b"NSO"
            + b"NPDM"
        )
        self.assertEqual(extract_pfs0(data), {"main": b"NSO", "main.npdm": b"NPDM"})
        with self.assertRaises(ValueError):
            extract_pfs0(data[:-1])

    def test_linked_svc_requires_matching_npdm_grant(self):
        symbols = "0000000000002600 T svcSendSyncRequest\n"
        audit_symbols(SAFE, symbols)
        with self.assertRaises(ValueError):
            audit_symbols(SAFE, symbols + "0000000000002608 T svcMapMemory\n")
        with self.assertRaises(ValueError):
            audit_symbols(SAFE, symbols + "0000000000002608 W svcMapMemory\n")

    def test_rejects_extra_kernel_capability_category(self):
        for capability in (
            {"type": "map_page", "value": "0x0"},
            {"type": "map_range", "value": "0x0"},
            {"type": "debug_flags", "value": {"allow_debug": True}},
        ):
            fixture = copy.deepcopy(SAFE)
            fixture["kernel_capabilities"].append(capability)
            with self.assertRaises(ValueError):
                audit_metadata(fixture)

    def test_rejects_cheat_write_control_or_unlisted_calls_in_source(self):
        audit_source_text("svcDebugActiveProcess(&handle, pid);\n")
        audit_source_text("svcReadDebugProcessMemory(buffer, handle, address, size);\n")
        audit_source_text("svcGetInfo(&heap, InfoType_HeapRegionAddress, handle, 0);\n")
        audit_source_text("svcCloseHandle(handle);\n")
        for forbidden in (
            "serviceDispatchOut(&service, 65002, metadata)",
            "serviceDispatchIn(&service, 65102, request)",
            "serviceDispatchIn(&service, 65103, request)",
            "serviceDispatch(&service, 65003)",
            'smGetService(&s, "dmnt:cht")',
            "dmntchtInitialize()",
            "// cheat",
            "svcWriteDebugProcessMemory(h, b, a, n)",
            "svcBreakDebugProcess(h)",
            "svcTerminateDebugProcess(h)",
            "svcGetDebugEvent(e, h)",
            "svcContinueDebugEvent(h, 0, 0, 0)",
            "svcQueryDebugProcessMemory(&i, &p, h, a)",
            "svcSetDebugThreadContext(h, 0, 0, 0)",
            "svcQueryMemory(&i, &p, a)",
            "svcMapProcessMemory(a, h, b, n)",
            "svcSomethingNew(h)",
            "pmdmntStartProcess(pid)",
            "pmdmntHookToCreateProcess(&event, id)",
            "pmdmntClearHook(0)",
            "pmdmntGetApplicationProcessId(&pid)",
            "pmdmntAtmosphereGetProcessInfo(0, 0, 0, pid)",
        ):
            with self.assertRaises(ValueError, msg=forbidden):
                audit_source_text(forbidden)

    def test_repository_sources_pass_source_audit(self):
        for path in sorted(Path("source").rglob("*.[ch]")):
            audit_source_text(path.read_text())

    def test_pm_application_pid_probe_is_limited_to_pm_session(self):
        source = (
            "Service *pm_session = pmdmntGetServiceSession();\n"
            "serviceDispatchOut(pm_session, 4, pid);\n"
        )
        audit_source_text(source)
        with self.assertRaises(ValueError):
            audit_source_text("serviceDispatchOut(&dmnt_session, 4, pid);\n")
        with self.assertRaises(ValueError):
            audit_source_text("serviceDispatchOut(pm_session, 4, pid);\n")
        with self.assertRaises(ValueError):
            audit_source_text(
                "Service *pm_session = pmdmntGetServiceSession();\n"
                "pm_session = &dmnt_session;\n"
                "serviceDispatchOut(pm_session, 4, pid);\n"
            )


if __name__ == "__main__":
    unittest.main()
