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
    if (R_FAILED(smInitialize())) diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
    if (R_FAILED(pmdmntInitialize()) || mp_memory_initialize() != 0 ||
        R_FAILED(socketInitializeDefault())) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
    }
}

void __appExit(void) {
    socketExit();
    mp_memory_exit();
    pmdmntExit();
    smExit();
}

int main(void) {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return 1;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(6000);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0) { close(listener); return 2; }
    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;
        mp_serve_client(client, &mp_switch_backend, 5);
        close(client);
    }
}
