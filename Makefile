HOST_CC ?= cc
HOST_CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -Isource
SWITCH_CC = $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-gcc
SWITCH_TOOLS = $(DEVKITPRO)/tools/bin
SWITCH_CFLAGS = -std=gnu11 -O2 -Wall -Wextra -Werror -D__SWITCH__ -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -ffunction-sections -fdata-sections -Isource -I$(DEVKITPRO)/libnx/include
SWITCH_LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -L$(DEVKITPRO)/libnx/lib -lnx
SWITCH_SOURCES = source/main.c source/memory.c source/server.c source/protocol.c

.PHONY: test clean switch
test: build/test_protocol build/fake_server
	./build/test_protocol
	python3 -m unittest discover -s tests -p 'test_*.py' -v

build/test_protocol: tests/test_protocol.c source/protocol.c source/protocol.h
	@mkdir -p build
	$(HOST_CC) $(HOST_CFLAGS) tests/test_protocol.c source/protocol.c -o $@

build/fake_server: tests/fake_server.c source/server.c source/server.h source/protocol.c source/protocol.h
	@mkdir -p build
	$(HOST_CC) $(HOST_CFLAGS) tests/fake_server.c source/server.c source/protocol.c -o $@

switch: build/switch/memory_peek.nsp
	python3 tools/audit.py build/switch --hactool $(SWITCH_TOOLS)/hactool --nm $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-nm

build/switch/memory_peek.elf: $(SWITCH_SOURCES) source/server.h source/protocol.h source/memory.h
	@mkdir -p build/switch
	$(SWITCH_CC) $(SWITCH_CFLAGS) $(SWITCH_SOURCES) $(SWITCH_LDFLAGS) -o $@

build/switch/memory_peek.nso: build/switch/memory_peek.elf
	$(SWITCH_TOOLS)/elf2nso $< $@

build/switch/memory_peek.npdm: memory_peek.json
	@mkdir -p build/switch
	$(SWITCH_TOOLS)/npdmtool $< $@

build/switch/memory_peek.nsp: build/switch/memory_peek.nso build/switch/memory_peek.npdm
	@mkdir -p build/switch/exefs
	cp build/switch/memory_peek.nso build/switch/exefs/main
	cp build/switch/memory_peek.npdm build/switch/exefs/main.npdm
	$(SWITCH_TOOLS)/build_pfs0 build/switch/exefs $@

clean:
	rm -rf build
