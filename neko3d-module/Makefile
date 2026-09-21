TOP := $(CURDIR)
NEKO := $(TOP)/../neko3d
DLINK := $(TOP)/../extern/libnx-dlink

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
CXX := $(DKA64)/bin/aarch64-none-elf-g++
PY := python3

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
CFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -D__DK_INTERNAL__ -I$(NEKO)/include -I$(DEVKITPRO)/libnx/include -I$(NEKO)/release
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
RENDERER_CFLAGS := $(subst -D__DK_INTERNAL__,, $(CFLAGS))
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,-Map,$(TOP)/neko3d.map

SRCS := $(wildcard $(NEKO)/source/*.cpp) $(wildcard $(NEKO)/source/maxwell/*.cpp)
OBJS := $(patsubst $(NEKO)/source/%.cpp,$(TOP)/build/%.o,$(SRCS)) $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o

all: $(TOP)/neko3d.dnro $(TOP)/neko3d.undef $(TOP)/neko3d.def

$(TOP)/neko3d.def: $(TOP)/neko3d.elf
	$(DKA64)/bin/aarch64-none-elf-nm -D --defined-only $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/neko3d.undef: $(TOP)/neko3d.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/neko3d.dnro: $(TOP)/neko3d.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/neko3d.elf $(TOP)/neko3d.dnro

$(TOP)/neko3d.elf: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)

$(TOP)/build/%.o: $(NEKO)/source/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TOP)/build/stub/stub.o: $(TOP)/stub/stub.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOP)/build/renderer.o: $(TOP)/renderer.c
	@mkdir -p $(dir $@)
	$(CC) $(RENDERER_CFLAGS) -c $< -o $@

$(TOP)/build/shaders/%.dksh.o: $(TOP)/shaders/%.dksh
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/bin2s $< > $(TOP)/build/shaders/$*.dksh.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/shaders/$*.dksh.s -o $@

$(TOP)/build/stub/crt0.o: $(TOP)/stub/crt0.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(TOP)/build $(TOP)/neko3d.elf $(TOP)/neko3d.dnro $(TOP)/neko3d.map $(TOP)/neko3d.undef $(TOP)/neko3d.def

.PHONY: all clean
