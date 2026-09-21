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

make -C "$ROOT/loader" -j"$(nproc)"

rm -rf "$DEST"
mkdir -p "$DEST"
cp "$ROOT/loader/loader.nsp" "$DEST/exefs.nsp"

echo "built at:"
find "$PKG" -type f | sort