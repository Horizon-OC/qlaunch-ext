TOP := $(CURDIR)
NEKO := $(TOP)/../neko3d
DLINK := $(TOP)/../extern/libnx-dlink

DEVKITPRO ?= /opt/devkitpro
DKA64 := $(DEVKITPRO)/devkitA64
CC := $(DKA64)/bin/aarch64-none-elf-gcc
AS := $(DKA64)/bin/aarch64-none-elf-gcc
CXX := $(DKA64)/bin/aarch64-none-elf-g++
PY := python3
UAM := $(DEVKITPRO)/tools/bin/uam

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC
BASEFLAGS := -g -O2 -ffunction-sections -fdata-sections $(ARCH) -D__SWITCH__ -I$(NEKO)/include -I$(DEVKITPRO)/libnx/include -I$(TOP)/src
CFLAGS := $(BASEFLAGS)
CXXFLAGS := $(BASEFLAGS) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -MMD -MP
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DLINK)/tools/module.specs -g $(ARCH) -shared -nodefaultlibs -Wl,--export-dynamic -Wl,-Map,$(TOP)/menu.map -Wl,-z,nopack-relative-relocs

SRCS := $(wildcard $(TOP)/src/*.cpp $(TOP)/src/core/*.cpp $(TOP)/src/widgets/*.cpp)
OBJS := $(patsubst $(TOP)/%.cpp,$(TOP)/build/%.o,$(SRCS))
OBJS += $(TOP)/build/stub/stub.o $(TOP)/build/stub/crt0.o
OBJS += $(TOP)/build/shaders/quad_vsh.dksh.o $(TOP)/build/shaders/color_fsh.dksh.o
OBJS += $(TOP)/build/shaders/text_vsh.dksh.o $(TOP)/build/shaders/text_fsh.dksh.o
OBJS += $(TOP)/build/shaders/panel_vsh.dksh.o $(TOP)/build/shaders/panel_fsh.dksh.o
OBJS += $(TOP)/build/shaders/icon_vsh.dksh.o $(TOP)/build/shaders/icon_fsh.dksh.o
OBJS += $(TOP)/build/shaders/bg_fsh.dksh.o
OBJS += $(TOP)/build/shaders/hl_vsh.dksh.o $(TOP)/build/shaders/hl_fsh.dksh.o
OBJS += $(TOP)/build/assets/font.o $(TOP)/build/assets/iconfont.o $(TOP)/build/assets/layout.o
OBJS += $(TOP)/build/assets/hud/hud_wifi.o $(TOP)/build/assets/hud/hud_battery.o
OBJS += $(TOP)/build/assets/hud/hud_user.o $(TOP)/build/assets/hud/hud_btn_a.o
OBJS += $(TOP)/build/assets/hud/hud_btn_plus.o
OBJS += $(TOP)/build/assets/hud/hud_folder.o $(TOP)/build/assets/hud/hud_vgccard.o
OBJS += $(TOP)/build/assets/hud/top_sel_home.o $(TOP)/build/assets/hud/top_sel_connect.o
OBJS += $(TOP)/build/assets/hud/top_sel_eshop.o $(TOP)/build/assets/hud/top_sel_album.o
OBJS += $(TOP)/build/assets/hud/top_sel_settings.o $(TOP)/build/assets/hud/top_des_home.o
OBJS += $(TOP)/build/assets/hud/top_des_connect.o $(TOP)/build/assets/hud/top_des_eshop.o
OBJS += $(TOP)/build/assets/hud/top_des_album.o $(TOP)/build/assets/hud/top_des_settings.o
OBJS += $(TOP)/build/assets/hud/marker_sheet.o
OBJS += $(TOP)/build/assets/theme_light.o $(TOP)/build/assets/theme_dark.o $(TOP)/build/assets/themes.o
OBJS += $(TOP)/build/assets/sfx/s2_cursor_move.o $(TOP)/build/assets/sfx/s2_button_click.o $(TOP)/build/assets/sfx/s2_back_select.o
OBJS += $(TOP)/build/assets/sfx/s2_album.o $(TOP)/build/assets/sfx/s2_settings.o $(TOP)/build/assets/sfx/s2_home_menu.o
OBJS += $(TOP)/build/assets/sfx/s2_eshop.o $(TOP)/build/assets/sfx/s2_game_chat.o $(TOP)/build/assets/sfx/s2_standby.o
OBJS += $(TOP)/build/assets/sfx/news.o $(TOP)/build/assets/sfx/virtual_game_cards.o

all: $(TOP)/menu.dnro $(TOP)/menu.undef

-include $(OBJS:.o=.d)

$(TOP)/menu.undef: $(TOP)/menu.elf
	$(DKA64)/bin/aarch64-none-elf-nm -u -D $< | awk '{print $$NF}' | sort -u > $@

$(TOP)/menu.dnro: $(TOP)/menu.elf
	$(PY) $(DLINK)/tools/elf2dnro.py $(TOP)/menu.elf $(TOP)/menu.dnro

$(TOP)/menu.elf: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)

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

