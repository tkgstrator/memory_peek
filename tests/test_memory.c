#include "memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

struct region {
    u64 base, size;
};
struct metadata {
    u64 pid;
    u64 title_id;
    struct region main_nso, heap, alias, address_space;
    u8 build_id[0x20];
};

static Service pm_service = {1};
static int pm_queries;

Result smGetService(Service *service, const char *name) {
    assert(strcmp(name, "dmnt:cht") == 0);
    service->kind = 2;
    return 0;
}
void serviceClose(Service *service) { (void)service; }
Service *pmdmntGetServiceSession(void) { return &pm_service; }

/* Reproduces libnx selecting the old pm command when HOS version is unset. */
Result pmdmntGetApplicationProcessId(u64 *pid) {
    (void)pid;
    return 1;
}

Result test_service_dispatch_out(Service *service, int command, void *output, size_t size) {
    if (service->kind == 1) {
        assert(command == 4);
        assert(size == sizeof(u64));
        *(u64 *)output = 42;
        ++pm_queries;
        return 0;
    }
    assert(service->kind == 2);
    if (command == 65000) {
        assert(size == sizeof(u8));
        *(u8 *)output = 1;
        return 0;
    }
    assert(command == 65002);
    assert(size == sizeof(struct metadata));
    struct metadata data = {0};
    data.pid = 42;
    data.title_id = 0x0100C2500FC20000ULL;
    data.main_nso.base = 0x1000;
    data.heap.base = 0x8000;
    memcpy(output, &data, size);
    return 0;
}
Result test_service_dispatch_in(Service *service, int command, const void *input, size_t input_size,
                                void *output, size_t output_size) {
    (void)service;
    (void)command;
    (void)input;
    (void)input_size;
    (void)output;
    (void)output_size;
    return 1;
}

int main(void) {
    assert(mp_memory_initialize() == 0);
    struct mp_snapshot snapshot = {0};
    assert(mp_switch_backend.begin(NULL, &snapshot) == 0);
    assert(snapshot.pid == 42);
    assert(snapshot.title_id == 0x0100C2500FC20000ULL);
    assert(snapshot.main_base == 0x1000);
    assert(mp_switch_backend.still_current(NULL, &snapshot));
    assert(pm_queries == 2);
    mp_memory_exit();
    puts("memory tests passed");
}
