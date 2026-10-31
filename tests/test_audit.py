import copy
import struct
import unittest

from tools.audit import audit_metadata, audit_symbols, audit_source_text, extract_pfs0


SAFE = {
    "title_id": "0x43004D5045454B01",
    "title_id_range_min": "0x43004D5045454B01",
    "title_id_range_max": "0x43004D5045454B01",
    "filesystem_access": {"permissions": "0x0"},
    "service_access": ["pm:dmnt", "dmnt:cht", "bsd:u"],
    "service_host": [],
    "kernel_capabilities": [
      {"type": "kernel_flags", "value": {
          "highest_thread_priority": 63, "lowest_thread_priority": 24,
          "lowest_cpu_id": 3, "highest_cpu_id": 3}},
      {"type": "syscalls", "value": {
        "svcSetHeapSize": "0x01", "svcSetMemoryPermission": "0x02",
        "svcSetMemoryAttribute": "0x03", "svcUnmapMemory": "0x05",
        "svcQueryMemory": "0x06", "svcExitProcess": "0x07",
        "svcCreateThread": "0x08", "svcStartThread": "0x09",
        "svcExitThread": "0x0a", "svcSleepThread": "0x0b",
        "svcCreateTransferMemory": "0x15", "svcCloseHandle": "0x16",
        "svcWaitSynchronization": "0x18", "svcArbitrateLock": "0x1a",
        "svcArbitrateUnlock": "0x1b", "svcWaitProcessWideKeyAtomic": "0x1c",
        "svcSignalProcessWideKey": "0x1d", "svcConnectToNamedPort": "0x1f",
        "svcSendSyncRequest": "0x21", "svcBreak": "0x26",
        "svcReturnFromException": "0x28", "svcGetInfo": "0x29",
        "svcUnmapTransferMemory": "0x52",
      }},
      {"type": "min_kernel_version", "value": "0x0030"},
      {"type": "handle_table_size", "value": 128},
      {"type": "debug_flags", "value": {"allow_debug": False, "force_debug": False}},
    ],
}


class AuditTests(unittest.TestCase):
    def test_rejects_unsafe_npdm_grants(self):
        audit_metadata(SAFE)
        for mutation in (
            lambda x: x["kernel_capabilities"][1]["value"].update({"svcWriteDebugProcessMemory": "0x6b"}),
            lambda x: x["kernel_capabilities"][1]["value"].update({"svcDebugActiveProcess": "0x60"}),
            lambda x: x["kernel_capabilities"][1]["value"].update({"svcReadDebugProcessMemory": "0x6a"}),
            lambda x: x["kernel_capabilities"][1]["value"].update({"svcTerminateDebugProcess": "0x62"}),
            lambda x: x["service_access"].append("*"),
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
        data = struct.pack("<4sIII", b"PFS0", 2, len(names), 0) + first + second + names + b"NSO" + b"NPDM"
        self.assertEqual(extract_pfs0(data), {"main": b"NSO", "main.npdm": b"NPDM"})
        with self.assertRaises(ValueError):
            extract_pfs0(data[:-1])

    def test_linked_svc_requires_matching_npdm_grant(self):
        symbols = "0000000000002600 T svcSendSyncRequest\n"
        audit_symbols(SAFE, symbols)
        with self.assertRaises(ValueError):
            audit_symbols(SAFE, symbols + "0000000000002608 T svcMapMemory\n")

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

    def test_rejects_attach_or_mutating_cheat_ipc_in_source(self):
        audit_source_text("serviceDispatchOut(&service, 65002, metadata);\n")
        audit_source_text("serviceDispatchIn(&service, 65102, request);\n")
        for forbidden in ("svcDebugActiveProcess(&handle, pid)",
                          "serviceDispatch(&service, 65003)",
                          "serviceDispatchIn(&service, 65103, request)",
                          "unsigned command=65103; serviceDispatchIn(&service, command, request)",
                          "serviceDispatchInOut(&service, 65103, request)"):
            with self.assertRaises(ValueError):
                audit_source_text(forbidden)


if __name__ == "__main__":
    unittest.main()
