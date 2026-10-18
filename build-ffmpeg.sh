#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
FFSRC="$ROOT/extern/FFmpeg-dlink"
PREFIX="$FFSRC/install"
BUILD="$FFSRC/build"

if [ -z "${DEVKITPRO:-}" ]; then
    if [ -d "/opt/devkitpro" ]; then
        DEVKITPRO="/opt/devkitpro"
        export DEVKITPRO
    else
        echo "error: DEVKITPRO is not set" >&2
        exit 1
    fi
fi

if [ ! -f "$FFSRC/configure" ]; then
    echo "error: $FFSRC/configure missing; run: git submodule update --init extern/FFmpeg-dlink" >&2
    exit 1
fi

DKA="$DEVKITPRO/devkitA64"
rm -rf "$BUILD"
mkdir -p "$BUILD"
# Pre-create ffbuild: configure logs to ffbuild/config.log from its first
# check, but only mkdirs it halfway through. A missing log dir makes every
# test redirect fail, so the test binary never runs and features (rint and
# friends) misdetect as missing, breaking the build in libavutil/libm.h.
mkdir -p "$BUILD/ffbuild"
cd "$BUILD"
"$FFSRC/configure" \
    --prefix="$PREFIX" \
    --cc="$DKA/bin/aarch64-none-elf-gcc" \
    --ar="$DKA/bin/aarch64-none-elf-ar" \
    --nm="$DKA/bin/aarch64-none-elf-nm" \
    --ranlib="$DKA/bin/aarch64-none-elf-ranlib" \
    --strip="$DKA/bin/aarch64-none-elf-strip" \
    --extra-cflags="-isystem $DEVKITPRO/libnx/include -D__SWITCH__ -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -O2" \
    --extra-ldflags="-L$DEVKITPRO/libnx/lib -fPIC" \
    --target-os=horizon \
    --enable-cross-compile \
    --cross-prefix=aarch64-none-elf- \
    --arch=aarch64 \
    --cpu=cortex-a57 \
    --enable-neon \
    --enable-pic \
    --enable-gpl \
    --enable-static \
    --disable-shared \
    --disable-doc \
    --disable-programs \
    --disable-debug \
    --disable-autodetect \
    --disable-runtime-cpudetect \
    --disable-all \
    --enable-avformat \
    --enable-avcodec \
    --enable-swscale \
    --enable-swresample \
    --enable-avutil \
    --enable-nvtegra \
    --enable-demuxer=mov \
    --enable-decoder='h264,aac' \
    --enable-parser='h264,aac' \
    --enable-bsf='h264_mp4toannexb,aac_adtstoasc' \
    --enable-hwaccel=h264_nvtegra \
    --disable-encoders \
    --disable-muxers \
    --disable-filters
make -j"$(nproc)"
make install

echo "ffmpeg installed to $PREFIX"
