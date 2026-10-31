#include "server.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct scenario { const char *name; };
static int begin(void *context, struct mp_snapshot *snapshot) {
    struct scenario *scenario = context;
    if (strcmp(scenario->name, "no_game") == 0) return -1;
    if (strcmp(scenario->name, "unavailable") == 0) return -2;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->pid = 3;
    snapshot->title_id = 0x0100C2500FC20000ULL;
    snapshot->main_base = 0x1000;
    snapshot->heap_base = 0x8000;
    for (int i = 0; i < 8; ++i) snapshot->build_id[i] = (uint8_t)(0xA0 + i);
    return 0;
}
static int read_memory(void *context, const struct mp_snapshot *snapshot, uint64_t address,
                       uint8_t *destination, size_t length) {
    struct scenario *scenario = context;
    (void)snapshot;
    if (strcmp(scenario->name, "read_fail") == 0 && address >= 0x2000) return -1;
    for (size_t i = 0; i < length; ++i) destination[i] = (uint8_t)((address + i) & 0xff);
    return 0;
}
static int still_current(void *context, const struct mp_snapshot *snapshot) {
    (void)snapshot;
    return strcmp(((struct scenario *)context)->name, "switch") != 0;
}
static void end(void *context, struct mp_snapshot *snapshot) { (void)context; (void)snapshot; }
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    struct scenario scenario = {argv[1]};
    struct mp_backend backend = {&scenario, begin, read_memory, still_current, end};
    int server = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (server < 0 || bind(server, (struct sockaddr *)&address, sizeof(address)) || listen(server, 1)) return 3;
    socklen_t length = sizeof(address);
    if (getsockname(server, (struct sockaddr *)&address, &length)) return 4;
    printf("%u\n", (unsigned)ntohs(address.sin_port));
    fflush(stdout);
    int client = accept(server, NULL, NULL);
    if (client >= 0) { mp_serve_client(client, &backend, 1); close(client); }
    close(server);
    return 0;
}
