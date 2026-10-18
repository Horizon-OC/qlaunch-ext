TOP := $(CURDIR)
DLINK := $(TOP)/../../../extern/libnx-dlink
MENUCORE := $(TOP)/../../src/core

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
CXX := $(DKA64)/bin/aarch64-none-elf-g++
PY := python3

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
BASEFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -I$(DEVKITPRO)/libnx/include -I$(MENUCORE)
CFLAGS := $(BASEFLAGS) -fvisibility=hidden
CXXFLAGS := $(BASEFLAGS) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fvisibility=hidden -MMD -MP
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,-z,nopack-relative-relocs -Wl,-Map,$(TOP)/EShop.map

SRCS := $(notdir $(wildcard $(TOP)/src/*.cpp))
OBJS := $(patsubst %.cpp,$(TOP)/build/src/%.o,$(SRCS))
OBJS += $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o
OBJS += $(TOP)/build/icons/top_sel_eshop.o $(TOP)/build/icons/top_des_eshop.o

all: $(TOP)/EShop.dnro $(TOP)/EShop.undef

-include $(TOP)/build/src/*.d

$(TOP)/EShop.undef: $(TOP)/EShop.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/EShop.dnro: $(TOP)/EShop.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/EShop.elf $(TOP)/EShop.dnro

$(TOP)/EShop.elf: $(OBJS)
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

$(TOP)/build/icons/%.o: $(TOP)/../../assets/hud/%.png
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/bin2s $< > $(TOP)/build/icons/$*.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/icons/$*.s -o $@

clean:
	rm -rf $(TOP)/build $(TOP)/EShop.elf $(TOP)/EShop.dnro $(TOP)/EShop.map $(TOP)/EShop.undef

.PHONY: all clean
