#ifndef MEMORY_PEEK_MEMORY_H
#define MEMORY_PEEK_MEMORY_H

#include "server.h"

extern const struct mp_backend mp_switch_backend;
int mp_memory_initialize(void);
void mp_memory_exit(void);

#endif
