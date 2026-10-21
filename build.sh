#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TID="0100000000001000"
PKG="$ROOT/dist"
DEST="$PKG/atmosphere/contents/$TID"

if [ -z "${DEVKITPRO:-}" ]; then
    if [ -d "/opt/devkitpro" ]; then
        DEVKITPRO="/opt/devkitpro"
        export DEVKITPRO
    else
        echo "error: DEVKITPRO is not set" >&2
        exit 1
    fi
fi

NEKOMOD="$ROOT/extern/SDL-dlink/extern/neko3d/neko3d-module"
NEKOSRC="$ROOT/extern/SDL-dlink/extern/neko3d"
FFDIR="$ROOT/extern/FFmpeg-dlink"

# newer <target> <roots...>: true when target is missing or any file under
# roots is newer. Pure mtime vs real artifacts (no state files), so slow
# no-op makes are skipped. Caveat shared with SDL's build.sh: after
# *deleting* a source, force with a clean of that module's build dir.
newer() {
    tgt=$1; shift
    [ -e "$tgt" ] || return 0
    roots=""
    for r in "$@"; do
        [ -e "$r" ] && roots="$roots $r"
    done
    [ -z "$roots" ] && return 1
    # unquoted roots: intentional word splitting into find roots
    # shellcheck disable=SC2086
    [ -n "$(find $roots -newer "$tgt" -print -quit 2>/dev/null)" ]
}

if newer "$NEKOMOD/neko3d.dnro" "$NEKOMOD/Makefile" "$NEKOMOD/stub" \
        "$NEKOSRC/source" "$NEKOSRC/include"; then
    make -C "$NEKOMOD" -j"$(nproc)"
fi
# Shared INI parser for plugins (tiny, always rebuilt).
make -C "$ROOT/lib/miniini-module" -j"$(nproc)"
# Shared JSON reads for menus and plugins (tiny, always rebuilt).
make -C "$ROOT/lib/json-module" -j"$(nproc)"
# Shared hashing for menus and plugins.
make -C "$ROOT/lib/xxhash-module" -j"$(nproc)"
# ffmpeg static archives for headers + dnro link (built once, gitignored).
if [ ! -f "$FFDIR/install/lib/libavcodec.a" ]; then
    sh "$ROOT/build-ffmpeg.sh"
fi
# ffmpeg shared module for sdmc:/lib (album submenu imports av_* from it).
if [ ! -d "$FFDIR/dnro" ]; then
    echo "error: $FFDIR/dnro missing; run: git submodule update --init extern/FFmpeg-dlink" >&2
    exit 1
fi
make -C "$FFDIR/dnro" -j"$(nproc)" FFPREFIX="$FFDIR/install" DLINK="$ROOT/extern/libnx-dlink"
make -C "$ROOT/menu" -j"$(nproc)"
make -C "$ROOT/menu/default_layout/layout" -j"$(nproc)"
if [ -d "$ROOT/menu/default_layout/submenus" ]; then
    for p in "$ROOT"/menu/default_layout/submenus/*/; do
        if [ -f "${p}Makefile" ]; then
            make -C "$p" -j"$(nproc)"
        fi
    done
fi
if [ -d "$ROOT/plugins" ]; then
    for p in "$ROOT"/plugins/*/; do
        if [ -f "${p}Makefile" ]; then
            make -C "$p" -j"$(nproc)"
        fi
    done
fi

# NRO payload
if newer "$ROOT/loader/nroloader/nroloader.nsp" "$ROOT/loader/nroloader/src" \
        "$ROOT/loader/nroloader/Makefile" "$ROOT/loader/nroloader/nroloader.json"; then
    make -C "$ROOT/loader/nroloader" -j"$(nproc)"
fi
make -C "$ROOT/loader" -j"$(nproc)"
HOST_ELF="$ROOT/loader/loader.elf" sh "$ROOT/extern/SDL-dlink/build.sh"

NM="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-nm"
FFDEF="$FFDIR/dnro/ffmpeg.def"
# Menu ABI check: every menu import (including SDL_*) must resolve from the
# loader, sdl3, neko3d, or the lib modules. A menu that imports what nobody
# provides dies at dlopen with a bare dlink COULD_NOT_RESOLVE.
"$NM" -D -u "$ROOT/menu/menu.elf" | awk "{print \$NF}" | sort -u > "$ROOT/build.menuneed"
{ "$NM" -D --defined-only "$ROOT/loader/loader.elf" | awk "{print \$NF}"
  cat "$ROOT/extern/SDL-dlink/dnro/sdl3.def" "$NEKOMOD/neko3d.def" \
      "$ROOT/lib/miniini-module/miniini.def" "$ROOT/lib/json-module/json.def" \
      "$ROOT/lib/xxhash-module/xxhash.def" "$FFDEF"
} | tr -d '\r' | sort -u > "$ROOT/build.menuhave"
MMISSING=$(comm -23 "$ROOT/build.menuneed" "$ROOT/build.menuhave" || true)
rm -f "$ROOT/build.menuneed" "$ROOT/build.menuhave"
if [ -n "$MMISSING" ]; then
    echo "error: $ROOT/menu/menu.elf bad imports:" >&2
    echo "$MMISSING" >&2
    exit 1
fi
echo "menu import check passed"
# ffmpeg ABI check: the shared module may only import what the loader host
# provides (libc/libnx/pthreads/nv). Anything else dies at dlopen.
"$NM" -D -u "$FFDIR/dnro/ffmpeg.elf" | awk "{print \$NF}" | sort -u > "$ROOT/build.ffneed"
{ "$NM" -D --defined-only "$ROOT/loader/loader.elf" | awk "{print \$NF}"
} | tr -d '\r' | sort -u > "$ROOT/build.ffhave"
FMISSING=$(comm -23 "$ROOT/build.ffneed" "$ROOT/build.ffhave" || true)
rm -f "$ROOT/build.ffneed" "$ROOT/build.ffhave"
if [ -n "$FMISSING" ]; then
    echo "error: $FFDIR/dnro/ffmpeg.elf bad imports:" >&2
    echo "$FMISSING" >&2
    exit 1
fi
echo "ffmpeg import check passed"
for p in "$ROOT"/plugins/*/*.elf "$ROOT"/menu/default_layout/submenus/*/*.elf "$ROOT"/menu/default_layout/layout/*.elf; do
    [ -f "$p" ] || continue
    "$NM" -D -u "$p" | awk "{print \$NF}" | sort -u > "$ROOT/build.plugneed"
    { "$NM" -D --defined-only "$ROOT/loader/loader.elf" | awk "{print \$NF}"
      cat "$NEKOMOD/neko3d.def" "$ROOT/lib/miniini-module/miniini.def" \
          "$ROOT/lib/json-module/json.def" "$ROOT/lib/xxhash-module/xxhash.def" \
          "$FFDEF"
    } | sort -u > "$ROOT/build.plughave"
    PMISSING=$(comm -23 "$ROOT/build.plugneed" "$ROOT/build.plughave" || true)
    rm -f "$ROOT/build.plugneed" "$ROOT/build.plughave"
    if [ -n "$PMISSING" ]; then
        echo "error: $p bad imports:" >&2
        echo "$PMISSING" >&2
        exit 1
    fi
done
echo "import check passed"

rm -rf "$DEST"
mkdir -p "$DEST"
mkdir -p "$PKG/lib"
rm -rf "$PKG/switch"
rm -rf "$PKG/sdroot"
mkdir -p "$PKG/qlaunch-ext/plugins"
cp "$ROOT/loader/loader.nsp" "$DEST/exefs.nsp"
cp "$NEKOMOD/neko3d.dnro" "$PKG/lib/neko3d.dnro"
cp "$ROOT/extern/SDL-dlink/dist/lib/sdl3.dnro" "$PKG/lib/sdl3.dnro"
cp "$ROOT/lib/miniini-module/miniini.dnro" "$PKG/lib/miniini.dnro"
cp "$ROOT/lib/json-module/json.dnro" "$PKG/lib/json.dnro"
cp "$ROOT/lib/xxhash-module/xxhash.dnro" "$PKG/lib/xxhash.dnro"
cp "$FFDIR/dnro/ffmpeg.dnro" "$PKG/lib/ffmpeg.dnro"
mkdir -p "$PKG/qlaunch-ext/menus"
cp "$ROOT/menu/menu.dnro" "$PKG/qlaunch-ext/menus/menu.dnro"
mkdir -p "$PKG/qlaunch-ext/NekoMenu/default/submenus"
cp "$ROOT/menu/default_layout/theme.ini" "$PKG/qlaunch-ext/NekoMenu/theme.ini"
cp "$ROOT/menu/default_layout/layout/layout.dnro" "$PKG/qlaunch-ext/NekoMenu/default/layout.dnro"
cp "$ROOT/menu/default_layout/assets/TabOrder.json" "$PKG/qlaunch-ext/NekoMenu/default/TabOrder.json"
cp "$ROOT/menu/src/assets/themes.json" "$PKG/qlaunch-ext/NekoMenu/default/themes.json"
mkdir -p "$PKG/qlaunch-ext/NekoMenu/default/assets"
cp "$ROOT/menu/src/assets/sfx.json" "$PKG/qlaunch-ext/NekoMenu/default/assets/sfx.json"
rm -f "$PKG/qlaunch-ext/NekoMenu/default/sfx.json"
mkdir -p "$PKG/qlaunch-ext/NekoMenu/default/assets/hud"
cp "$ROOT"/menu/assets/hud/*.png "$PKG/qlaunch-ext/NekoMenu/default/assets/hud/"
mkdir -p "$PKG/qlaunch-ext/NekoMenu/default/assets/sfx"
for wav in s2_cursor_move s2_button_click s2_back_select s2_album s2_settings s2_home_menu s2_eshop s2_game_chat s2_standby news; do
    cp "$ROOT/menu/assets/sfx/$wav.wav" "$PKG/qlaunch-ext/NekoMenu/default/assets/sfx/"
done
cp "$ROOT/menu/assets/font.otf" "$ROOT/menu/assets/switch_icons.ttf" "$PKG/qlaunch-ext/NekoMenu/default/assets/"
for f in "$ROOT"/menu/default_layout/submenus/*/*.dnro; do
    if [ -f "$f" ]; then
        cp "$f" "$PKG/qlaunch-ext/NekoMenu/default/submenus/"
    fi
done
mkdir -p "$PKG/qlaunch-ext/NekoMenu/widgets"
for f in "$ROOT"/plugins/*/*.dnro; do
    if [ -f "$f" ]; then
        cp "$f" "$PKG/qlaunch-ext/plugins/"
    fi
done
mkdir -p "$PKG/qlaunch-ext/icons"
for f in "$ROOT"/plugins/*/icon/*.jpg; do
    if [ -f "$f" ]; then
        cp "$f" "$PKG/qlaunch-ext/icons/"
    fi
done
mkdir -p "$PKG/qlaunch-ext/bin"
cp "$ROOT/loader/nroloader/nroloader.nsp" "$PKG/qlaunch-ext/bin/"
mkdir -p "$PKG/qlaunch-ext/forwarders"
for f in "$ROOT"/plugins/ForwarderPlugin/forwarders/*.ini; do
    if [ -f "$f" ]; then
        cp "$f" "$PKG/qlaunch-ext/forwarders/"
    fi
done
mkdir -p "$PKG/qlaunch-ext/config/plugin"
cp "$ROOT/plugins/ForwarderPlugin/config/forwarder.ini" "$PKG/qlaunch-ext/config/plugin/"
echo "copy dist/lib/* -> sdmc:/lib/"
echo "copy dist/qlaunch-ext/* -> sdmc:/qlaunch-ext/"

echo "built at:"
find "$PKG" -type f | sort
