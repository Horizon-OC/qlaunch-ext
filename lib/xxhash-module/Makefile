TOP := $(CURDIR)
XXHASH := $(TOP)/../../extern/xxhash
DLINK := $(TOP)/../../extern/libnx-dlink

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
PY := python3

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
CFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -I$(DEVKITPRO)/libnx/include -I$(XXHASH)
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,--gc-sections -Wl,-Map,$(TOP)/xxhash.map

SRCS := $(notdir $(wildcard $(TOP)/src/*.c))
OBJS := $(patsubst %.c,$(TOP)/build/src/%.o,$(SRCS))
OBJS += $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o

all: $(TOP)/xxhash.dnro $(TOP)/xxhash.undef $(TOP)/xxhash.def

$(TOP)/xxhash.def: $(TOP)/xxhash.elf
	$(DKA64)/bin/aarch64-none-elf-nm -D --defined-only $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/xxhash.undef: $(TOP)/xxhash.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/xxhash.dnro: $(TOP)/xxhash.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/xxhash.elf $(TOP)/xxhash.dnro

$(TOP)/xxhash.elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(TOP)/build/src/%.o: $(TOP)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOP)/build/stub/stub.o: $(TOP)/stub/stub.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOP)/build/stub/crt0.o: $(TOP)/stub/crt0.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(TOP)/build $(TOP)/xxhash.elf $(TOP)/xxhash.dnro $(TOP)/xxhash.map $(TOP)/xxhash.undef $(TOP)/xxhash.def

.PHONY: all clean
