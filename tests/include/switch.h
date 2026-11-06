#ifndef MEMORY_PEEK_TEST_SWITCH_H
#define MEMORY_PEEK_TEST_SWITCH_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint64_t u64;
typedef int Result;
typedef struct {
    int kind;
} Service;

#define R_FAILED(result) ((result) != 0)
#define R_SUCCEEDED(result) ((result) == 0)

Result smGetService(Service *service, const char *name);
void serviceClose(Service *service);
Service *pmdmntGetServiceSession(void);
Result pmdmntGetApplicationProcessId(u64 *pid);
Result test_service_dispatch_out(Service *service, int command, void *output, size_t size);
Result test_service_dispatch_in(Service *service, int command, const void *input, size_t input_size,
                                void *output, size_t output_size);

#define serviceDispatchOut(service, command, output)                                               \
    test_service_dispatch_out(service, command, &(output), sizeof(output))
#define serviceDispatchIn(service, command, input, ...)                                            \
    test_service_dispatch_in(service, command, &(input), sizeof(input), destination, length)

#endif
