#!/bin/bash
# Builds the x86_64 libraries Wine's Unix side needs (docs/plan/02-engine-r-wine.md), cross-compiled with
# the native arm64 clang (`-arch x86_64`), never taken from an x86 Homebrew:
#   x86_64/   freetype, gnutls (+ static nettle/gmp, bundled libtasn1/libunistring), SDL2
#   gstreamer/ headers + pkg-config files unpacked from GStreamer's official devel package (not installed)
# Needs engine/toolchain.sh first (pkgconf). Steps are stamped like toolchain.sh.
set -euo pipefail

ROOT=${CIDER_TOOLCHAIN:-$HOME/Library/Caches/Cider/toolchain}
SRC=$ROOT/src
BUILD=$ROOT/build/x86_64
PREFIX=$ROOT/x86_64
GST=$ROOT/gstreamer
JOBS=$(sysctl -n hw.ncpu)
mkdir -p "$SRC" "$BUILD" "$PREFIX" "$ROOT/stamps"

export MACOSX_DEPLOYMENT_TARGET=14.0
# SDK: build against an SDK matching the deployment target. A newer SDK makes configure see functions the
# running macOS may not have (Xcode 27's macOS 27 SDK declares pipe2; weak-linked, it is NULL on macOS 26
# and wineboot crashed at pc=0). Override with CIDER_SDK.
if [ -z "${CIDER_SDK:-}" ]; then
    CIDER_SDK=$(ls -d /Library/Developer/CommandLineTools/SDKs/MacOSX15.*.sdk 2>/dev/null | sort -V | tail -1)
fi
[ -d "${CIDER_SDK:-}" ] || { echo "need a macOS 15 SDK (Command Line Tools) or CIDER_SDK=<path>" >&2; exit 1; }
export SDKROOT=$CIDER_SDK
export PATH="$ROOT/host/bin:/usr/bin:/bin:/usr/sbin:/sbin"
export CC="clang -arch x86_64" CXX="clang++ -arch x86_64"
export CFLAGS="-O2" CXXFLAGS="-O2" CPPFLAGS="-I$PREFIX/include" LDFLAGS="-L$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"
HOSTARGS=(--host=x86_64-apple-darwin --build=aarch64-apple-darwin --prefix="$PREFIX")

log() { printf '\n==> %s\n' "$*"; }
fetch() {
    local file="$SRC/$(basename "$1")"
    [ -s "$file" ] || curl -fsSL --retry 3 -o "$file" "$1"
    echo "$2  $file" | shasum -a 256 -c --quiet - || { echo "checksum mismatch: $file" >&2; rm -f "$file"; exit 1; }
}
unpack() {
    local name; name=$(basename "$1"); name=${name%.tar.*}
    rm -rf "${BUILD:?}/$name"; tar -xf "$SRC/$(basename "$1")" -C "$BUILD"; echo "$BUILD/$name"
}
done_() { [ -e "$ROOT/stamps/$1" ]; }
mark() { touch "$ROOT/stamps/$1"; }

GMP=https://ftpmirror.gnu.org/gmp/gmp-6.3.0.tar.xz;                                   GMP_SHA=a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898
NETTLE=https://ftpmirror.gnu.org/nettle/nettle-3.10.2.tar.gz;                         NETTLE_SHA=fe9ff51cb1f2abb5e65a6b8c10a92da0ab5ab6eaf26e7fc2b675c45f1fb519b5
GNUTLS=https://www.gnupg.org/ftp/gcrypt/gnutls/v3.8/gnutls-3.8.10.tar.xz;              GNUTLS_SHA=db7fab7cce791e7727ebbef2334301c821d79a550ec55c9ef096b610b03eb6b7
FREETYPE=https://download.savannah.gnu.org/releases/freetype/freetype-2.14.1.tar.xz;  FREETYPE_SHA=32427e8c471ac095853212a37aef816c60b42052d4d9e48230bab3bdf2936ccc
SDL2=https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz; SDL2_SHA=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
GSTDEV=https://gstreamer.freedesktop.org/data/pkg/osx/1.28.7/gstreamer-1.0-devel-1.28.7-universal.pkg
GSTDEV_SHA=72a44870cf02472cbf6e9a84bcc25ee6807dd1c26659a112066373544d365e7a

build() { # <stamp> <url> <sha> [configure args...]
    local stamp=$1 url=$2 sha=$3; shift 3
    done_ "x86_64-$stamp" && return
    log "x86_64: $stamp"
    fetch "$url" "$sha"
    local dir; dir=$(unpack "$url")
    (cd "$dir" && ./configure "${HOSTARGS[@]}" "$@" >configure.log 2>&1 &&
        make -j"$JOBS" >make.log 2>&1 && make install >install.log 2>&1) || { echo "failed: see $dir/*.log" >&2; exit 1; }
    mark "x86_64-$stamp"
}

# gnutls links these statically, so the engine ships one gnutls dylib instead of five.
build gmp "$GMP" "$GMP_SHA" --disable-shared --enable-static --with-pic
build nettle "$NETTLE" "$NETTLE_SHA" --disable-shared --enable-static --disable-documentation --disable-openssl \
    --enable-mini-gmp=no --with-include-path="$PREFIX/include" --with-lib-path="$PREFIX/lib"
build gnutls "$GNUTLS" "$GNUTLS_SHA" --enable-shared --disable-static --with-included-libtasn1 --with-included-unistring \
    --without-p11-kit --without-idn --without-zstd --without-brotli --without-tpm --without-tpm2 --without-leancrypto \
    --disable-doc --disable-tests --disable-tools --disable-cxx --disable-nls --disable-guile --disable-full-test-suite
build freetype "$FREETYPE" "$FREETYPE_SHA" --enable-shared --disable-static --with-zlib=yes --without-png \
    --without-bzip2 --without-brotli --without-harfbuzz
build sdl2 "$SDL2" "$SDL2_SHA" --enable-shared --disable-static --disable-video-x11 --disable-video-opengles \
    --disable-video-vulkan --disable-render-metal

# GStreamer headers and .pc files for winegstreamer (the runtime is the user's framework for now, ADR-012 later).
if ! done_ gstreamer-devel; then
    log "gstreamer devel headers"
    pkg="$SRC/$(basename "$GSTDEV")"
    [ -s "$pkg" ] || curl -fsSL --retry 3 -o "$pkg" "$GSTDEV"
    echo "$GSTDEV_SHA  $pkg" | shasum -a 256 -c --quiet -
    rm -rf "$BUILD/gstdev" "$GST"; pkgutil --expand-full "$pkg" "$BUILD/gstdev"
    # The devel package is a set of component packages; merge all their payloads.
    mkdir -p "$GST"
    find "$BUILD/gstdev" -type d -name Payload -maxdepth 2 | while read -r payload; do cp -R "$payload"/. "$GST"/; done
    rm -rf "$BUILD/gstdev"
    mark gstreamer-devel
fi

# MoltenVK: the official release (universal), same 1.4.0 the working Gcenx engine ships.
MOLTENVK=https://github.com/KhronosGroup/MoltenVK/releases/download/v1.4.0/MoltenVK-macos.tar
MOLTENVK_SHA=f4feaf6a4988352de8e6d49874ccc1cd6a45021e3cb476e8531bef7ecc73e93a
if ! done_ moltenvk-1.4.0; then
    log "MoltenVK 1.4.0"
    file="$SRC/MoltenVK-macos-1.4.0.tar"
    [ -s "$file" ] || curl -fsSL --retry 3 -o "$file" "$MOLTENVK"
    echo "$MOLTENVK_SHA  $file" | shasum -a 256 -c --quiet -
    tar -xf "$file" -C "$BUILD" MoltenVK/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib
    cp "$BUILD/MoltenVK/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib" "$PREFIX/lib/"
    mark moltenvk-1.4.0
fi

# Wine's configure records each library's install name as the name it dlopens at run time, so every
# library the engine ships must be @rpath-relative before Wine is configured (the engine adds rpaths to
# its own frameworks/ directory). Re-sign ad hoc: changing a load command invalidates the signature.
for lib in "$PREFIX"/lib/*.dylib; do
    [ -L "$lib" ] && continue
    name=$(otool -D "$lib" | tail -1); base=$(basename "$name")
    if [ "$name" != "@rpath/$base" ]; then
        install_name_tool -id "@rpath/$base" "$lib" 2>/dev/null
        codesign -f -s - "$lib" 2>/dev/null
    fi
done

log "x86_64 deps ready: $PREFIX"
ls "$PREFIX/lib" | grep -E '\.dylib$'
