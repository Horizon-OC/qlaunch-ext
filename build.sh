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

make -C "$ROOT/neko3d-module" -j"$(nproc)"
make -C "$ROOT/menu" -j"$(nproc)"
make -C "$ROOT/loader" -j"$(nproc)"

rm -rf "$DEST"
mkdir -p "$DEST"
mkdir -p "$PKG/lib"
mkdir -p "$PKG/switch/qlaunch-ext/menus"
cp "$ROOT/loader/loader.nsp" "$DEST/exefs.nsp"
cp "$ROOT/neko3d-module/neko3d.dnro" "$PKG/lib/neko3d.dnro"
cp "$ROOT/menu/menu.dnro" "$PKG/switch/qlaunch-ext/menus/menu.dnro"
echo "copy dist/lib/* -> sdmc:/lib/"
echo "copy dist/switch/* -> sdmc:/switch/"

echo "built at:"
find "$PKG" -type f | sort