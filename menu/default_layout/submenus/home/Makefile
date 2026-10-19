TOP := $(CURDIR)
DLINK := $(TOP)/../../../../extern/libnx-dlink
MENUCORE := $(TOP)/../../../src/core
WIDGETS := $(TOP)/../../../src/widgets

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
CXX := $(DKA64)/bin/aarch64-none-elf-g++
PY := python3

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
BASEFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -I$(DEVKITPRO)/libnx/include -I$(MENUCORE) -I$(WIDGETS) -I$(TOP)/src
CFLAGS := $(BASEFLAGS) -fvisibility=hidden
CXXFLAGS := $(BASEFLAGS) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fvisibility=hidden -MMD -MP
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,-z,nopack-relative-relocs -Wl,-Map,$(TOP)/home.map

SRCS := $(notdir $(wildcard $(TOP)/src/*.cpp))
OBJS := $(patsubst %.cpp,$(TOP)/build/src/%.o,$(SRCS))
OBJS += $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o

all: $(TOP)/home.dnro $(TOP)/home.undef

$(TOP)/home.undef: $(TOP)/home.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/home.dnro: $(TOP)/home.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/home.elf $(TOP)/home.dnro

$(TOP)/home.elf: $(OBJS) $(TOP)/Makefile
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)

$(TOP)/build/src/%.o: $(TOP)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TOP)/build/stub/stub.o: $(TOP)/stub/stub.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOP)/build/stub/crt0.o: $(TOP)/stub/crt0.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(TOP)/build $(TOP)/home.elf $(TOP)/home.dnro $(TOP)/home.map $(TOP)/home.undef

.PHONY: all clean
