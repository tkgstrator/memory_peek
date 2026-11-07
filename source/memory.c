#include "memory.h"
#include <switch.h>
#include <string.h>

#define MP_MODULE_SLOTS 2
/* Atmosphere pm::ResultProcessNotFound when no application is running. */
#define MP_PM_PROCESS_NOT_FOUND 0x20F

/* Raw initialization result per service; 0xFFFFFFFF until first attempted. */
#define MP_INIT_NOT_ATTEMPTED ((Result)0xFFFFFFFF)
static int ldr_ready, info_ready;
static Result ldr_init_rc = MP_INIT_NOT_ATTEMPTED, info_init_rc = MP_INIT_NOT_ATTEMPTED;

/* This sysmodule skips libnx's default HOS-version initialization. On supported
 * firmware, pm:dmnt GetApplicationProcessId is command 4; the libnx wrapper
 * would send the pre-5.0 command 5 when hosversionGet() is still zero. */
static Result get_application_process_id(u64 *pid) {
    Service *pm_session = pmdmntGetServiceSession();
    return serviceDispatchOut(pm_session, 4, *pid);
}

/* Attempts only the services not yet up; successes are retained and never initialized
 * twice. Failures are recorded, not fatal. Returns 0 when both are ready. */
int mp_memory_initialize(void) {
    if (!ldr_ready) {
        ldr_init_rc = ldrDmntInitialize();
        ldr_ready = R_SUCCEEDED(ldr_init_rc);
    }
    if (!info_ready) {
        info_init_rc = pminfoInitialize();
        info_ready = R_SUCCEEDED(info_init_rc);
    }
    return ldr_ready && info_ready ? 0 : -1;
}

void mp_memory_exit(void) {
    if (info_ready)
        pminfoExit();
    if (ldr_ready)
        ldrDmntExit();
    info_ready = ldr_ready = 0;
    ldr_init_rc = info_init_rc = MP_INIT_NOT_ATTEMPTED;
}

/* One read-only probe: pm pid, debug attach, loader modules, program id, heap.
 * A nonzero handle is returned still open; the caller owns closing it. */
struct probe {
    struct mp_diagnostics d;
    Handle handle;
    uint8_t build_id[8];
};

static void close_handle(Handle *handle) {
    if (*handle) {
        svcCloseHandle(*handle);
        *handle = 0;
    }
}

static void run_probe(struct probe *probe) {
    memset(probe, 0, sizeof(*probe));
    struct mp_diagnostics *d = &probe->d;
    if (!ldr_ready || !info_ready)
        mp_memory_initialize();
    d->ldr_init_rc = (uint32_t)ldr_init_rc;
    d->info_init_rc = (uint32_t)info_init_rc;
    d->ldr_ready = (uint32_t)ldr_ready;
    d->info_ready = (uint32_t)info_ready;
    u64 pid = 0;
    d->pm_rc = (uint32_t)get_application_process_id(&pid);
    if (d->pm_rc != 0)
        return;
    d->pid = pid;
    /* Never attach or call ldr/pminfo while either service is unavailable. */
    if (pid == 0 || !ldr_ready || !info_ready)
        return;

    Handle handle = 0;
    d->attach_rc = (uint32_t)svcDebugActiveProcess(&handle, pid);
    if (d->attach_rc != 0)
        return;
    probe->handle = handle;

    LoaderModuleInfo modules[MP_MODULE_SLOTS];
    memset(modules, 0, sizeof(modules));
    s32 count = 0;
    d->ldr_rc = (uint32_t)ldrDmntGetProcessModuleInfo(pid, modules, MP_MODULE_SLOTS, &count);
    if (d->ldr_rc == 0 && count >= 1 && count <= MP_MODULE_SLOTS) {
        /* Same module choice as sys-botbase: the second module when exactly two. */
        const LoaderModuleInfo *main_module = &modules[count == 2 ? 1 : 0];
        d->modules = (uint32_t)count;
        d->main_base = main_module->base_address;
        memcpy(probe->build_id, main_module->build_id, sizeof(probe->build_id));
    }

    u64 title = 0;
    d->info_rc = (uint32_t)pminfoGetProgramId(&title, pid);
    if (d->info_rc == 0)
        d->title_id = title;

    u64 heap = 0;
    d->heap_rc = (uint32_t)svcGetInfo(&heap, InfoType_HeapRegionAddress, handle, 0);
    if (d->heap_rc == 0)
        d->heap_base = heap;
}

static int probe_complete(const struct probe *probe) {
    const struct mp_diagnostics *d = &probe->d;
    return d->ldr_ready && d->info_ready && d->pm_rc == 0 && d->pid != 0 && d->attach_rc == 0 &&
           d->ldr_rc == 0 && d->modules >= 1 && d->main_base != 0 && d->info_rc == 0 &&
           d->heap_rc == 0;
}

static int begin_snapshot(void *context, struct mp_snapshot *snapshot) {
    (void)context;
    struct probe probe;
    run_probe(&probe);
    if (probe.d.pm_rc == MP_PM_PROCESS_NOT_FOUND || (probe.d.pm_rc == 0 && probe.d.pid == 0))
        return -1;
    if (probe.d.pm_rc != 0)
        return -2;
    if (!probe_complete(&probe)) {
        close_handle(&probe.handle);
        return -2;
    }
    snapshot->pid = probe.d.pid;
    snapshot->title_id = probe.d.title_id;
    snapshot->main_base = probe.d.main_base;
    snapshot->heap_base = probe.d.heap_base;
    memcpy(snapshot->build_id, probe.build_id, sizeof(snapshot->build_id));
    snapshot->debug_handle = probe.handle;
    return 0;
}

static int read_memory(void *context, const struct mp_snapshot *snapshot, uint64_t address,
                       uint8_t *destination, size_t length) {
    (void)context;
    if (snapshot->debug_handle == 0)
        return -1;
    return R_SUCCEEDED(
               svcReadDebugProcessMemory(destination, snapshot->debug_handle, address, length))
               ? 0
               : -1;
}

static int still_current(void *context, const struct mp_snapshot *snapshot) {
    (void)context;
    u64 pid = 0, title = 0;
    return info_ready && R_SUCCEEDED(get_application_process_id(&pid)) && pid == snapshot->pid &&
           R_SUCCEEDED(pminfoGetProgramId(&title, pid)) && title == snapshot->title_id;
}

static void end_snapshot(void *context, struct mp_snapshot *snapshot) {
    (void)context;
    Handle handle = snapshot->debug_handle;
    snapshot->debug_handle = 0;
    close_handle(&handle);
}

static int diagnose(void *context, struct mp_diagnostics *diagnostics) {
    (void)context;
    struct probe probe;
    run_probe(&probe);
    close_handle(&probe.handle);
    *diagnostics = probe.d;
    return 0;
}

const struct mp_backend mp_switch_backend = {NULL,          begin_snapshot, read_memory,
                                             still_current, end_snapshot,   diagnose};
