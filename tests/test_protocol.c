#include "protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void good(const char *s, enum mp_operation operation, uint32_t count, uint32_t total) {
    struct mp_request request;
    assert(mp_parse_request(s, strlen(s), &request) == 0);
    assert(request.operation == operation);
    assert(request.count == count);
    assert(request.total_length == total);
}
static void bad(const char *s) {
    struct mp_request request;
    memset(&request, 0xA5, sizeof(request));
    assert(mp_parse_request(s, strlen(s), &request) == -1);
}
int main(void) {
    good("getVersion", MP_VERSION, 0, 0);
    good("getIdentity", MP_IDENTITY, 0, 0);
    good("getTitleID", MP_TITLE_ID, 0, 0);
    good("getMainNsoBase", MP_MAIN_BASE, 0, 0);
    good("getBuildID", MP_BUILD_ID, 0, 0);
    good("getHeapBase", MP_HEAP_BASE, 0, 0);
    good("isProgramRunning 0x0100C2500FC20000", MP_IS_RUNNING, 0, 0);
    good("peekMain 0x20 0x10", MP_PEEK_MAIN, 1, 16);
    good("peekAbsolute 0x1000 0x10", MP_PEEK_ABSOLUTE, 1, 16);
    good("peekAbsolute 0x1000 0x100000", MP_PEEK_ABSOLUTE, 1, MP_MAX_READ);
    good("peekAbsoluteMulti 0x1000 0x10 0x2000 0x20", MP_PEEK_ABSOLUTE_MULTI, 2, 48);
    struct mp_request request;
    const char *multi_request = "peekAbsoluteMulti 0x1000 0x10 0x2000 0x20";
    assert(mp_parse_request(multi_request, strlen(multi_request), &request) == 0);
    assert(request.ranges[0].address == 0x1000 && request.ranges[1].address == 0x2000);
    assert(request.ranges[0].length == 16 && request.ranges[1].length == 32);
    const char *invalid[] = {
        "poke 0x1000 0xAB", "freeze 0x1000", "click A", "peekMainMulti 0x0 0x1",
        "getVersion extra", "peekAbsolute", "peekAbsolute 0x1", "peekAbsolute 0x1 0x0",
        "peekAbsolute 0x1 0x100001", "peekAbsolute 0xFFFFFFFFFFFFFFFF 0x2",
        "peekAbsolute 0xGG 0x1", "peekAbsolute 0x1 -1", "peekAbsolute 1.5 2",
        "peekAbsoluteMulti 0x1 0x80000 0x2 0x80001",
        "peekAbsoluteMulti 0x1 0x1 0x2", "isProgramRunning 0100C2500FC20000",
        "", "getVersion\ngetIdentity", "getVersion\0junk"
    };
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]) - 1; ++i) bad(invalid[i]);
    assert(mp_parse_request(invalid[sizeof(invalid)/sizeof(invalid[0])-1], 15, &request) == -1);
    char many[2049]; memset(many, 'A', 2048); many[2048] = 0;
    assert(mp_parse_request(many, 2048, &request) == -1);
    assert(mp_parse_request(many, 2049, &request) == -1);
    char multi[2048] = "peekAbsoluteMulti";
    for (int i = 0; i < 33; ++i) strcat(multi, " 0x1 0x1");
    bad(multi);
    puts("protocol tests passed");
}
