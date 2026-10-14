TOP := $(CURDIR)
MINIINI := $(TOP)/../../extern/miniini
DLINK := $(TOP)/../../extern/libnx-dlink

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
PY := python3

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
CFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -I$(DEVKITPRO)/libnx/include -I$(MINIINI) -DINI_READONLY -DINI_NOBROWSE -DNDEBUG
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,--gc-sections -Wl,-Map,$(TOP)/miniini.map

all: $(TOP)/miniini.dnro $(TOP)/miniini.undef $(TOP)/miniini.def

$(TOP)/miniini.def: $(TOP)/miniini.elf
	$(DKA64)/bin/aarch64-none-elf-nm -D --defined-only $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/miniini.undef: $(TOP)/miniini.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/miniini.dnro: $(TOP)/miniini.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/miniini.elf $(TOP)/miniini.dnro

$(TOP)/miniini.elf: $(TOP)/build/minIni.o $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o
	$(CC) $(LDFLAGS) -o $@ $^

$(TOP)/build/minIni.o: $(MINIINI)/minIni.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOP)/build/stub/stub.o: $(TOP)/stub/stub.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOP)/build/stub/crt0.o: $(TOP)/stub/crt0.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(TOP)/build $(TOP)/miniini.elf $(TOP)/miniini.dnro $(TOP)/miniini.map $(TOP)/miniini.undef $(TOP)/miniini.def

.PHONY: all clean
