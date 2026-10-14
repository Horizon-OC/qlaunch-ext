TOP := $(CURDIR)
DLINK := $(TOP)/../../extern/libnx-dlink
SDK := $(TOP)/../../plugins/include
MINIINI := $(TOP)/../../extern/miniini

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
CXX := $(DKA64)/bin/aarch64-none-elf-g++
PY := python3

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
BASEFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -I$(DEVKITPRO)/libnx/include -I$(SDK) -I$(MINIINI) -I$(TOP)/src -DINI_READONLY -DINI_NOBROWSE -DNDEBUG
CFLAGS := $(BASEFLAGS) -fvisibility=hidden

CXXFLAGS := $(BASEFLAGS) -std=gnu++23 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fvisibility=hidden
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,-Map,$(TOP)/ForwarderPlugin.map

SRCS := $(notdir $(wildcard $(TOP)/src/*.cpp))
OBJS := $(patsubst %.cpp,$(TOP)/build/src/%.o,$(SRCS))
OBJS += $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o

all: $(TOP)/ForwarderPlugin.dnro $(TOP)/ForwarderPlugin.undef

$(TOP)/ForwarderPlugin.undef: $(TOP)/ForwarderPlugin.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/ForwarderPlugin.dnro: $(TOP)/ForwarderPlugin.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/ForwarderPlugin.elf $(TOP)/ForwarderPlugin.dnro

$(TOP)/ForwarderPlugin.elf: $(OBJS)
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
	rm -rf $(TOP)/build $(TOP)/ForwarderPlugin.elf $(TOP)/ForwarderPlugin.dnro $(TOP)/ForwarderPlugin.map $(TOP)/ForwarderPlugin.undef

.PHONY: all clean
