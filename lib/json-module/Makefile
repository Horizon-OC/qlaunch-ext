TOP := $(CURDIR)
JSON := $(TOP)/../../extern/json
DLINK := $(TOP)/../../extern/libnx-dlink

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
CXX := $(DKA64)/bin/aarch64-none-elf-g++
PY := python3

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
BASEFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -I$(DEVKITPRO)/libnx/include -I$(JSON)
CFLAGS := $(BASEFLAGS)
CXXFLAGS := $(BASEFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions -fno-threadsafe-statics
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,--gc-sections -Wl,-Map,$(TOP)/json.map

SRCS := $(notdir $(wildcard $(TOP)/src/*.cpp))
OBJS := $(patsubst %.cpp,$(TOP)/build/src/%.o,$(SRCS))
OBJS += $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o

all: $(TOP)/json.dnro $(TOP)/json.undef $(TOP)/json.def

$(TOP)/json.def: $(TOP)/json.elf
	$(DKA64)/bin/aarch64-none-elf-nm -D --defined-only $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/json.undef: $(TOP)/json.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/json.dnro: $(TOP)/json.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/json.elf $(TOP)/json.dnro

$(TOP)/json.elf: $(OBJS)
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
	rm -rf $(TOP)/build $(TOP)/json.elf $(TOP)/json.dnro $(TOP)/json.map $(TOP)/json.undef $(TOP)/json.def

.PHONY: all clean
