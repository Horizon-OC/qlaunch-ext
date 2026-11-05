TOP := $(CURDIR)
DLINK := $(TOP)/../../../../extern/libnx-dlink
MENUCORE := $(TOP)/../../../src/core
THIRDPARTY := $(TOP)/../../../src/third_party

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
CXX := $(DKA64)/bin/aarch64-none-elf-g++
PY := python3

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
BASEFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -DSTBI_NO_THREAD_LOCALS -I$(DEVKITPRO)/libnx/include -I$(MENUCORE) -I$(THIRDPARTY)
CFLAGS := $(BASEFLAGS) -fvisibility=hidden
CXXFLAGS := $(BASEFLAGS) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fvisibility=hidden -MMD -MP
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,-z,nopack-relative-relocs -Wl,-Map,$(TOP)/Settings.map

SRCS := $(notdir $(wildcard $(TOP)/src/*.cpp))
OBJS := $(patsubst %.cpp,$(TOP)/build/src/%.o,$(SRCS))
OBJS += $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o
OBJS += $(TOP)/build/icons/top_sel_settings.o $(TOP)/build/icons/top_des_settings.o
OBJS += $(TOP)/build/icons/set_00.o $(TOP)/build/icons/set_01.o $(TOP)/build/icons/set_02.o
OBJS += $(TOP)/build/icons/set_03.o $(TOP)/build/icons/set_04.o $(TOP)/build/icons/set_05.o
OBJS += $(TOP)/build/icons/set_06.o $(TOP)/build/icons/set_07.o $(TOP)/build/icons/set_08.o
OBJS += $(TOP)/build/icons/set_09.o $(TOP)/build/icons/set_10.o $(TOP)/build/icons/set_11.o
OBJS += $(TOP)/build/icons/set_12.o $(TOP)/build/icons/set_13.o $(TOP)/build/icons/set_14.o
OBJS += $(TOP)/build/icons/set_15.o $(TOP)/build/icons/set_16.o $(TOP)/build/icons/set_17.o

all: $(TOP)/Settings.dnro $(TOP)/Settings.undef

-include $(TOP)/build/src/*.d

$(TOP)/Settings.undef: $(TOP)/Settings.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/Settings.dnro: $(TOP)/Settings.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/Settings.elf $(TOP)/Settings.dnro

$(TOP)/Settings.elf: $(OBJS)
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

$(TOP)/build/icons/%.o: $(TOP)/../../../assets/hud/%.png
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/bin2s $< > $(TOP)/build/icons/$*.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/icons/$*.s -o $@

clean:
	rm -rf $(TOP)/build $(TOP)/Settings.elf $(TOP)/Settings.dnro $(TOP)/Settings.map $(TOP)/Settings.undef

.PHONY: all clean
