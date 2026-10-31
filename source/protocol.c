#include "protocol.h"
#include <limits.h>
#include <string.h>

static int parse_number(const char *text, uint64_t *value) {
    unsigned base = 10;
    if (text[0] == '0' && text[1] == 'x') {
        text += 2;
        base = 16;
    }
    if (!*text)
        return -1;
    uint64_t number = 0;
    for (; *text; ++text) {
        unsigned digit;
        if (*text >= '0' && *text <= '9')
            digit = (unsigned)(*text - '0');
        else if (base == 16 && *text >= 'a' && *text <= 'f')
            digit = (unsigned)(*text - 'a' + 10);
        else if (base == 16 && *text >= 'A' && *text <= 'F')
            digit = (unsigned)(*text - 'A' + 10);
        else
            return -1;
        if (digit >= base || number > (UINT64_MAX - digit) / base)
            return -1;
        number = number * base + digit;
    }
    *value = number;
    return 0;
}

static int metadata_operation(const char *command, enum mp_operation *operation) {
    static const struct {
        const char *name;
        enum mp_operation operation;
    } commands[] = {{"getVersion", MP_VERSION},  {"getIdentity", MP_IDENTITY},
                    {"getTitleID", MP_TITLE_ID}, {"getMainNsoBase", MP_MAIN_BASE},
                    {"getBuildID", MP_BUILD_ID}, {"getHeapBase", MP_HEAP_BASE}};
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (strcmp(command, commands[i].name) == 0) {
            *operation = commands[i].operation;
            return 0;
        }
    }
    return -1;
}

int mp_parse_request(const char *line, size_t length, struct mp_request *out) {
    if (!line || !out || length == 0 || length > MP_MAX_LINE || memchr(line, 0, length))
        return -1;
    char buffer[MP_MAX_LINE + 1];
    memcpy(buffer, line, length);
    buffer[length] = 0;
    char *parts[1 + 2 * MP_MAX_RANGES + 1];
    size_t part_count = 0;
    char *cursor = buffer;
    while (*cursor) {
        if (*cursor == ' ' || *cursor == '\r' || *cursor == '\n' || *cursor == '\t')
            return -1;
        if (part_count >= sizeof(parts) / sizeof(parts[0]))
            return -1;
        parts[part_count++] = cursor;
        while (*cursor && *cursor != ' ') {
            if (*cursor == '\r' || *cursor == '\n' || *cursor == '\t')
                return -1;
            ++cursor;
        }
        if (!*cursor)
            break;
        *cursor++ = 0;
        if (!*cursor)
            return -1;
    }
    struct mp_request request = {0};
    if (metadata_operation(parts[0], &request.operation) == 0) {
        if (part_count != 1)
            return -1;
    } else if (strcmp(parts[0], "isProgramRunning") == 0) {
        if (part_count != 2 || parse_number(parts[1], &request.program_id) != 0)
            return -1;
        request.operation = MP_IS_RUNNING;
    } else {
        if (strcmp(parts[0], "peekMain") == 0)
            request.operation = MP_PEEK_MAIN;
        else if (strcmp(parts[0], "peekAbsolute") == 0)
            request.operation = MP_PEEK_ABSOLUTE;
        else if (strcmp(parts[0], "peekAbsoluteMulti") == 0)
            request.operation = MP_PEEK_ABSOLUTE_MULTI;
        else
            return -1;
        if (part_count < 3 || part_count % 2 == 0 ||
            (request.operation != MP_PEEK_ABSOLUTE_MULTI && part_count != 3))
            return -1;
        request.count = (uint32_t)((part_count - 1) / 2);
        if (request.count > MP_MAX_RANGES)
            return -1;
        for (uint32_t i = 0; i < request.count; ++i) {
            uint64_t address, length64;
            if (parse_number(parts[1 + i * 2], &address) != 0 ||
                parse_number(parts[2 + i * 2], &length64) != 0 || length64 == 0 ||
                length64 > MP_MAX_READ || address > UINT64_MAX - length64 ||
                length64 > MP_MAX_READ - request.total_length)
                return -1;
            request.ranges[i].address = address;
            request.ranges[i].length = (uint32_t)length64;
            request.total_length += (uint32_t)length64;
        }
    }
    *out = request;
    return 0;
}
