TOP := $(CURDIR)
DLINK := $(TOP)/../extern/libnx-dlink
# SDL3 headers (extern/SDL-dlink submodule once committed; override for dev).
SDLDIR ?= $(TOP)/../extern/SDL-dlink

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
AS := $(DKA64)/bin/aarch64-none-elf-gcc
CXX := $(DKA64)/bin/aarch64-none-elf-g++
PY := python3
UAM := $(DEVKITPRO)/tools/bin/uam

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
BASEFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -DINI_READONLY -DINI_NOBROWSE -I$(SDLDIR)/include -I$(DEVKITPRO)/libnx/include -I$(DLINK)/include -I$(TOP)/../extern/miniini -I$(TOP)/src
CFLAGS := $(BASEFLAGS)
CXXFLAGS := $(BASEFLAGS) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -MMD -MP
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,-Map,$(TOP)/menu.map -Wl,-z,nopack-relative-relocs -Wl,-Bsymbolic -Wl,--gc-sections

SRCS := $(wildcard $(TOP)/src/*.cpp $(TOP)/src/core/*.cpp $(TOP)/src/widgets/*.cpp)
OBJS := $(patsubst $(TOP)/%.cpp,$(TOP)/build/%.o,$(SRCS))
OBJS += $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o
OBJS += $(TOP)/build/assets/layout.o
OBJS += $(TOP)/build/assets/theme_light.o $(TOP)/build/assets/theme_dark.o $(TOP)/build/assets/themes.o

all: $(TOP)/menu.dnro $(TOP)/menu.undef

-include $(OBJS:.o=.d)

$(TOP)/menu.undef: $(TOP)/menu.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/menu.dnro: $(TOP)/menu.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/menu.elf $(TOP)/menu.dnro

$(TOP)/menu.elf: $(OBJS) $(TOP)/Makefile
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) -lgcc

$(TOP)/build/%.o: $(TOP)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TOP)/build/stub/stub.o: $(TOP)/stub/stub.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOP)/build/shaders/%_vsh.dksh.o: $(TOP)/shaders/%_vsh.glsl
	@mkdir -p $(dir $@)
	$(UAM) -s vert -o $(TOP)/build/shaders/$*_vsh.dksh $<
	$(DEVKITPRO)/tools/bin/bin2s $(TOP)/build/shaders/$*_vsh.dksh > $(TOP)/build/shaders/$*_vsh.dksh.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/shaders/$*_vsh.dksh.s -o $@

$(TOP)/build/shaders/%_fsh.dksh.o: $(TOP)/shaders/%_fsh.glsl
	@mkdir -p $(dir $@)
	$(UAM) -s frag -o $(TOP)/build/shaders/$*_fsh.dksh $<
	$(DEVKITPRO)/tools/bin/bin2s $(TOP)/build/shaders/$*_fsh.dksh > $(TOP)/build/shaders/$*_fsh.dksh.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/shaders/$*_fsh.dksh.s -o $@

$(TOP)/build/assets/font.o: $(TOP)/assets/font.otf
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/bin2s $< > $(TOP)/build/assets/font.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/assets/font.s -o $@

$(TOP)/build/assets/iconfont.o: $(TOP)/assets/switch_icons.ttf
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/bin2s $< > $(TOP)/build/assets/iconfont.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/assets/iconfont.s -o $@

$(TOP)/build/assets/hud/%.o: $(TOP)/assets/hud/%.png
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/bin2s $< > $(TOP)/build/assets/hud/$*.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/assets/hud/$*.s -o $@

$(TOP)/build/assets/sfx/%.o: $(TOP)/assets/sfx/%.wav
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/bin2s $< > $(TOP)/build/assets/sfx/$*.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/assets/sfx/$*.s -o $@

$(TOP)/build/assets/%.o: $(TOP)/src/assets/%.json
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/bin2s $< > $(TOP)/build/assets/$*.s
	$(CC) $(ASFLAGS) -c $(TOP)/build/assets/$*.s -o $@

$(TOP)/build/stub/crt0.o: $(TOP)/stub/crt0.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(TOP)/build $(TOP)/menu.elf $(TOP)/menu.dnro $(TOP)/menu.map $(TOP)/menu.undef

.PHONY: all clean

