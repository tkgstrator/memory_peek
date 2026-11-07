#include "memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

/* Fake Switch: every service/SVC outcome is configurable and open handles are counted. */
static Service pm_service = {1};
static Result pm_rc, ldr_init_rc, info_init_rc, attach_rc, ldr_rc, prog_rc, heap_rc, read_rc;
static u64 pm_pid, program_id, heap_base;
static s32 module_count;
static LoaderModuleInfo module_table[2];
static int next_handle, open_handles, attach_calls, close_calls, read_calls;
static int ldr_up, info_up, pm_calls;
static int sequence_attach_before_ldr, ldr_saw_open_handle, heap_saw_open_handle;
static u64 program_id_after_first;
static int program_queries;

Service *pmdmntGetServiceSession(void) { return &pm_service; }
Result test_service_dispatch_out(Service *service, int command, void *output, size_t size) {
    assert(service == &pm_service && command == 4 && size == sizeof(u64));
    *(u64 *)output = pm_pid;
    ++pm_calls;
    return pm_rc;
}
Result ldrDmntInitialize(void) {
    if (ldr_init_rc == 0)
        ldr_up = 1;
    return ldr_init_rc;
}
void ldrDmntExit(void) { ldr_up = 0; }
Result pminfoInitialize(void) {
    if (info_init_rc == 0)
        info_up = 1;
    return info_init_rc;
}
void pminfoExit(void) { info_up = 0; }
Result ldrDmntGetProcessModuleInfo(u64 pid, LoaderModuleInfo *out, u32 max, s32 *count) {
    assert(ldr_up && pid == pm_pid && max == 2);
    ldr_saw_open_handle = open_handles > 0;
    sequence_attach_before_ldr = attach_calls > 0;
    memcpy(out, module_table, sizeof(module_table));
    *count = module_count;
    return ldr_rc;
}
Result pminfoGetProgramId(u64 *id, u64 pid) {
    assert(info_up && pid == pm_pid);
    *id = ++program_queries > 1 && program_id_after_first ? program_id_after_first : program_id;
    return prog_rc;
}
Result svcDebugActiveProcess(Handle *handle, u64 pid) {
    assert(pid == pm_pid);
    ++attach_calls;
    if (attach_rc)
        return attach_rc;
    *handle = (Handle)(0x100 + ++next_handle);
    ++open_handles;
    return 0;
}
Result svcReadDebugProcessMemory(void *buffer, Handle debug, u64 address, u64 size) {
    assert(debug >= 0x100 && open_handles > 0);
    ++read_calls;
    if (read_rc)
        return read_rc;
    for (u64 i = 0; i < size; ++i)
        ((u8 *)buffer)[i] = (u8)(address + i);
    return 0;
}
Result svcGetInfo(u64 *out, InfoType type, Handle handle, u64 sub_type) {
    assert(type == InfoType_HeapRegionAddress && handle >= 0x100 && sub_type == 0);
    heap_saw_open_handle = open_handles > 0;
    *out = heap_base;
    return heap_rc;
}
Result svcCloseHandle(Handle handle) {
    assert(handle >= 0x100 && open_handles > 0);
    ++close_calls;
    --open_handles;
    return 0;
}

static void reset(void) {
    pm_rc = ldr_init_rc = info_init_rc = attach_rc = ldr_rc = prog_rc = heap_rc = read_rc = 0;
    pm_pid = 42;
    program_id = 0x0100C2500FC20000ULL;
    program_id_after_first = 0;
    heap_base = 0x8000;
    module_count = 2;
    memset(module_table, 0, sizeof(module_table));
    module_table[0].base_address = 0x500;
    module_table[1].base_address = 0x1000;
    for (int i = 0; i < 0x20; ++i) {
        module_table[0].build_id[i] = (u8)(0x10 + i);
        module_table[1].build_id[i] = (u8)(0xA0 + i);
    }
    open_handles = attach_calls = close_calls = read_calls = pm_calls = program_queries = 0;
    sequence_attach_before_ldr = ldr_saw_open_handle = heap_saw_open_handle = 0;
}

static void expect_failure(int expected_code, int expected_attaches) {
    struct mp_snapshot snapshot = {0};
    assert(mp_switch_backend.begin(NULL, &snapshot) == expected_code);
    assert(open_handles == 0);
    assert(attach_calls == expected_attaches);
    assert(close_calls == expected_attaches - (attach_rc ? 1 : 0));
}

int main(void) {
    reset();
    assert(mp_memory_initialize() == 0 && ldr_up && info_up);
    mp_memory_exit();
    assert(!ldr_up && !info_up);
    reset();
    info_init_rc = 1;
    assert(mp_memory_initialize() != 0 && !ldr_up && !info_up);
    reset();
    ldr_init_rc = 1;
    assert(mp_memory_initialize() != 0 && !ldr_up && !info_up);
    reset();
    assert(mp_memory_initialize() == 0);

    /* Successful begin: two modules selects module[1]; handle stays open until end. */
    struct mp_snapshot snapshot = {0};
    assert(mp_switch_backend.begin(NULL, &snapshot) == 0);
    assert(snapshot.pid == 42 && snapshot.title_id == 0x0100C2500FC20000ULL);
    assert(snapshot.main_base == 0x1000 && snapshot.heap_base == 0x8000);
    assert(snapshot.build_id[0] == 0xA0 && snapshot.build_id[7] == 0xA7);
    assert(sequence_attach_before_ldr && ldr_saw_open_handle && heap_saw_open_handle);
    assert(open_handles == 1 && snapshot.debug_handle != 0);
    uint8_t buffer[4];
    assert(mp_switch_backend.read(NULL, &snapshot, 0x20, buffer, 4) == 0);
    assert(buffer[0] == 0x20 && buffer[3] == 0x23);
    read_rc = 7;
    assert(mp_switch_backend.read(NULL, &snapshot, 0x20, buffer, 4) != 0);
    read_rc = 0;
    assert(mp_switch_backend.still_current(NULL, &snapshot));
    program_id_after_first = 0x01;
    assert(!mp_switch_backend.still_current(NULL, &snapshot));
    program_id_after_first = 0;
    pm_pid = 43;
    assert(!mp_switch_backend.still_current(NULL, &snapshot));
    pm_pid = 42;
    mp_switch_backend.end(NULL, &snapshot);
    assert(open_handles == 0 && close_calls == 1 && snapshot.debug_handle == 0);
    mp_switch_backend.end(NULL, &snapshot); /* idempotent */
    assert(close_calls == 1);

    /* One module selects module[0]. */
    reset();
    module_count = 1;
    assert(mp_switch_backend.begin(NULL, &snapshot) == 0);
    assert(snapshot.main_base == 0x500 && snapshot.build_id[0] == 0x10);
    mp_switch_backend.end(NULL, &snapshot);
    assert(open_handles == 0);

    /* No attach is made without a pid. */
    reset();
    pm_rc = 5; /* Service failure is not an absent game. */
    expect_failure(-2, 0);
    reset();
    pm_rc = 0x20F; /* pm::ResultProcessNotFound. */
    expect_failure(-1, 0);
    reset();
    pm_pid = 0;
    expect_failure(-1, 0);
    /* Attach failure opens nothing. */
    reset();
    attach_rc = 9;
    expect_failure(-2, 1);
    /* Every failure after attach closes the handle. */
    reset();
    ldr_rc = 3;
    expect_failure(-2, 1);
    reset();
    module_count = 0;
    expect_failure(-2, 1);
    reset();
    module_count = 3;
    expect_failure(-2, 1);
    reset();
    module_table[1].base_address = 0;
    expect_failure(-2, 1);
    reset();
    prog_rc = 4;
    expect_failure(-2, 1);
    reset();
    heap_rc = 6;
    expect_failure(-2, 1);

    /* Diagnostics report raw codes and always release the attach. */
    struct mp_diagnostics d;
    reset();
    assert(mp_switch_backend.diagnose(NULL, &d) == 0);
    assert(d.pm_rc == 0 && d.pid == 42 && d.attach_rc == 0 && d.ldr_rc == 0 && d.modules == 2);
    assert(d.info_rc == 0 && d.heap_rc == 0 && d.title_id == 0x0100C2500FC20000ULL);
    assert(d.main_base == 0x1000 && d.heap_base == 0x8000 && open_handles == 0 && close_calls == 1);
    reset();
    pm_rc = 0x11;
    assert(mp_switch_backend.diagnose(NULL, &d) == 0);
    assert(d.pm_rc == 0x11 && d.pid == 0 && attach_calls == 0);
    reset();
    attach_rc = 0x22;
    assert(mp_switch_backend.diagnose(NULL, &d) == 0);
    assert(d.attach_rc == 0x22 && d.pid == 42 && d.ldr_rc == 0 && d.modules == 0);
    reset();
    ldr_rc = 0x33;
    prog_rc = 0x44;
    heap_rc = 0x55;
    assert(mp_switch_backend.diagnose(NULL, &d) == 0);
    assert(d.ldr_rc == 0x33 && d.info_rc == 0x44 && d.heap_rc == 0x55 && d.main_base == 0);
    assert(d.title_id == 0 && open_handles == 0 && close_calls == 1);

    mp_memory_exit();
    puts("memory tests passed");
}
