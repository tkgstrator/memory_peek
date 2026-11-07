#include "memory.h"
#include <switch.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/* Sysmodule: no applet lifecycle or filesystem mount. */
u32 __nx_applet_type = AppletType_None;

void __libnx_initheap(void) {
    static u8 heap[6 * 1024 * 1024];
    extern void *fake_heap_start, *fake_heap_end;
    fake_heap_start = heap;
    fake_heap_end = heap + sizeof(heap);
}

void __appInit(void) {
    if (R_FAILED(smInitialize()))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
    /* Boot services can register after this sysmodule starts. */
    Result result = 0;
    for (int attempt = 0; attempt < 60; ++attempt) {
        result = pmdmntInitialize();
        if (R_SUCCEEDED(result))
            break;
        svcSleepThread(500000000);
    }
    if (R_FAILED(result))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
    /* Socket initialization is retried after transient early-boot failures. */
    for (int attempt = 0; attempt < 60; ++attempt) {
        result = socketInitializeDefault();
        if (R_SUCCEEDED(result))
            break;
        svcSleepThread(500000000);
    }
    if (R_FAILED(result))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
    /* ldr:dmnt and pm:info initialize on demand after the listener starts. */
}

void __appExit(void) {
    socketExit();
    mp_memory_exit();
    pmdmntExit();
    smExit();
}

/* Returns a listening socket, or -1 with nothing left open. */
static int open_listener(void) {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
        return -1;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(6000);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0) {
        close(listener);
        return -1;
    }
    return listener;
}

int main(void) {
    /* Retry with bounded backoff (0.5 s doubling to 5 s) instead of exiting. */
    int listener;
    for (u64 delay = 500000000ULL; (listener = open_listener()) < 0;
         delay = delay < 5000000000ULL ? delay * 2 : 5000000000ULL)
        svcSleepThread(delay);
    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            svcSleepThread(100000000);
            continue;
        }
        mp_serve_client(client, &mp_switch_backend, 5);
        close(client);
    }
}
