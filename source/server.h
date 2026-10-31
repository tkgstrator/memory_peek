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
};

struct mp_backend {
    void *context;
    int (*begin)(void *context, struct mp_snapshot *snapshot);
    int (*read)(void *context, const struct mp_snapshot *snapshot, uint64_t address,
                uint8_t *destination, size_t length);
    int (*still_current)(void *context, const struct mp_snapshot *snapshot);
    void (*end)(void *context, struct mp_snapshot *snapshot);
};

/* Returns after peer close, timeout, oversize line, or socket failure. */
void mp_serve_client(int socket_fd, const struct mp_backend *backend, int idle_seconds);

#endif
