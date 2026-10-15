#!/usr/bin/env bash
# Build wxWidgets outside the source tree: bashbuild scans every include directory.
set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
deps=${LISAEM_NATIVE_DEPS:-${HOME}/Library/Caches/lisaem-native}
jobs=${LISAEM_BUILD_JOBS:-$(sysctl -n hw.ncpu)}
version=3.2.11
archive="$deps/wxWidgets-$version.tar.bz2"
build="$deps/build-$(uname -m)"
mkdir -p "$deps"
if [[ ! -f "$archive" ]]; then
    curl -fL "https://github.com/wxWidgets/wxWidgets/releases/download/v$version/wxWidgets-$version.tar.bz2" -o "$archive.tmp"
    mv "$archive.tmp" "$archive"
fi
expected=6a129015bce2e914e4bf61ec4411854ad962801d47e92f2eb8340adb6a90af08
actual=$(shasum -a 256 "$archive" | cut -d ' ' -f 1)
[[ "$actual" == "$expected" ]] || { echo "wxWidgets checksum mismatch" >&2; exit 1; }
[[ -d "$deps/wxWidgets-$version" ]] || tar -xjf "$archive" -C "$deps"
mkdir -p "$build"
cd "$build"
if [[ ! -f Makefile ]]; then
    "../wxWidgets-$version/configure" \
        --enable-monolithic --enable-unicode --with-cocoa \
        --with-macosx-version-min=14.5 --disable-richtext --disable-debug \
        --disable-shared --without-expat --with-libtiff=builtin \
        --with-libpng=builtin --with-libjpeg=builtin --with-libxpm=builtin \
        --with-zlib=sys --with-sdl --with-libiconv-prefix=/usr \
        --prefix="$deps/install"
fi
make -j "$jobs"
cd "$repo"
# Use the build-tree wx-config; no system installation or sudo is necessary.
export PATH="$build:$PATH"
export MIN_MACOSX_VERSION=14.5
# Keep $0 relative: bashbuild sources child build.sh files which cd to dirname $0.
exec bash ./build.sh --no-upx --no-strip "$@"
