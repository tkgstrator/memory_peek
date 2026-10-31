#include "server.h"
#include "protocol.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>

static int send_all(int fd, const char *data, size_t length) {
    while (length) {
        ssize_t count = send(fd, data, length, 0);
        if (count <= 0)
            return -1;
        data += count;
        length -= (size_t)count;
    }
    return 0;
}
static int send_text(int fd, const char *text) { return send_all(fd, text, strlen(text)); }
static const char hex[] = "0123456789ABCDEF";

static int reply(int fd, const struct mp_request *request, const struct mp_backend *backend) {
    if (request->operation == MP_VERSION)
        return send_text(fd, "1.0.0\r\n");
    if (request->operation == MP_IDENTITY)
        return send_text(fd, "memory_peek/1\r\n");

    struct mp_snapshot snapshot = {0};
    int begin_result = backend->begin(backend->context, &snapshot);
    if (begin_result != 0) {
        if (begin_result == -2)
            return send_text(fd, "ERR READER_UNAVAILABLE\r\n");
        if (request->operation == MP_IS_RUNNING)
            return send_text(fd, "0\r\n");
        return send_text(fd, "ERR NO_GAME\r\n");
    }
    int result = 0;
    char response[32];
    switch (request->operation) {
    case MP_TITLE_ID:
        snprintf(response, sizeof(response), "%016" PRIX64 "\r\n", snapshot.title_id);
        break;
    case MP_MAIN_BASE:
        snprintf(response, sizeof(response), "%016" PRIX64 "\r\n", snapshot.main_base);
        break;
    case MP_HEAP_BASE:
        snprintf(response, sizeof(response), "%016" PRIX64 "\r\n", snapshot.heap_base);
        break;
    case MP_BUILD_ID:
        for (size_t i = 0; i < sizeof(snapshot.build_id); ++i) {
            response[2 * i] = hex[snapshot.build_id[i] >> 4];
            response[2 * i + 1] = hex[snapshot.build_id[i] & 15];
        }
        response[16] = '\r';
        response[17] = '\n';
        response[18] = 0;
        break;
    case MP_IS_RUNNING:
        strcpy(response, snapshot.title_id == request->program_id ? "1\r\n" : "0\r\n");
        break;
    default:
        response[0] = 0;
        break;
    }
    if (response[0]) {
        result = send_text(fd, backend->still_current(backend->context, &snapshot)
                                   ? response
                                   : "ERR GAME_CHANGED\r\n");
    } else {
        uint8_t *bytes = malloc(request->total_length);
        if (!bytes) {
            result = send_text(fd, "ERR READ_FAILED\r\n");
        } else {
            size_t offset = 0;
            int failed = 0;
            for (uint32_t i = 0; i < request->count && !failed; ++i) {
                uint64_t base = request->ranges[i].address;
                if (request->operation == MP_PEEK_MAIN) {
                    if (base > UINT64_MAX - snapshot.main_base ||
                        base + snapshot.main_base > UINT64_MAX - request->ranges[i].length) {
                        failed = 1;
                        break;
                    }
                    base += snapshot.main_base;
                }
                uint32_t remaining = request->ranges[i].length;
                while (remaining) {
                    size_t chunk = remaining > 65536 ? 65536 : remaining;
                    if (backend->read(backend->context, &snapshot, base, bytes + offset, chunk) !=
                        0) {
                        failed = 1;
                        break;
                    }
                    base += chunk;
                    offset += chunk;
                    remaining -= (uint32_t)chunk;
                }
            }
            if (failed)
                result = send_text(fd, "ERR READ_FAILED\r\n");
            else if (!backend->still_current(backend->context, &snapshot))
                result = send_text(fd, "ERR GAME_CHANGED\r\n");
            else {
                char *encoded = malloc((size_t)request->total_length * 2 + 2);
                if (!encoded)
                    result = send_text(fd, "ERR READ_FAILED\r\n");
                else {
                    for (size_t i = 0; i < request->total_length; ++i) {
                        encoded[i * 2] = hex[bytes[i] >> 4];
                        encoded[i * 2 + 1] = hex[bytes[i] & 15];
                    }
                    encoded[request->total_length * 2] = '\r';
                    encoded[request->total_length * 2 + 1] = '\n';
                    result = send_all(fd, encoded, (size_t)request->total_length * 2 + 2);
                    free(encoded);
                }
            }
            free(bytes);
        }
    }
    backend->end(backend->context, &snapshot);
    return result;
}

void mp_serve_client(int socket_fd, const struct mp_backend *backend, int idle_seconds) {
    struct timeval timeout = {idle_seconds, 0};
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0)
        return;
    char line[MP_MAX_LINE + 2];
    size_t length = 0;
    while (1) {
        char character;
        ssize_t count = recv(socket_fd, &character, 1, 0);
        if (count <= 0)
            return;
        if (character == '\n') {
            if (length && line[length - 1] == '\r')
                --length;
            struct mp_request request;
            if (mp_parse_request(line, length, &request) != 0) {
                if (send_text(socket_fd, "ERR INVALID_REQUEST\r\n") != 0)
                    return;
            } else if (reply(socket_fd, &request, backend) != 0)
                return;
            length = 0;
        } else {
            if (length >= MP_MAX_LINE + 1) {
                send_text(socket_fd, "ERR INVALID_REQUEST\r\n");
                return;
            }
            line[length++] = character;
        }
    }
}
