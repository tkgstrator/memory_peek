#ifndef MEMORY_PEEK_TEST_SWITCH_H
#define MEMORY_PEEK_TEST_SWITCH_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef int32_t s32;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int Result;
typedef u32 Handle;
typedef struct {
    int kind;
} Service;

typedef struct {
    u64 base_address;
    u64 size;
    u8 build_id[0x20];
} LoaderModuleInfo;

typedef enum { InfoType_HeapRegionAddress = 4 } InfoType;

#define R_FAILED(result) ((result) != 0)
#define R_SUCCEEDED(result) ((result) == 0)

Service *pmdmntGetServiceSession(void);
Result ldrDmntInitialize(void);
void ldrDmntExit(void);
Result ldrDmntGetProcessModuleInfo(u64 pid, LoaderModuleInfo *out, u32 max, s32 *count);
Result pminfoInitialize(void);
void pminfoExit(void);
Result pminfoGetProgramId(u64 *program_id, u64 pid);
Result svcDebugActiveProcess(Handle *handle, u64 pid);
Result svcReadDebugProcessMemory(void *buffer, Handle debug, u64 address, u64 size);
Result svcGetInfo(u64 *out, InfoType type, Handle handle, u64 sub_type);
Result svcCloseHandle(Handle handle);
Result test_service_dispatch_out(Service *service, int command, void *output, size_t size);

#define serviceDispatchOut(service, command, output)                                               \
    test_service_dispatch_out(service, command, &(output), sizeof(output))

#endif
