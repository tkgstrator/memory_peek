#include "memory.h"
#include <switch.h>
#include <string.h>

/* Layout of the Atmosphère 1.12.1 dmnt:cht metadata IPC response. */
struct dmnt_region { u64 base, size; };
struct dmnt_metadata {
    u64 pid;
    u64 title_id;
    struct dmnt_region main_nso;
    struct dmnt_region heap;
    struct dmnt_region alias;
    struct dmnt_region address_space;
    u8 build_id[0x20];
};
_Static_assert(sizeof(struct dmnt_metadata) == 0x70, "dmnt metadata wire size");

static Service dmnt_session;

int mp_memory_initialize(void) {
    return R_SUCCEEDED(smGetService(&dmnt_session, "dmnt:cht")) ? 0 : -1;
}

void mp_memory_exit(void) {
    serviceClose(&dmnt_session);
}

static int get_dmnt_metadata(struct dmnt_metadata *metadata) {
    u8 attached = 0;
    if (R_FAILED(serviceDispatchOut(&dmnt_session, 65000, attached)) || !attached) return -1;
    if (R_FAILED(serviceDispatchOut(&dmnt_session, 65002, *metadata))) return -1;
    return 0;
}

static int begin_snapshot(void *context, struct mp_snapshot *snapshot) {
    (void)context;
    u64 pid = 0;
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid)) || pid == 0) return -1;
    struct dmnt_metadata metadata = {0};
    if (get_dmnt_metadata(&metadata) != 0 || metadata.pid != pid ||
        metadata.main_nso.base == 0) return -2;
    snapshot->pid = pid;
    snapshot->title_id = metadata.title_id;
    snapshot->main_base = metadata.main_nso.base;
    snapshot->heap_base = metadata.heap.base;
    memcpy(snapshot->build_id, metadata.build_id, sizeof(snapshot->build_id));
    return 0;
}

static int read_memory(void *context, const struct mp_snapshot *snapshot, uint64_t address,
                       uint8_t *destination, size_t length) {
    (void)context;
    (void)snapshot;
    const struct { u64 address, size; } request = {address, length};
    Result result = serviceDispatchIn(&dmnt_session, 65102, request,
        .buffer_attrs = {SfBufferAttr_Out | SfBufferAttr_HipcMapAlias},
        .buffers = {{destination, length}});
    return R_SUCCEEDED(result) ? 0 : -1;
}

static int still_current(void *context, const struct mp_snapshot *snapshot) {
    (void)context;
    u64 pid = 0;
    struct dmnt_metadata metadata = {0};
    return R_SUCCEEDED(pmdmntGetApplicationProcessId(&pid)) && pid == snapshot->pid &&
           get_dmnt_metadata(&metadata) == 0 && metadata.pid == pid &&
           metadata.title_id == snapshot->title_id &&
           metadata.main_nso.base == snapshot->main_base;
}

static void end_snapshot(void *context, struct mp_snapshot *snapshot) {
    (void)context;
    (void)snapshot;
}

const struct mp_backend mp_switch_backend = {
    NULL, begin_snapshot, read_memory, still_current, end_snapshot
};
