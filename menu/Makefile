TOP := $(CURDIR)
NEKO := $(TOP)/../neko3d
DLINK := $(TOP)/../extern/libnx-dlink

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
PY := python3

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
CFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -I$(NEKO)/include -I$(DEVKITPRO)/libnx/include
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,-Map,$(TOP)/menu.map

OBJS := $(TOP)/build/menu.o $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o $(TOP)/build/shaders/triangle_vsh.dksh.o $(TOP)/build/shaders/color_fsh.dksh.o

all: $(TOP)/menu.dnro $(TOP)/menu.undef

$(TOP)/menu.undef: $(TOP)/menu.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/menu.dnro: $(TOP)/menu.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/menu.elf $(TOP)/menu.dnro

$(TOP)/menu.elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(TOP)/build/menu.o: $(TOP)/src/menu.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOP)/build/stub/stub.o: $(TOP)/stub/stub.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOP)/build/shaders/%.dksh.o: $(TOP)/shaders/%.dksh
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/bin2s $< > $(TOP)/build/shaders/$*.dksh.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/shaders/$*.dksh.s -o $@

$(TOP)/build/stub/crt0.o: $(TOP)/stub/crt0.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(TOP)/build $(TOP)/menu.elf $(TOP)/menu.dnro $(TOP)/menu.map $(TOP)/menu.undef

.PHONY: all clean
