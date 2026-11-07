#ifndef MEMORY_PEEK_SERVER_H
#define MEMORY_PEEK_SERVER_H

#include <stddef.h>
#include <stdint.h>

struct mp_snapshot {
    uint64_t pid;
    uint64_t title_id;
    uint64_t main_base;
    uint64_t heap_base;
    uint8_t build_id[8];
    /* Opaque to the server. Nonzero while a debug attach is open; end() closes it. */
    uint32_t debug_handle;
};

/* Raw read-only probe results. Fields for steps that were not reached are zero. */
struct mp_diagnostics {
    uint32_t pm_rc;
    uint64_t pid;
    uint32_t attach_rc;
    uint32_t ldr_rc;
    uint32_t modules;
    uint32_t info_rc;
    uint32_t heap_rc;
    uint64_t title_id;
    uint64_t main_base;
    uint64_t heap_base;
};

struct mp_backend {
    void *context;
    int (*begin)(void *context, struct mp_snapshot *snapshot);
    int (*read)(void *context, const struct mp_snapshot *snapshot, uint64_t address,
                uint8_t *destination, size_t length);
    int (*still_current)(void *context, const struct mp_snapshot *snapshot);
    /* Releases the attach opened by a successful begin; idempotent. The server calls it
     * before any network send or hex encoding. */
    void (*end)(void *context, struct mp_snapshot *snapshot);
    /* Runs a full probe and releases any attach before returning. */
    int (*diagnose)(void *context, struct mp_diagnostics *diagnostics);
};

/* Returns after peer close, timeout, oversize line, or socket failure. */
void mp_serve_client(int socket_fd, const struct mp_backend *backend, int idle_seconds);

#endif
