#ifndef MEMORY_PEEK_PROTOCOL_H
#define MEMORY_PEEK_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define MP_MAX_LINE 2048
#define MP_MAX_RANGES 32
#define MP_MAX_READ 1048576

enum mp_operation {
    MP_VERSION, MP_IDENTITY, MP_TITLE_ID, MP_MAIN_BASE, MP_BUILD_ID,
    MP_HEAP_BASE, MP_IS_RUNNING, MP_PEEK_MAIN, MP_PEEK_ABSOLUTE,
    MP_PEEK_ABSOLUTE_MULTI
};

struct mp_range { uint64_t address; uint32_t length; };
struct mp_request {
    enum mp_operation operation;
    uint64_t program_id;
    uint32_t count;
    uint32_t total_length;
    struct mp_range ranges[MP_MAX_RANGES];
};

/* Returns 0 for a valid request, -1 for an invalid request. */
int mp_parse_request(const char *line, size_t length, struct mp_request *out);

#endif
