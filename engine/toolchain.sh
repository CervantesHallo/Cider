#!/bin/bash
# Builds the toolchain used to build Cider's Wine engines (docs/plan/02-engine-r-wine.md):
#   host/   arm64 host tools: bison, pkgconf, plus gmp/mpfr/mpc for GCC
#   mingw/  mingw-w64 GCC cross compilers for i686 and x86_64 PE (llvm-mingw breaks Steam's login)
# Every input is pinned by sha256. Steps are stamped, so rerunning only redoes what is missing.
#
#   engine/toolchain.sh            # build everything
#   CIDER_TOOLCHAIN=/path ...      # build elsewhere (default ~/Library/Caches/Cider/toolchain)
set -euo pipefail

ROOT=${CIDER_TOOLCHAIN:-$HOME/Library/Caches/Cider/toolchain}
SRC=$ROOT/src
BUILD=$ROOT/build
HOST=$ROOT/host
MINGW=$ROOT/mingw
JOBS=$(sysctl -n hw.ncpu)
mkdir -p "$SRC" "$BUILD" "$HOST" "$MINGW" "$ROOT/stamps"

export PATH="$HOST/bin:/usr/bin:/bin:/usr/sbin:/sbin"
unset CC CXX CFLAGS CXXFLAGS LDFLAGS CPPFLAGS PKG_CONFIG_PATH
export MAKEINFO=true   # Apple's makeinfo is too old for the GNU manuals; docs are not needed

log() { printf '\n==> %s\n' "$*"; }

# fetch <url> <sha256>
fetch() {
    local file="$SRC/$(basename "$1")"
    [ -s "$file" ] || curl -fsSL --retry 3 -o "$file.part" "$1" && { [ -s "$file" ] || mv "$file.part" "$file"; }
    echo "$2  $file" | shasum -a 256 -c --quiet - || { echo "checksum mismatch: $file" >&2; rm -f "$file"; exit 1; }
}

# unpack <archive> → prints the extracted directory (fresh each time)
unpack() {
    local name
    name=$(basename "$1"); name=${name%.tar.*}
    rm -rf "${BUILD:?}/$name"
    tar -xf "$SRC/$(basename "$1")" -C "$BUILD"
    echo "$BUILD/$name"
}

done_() { [ -e "$ROOT/stamps/$1" ]; }
mark() { touch "$ROOT/stamps/$1"; }

# --- inputs -----------------------------------------------------------------------------------------
BISON=https://ftpmirror.gnu.org/bison/bison-3.8.2.tar.xz;                         BISON_SHA=9bba0214ccf7f1079c5d59210045227bcf619519840ebfa80cd3849cff5a5bf2
PKGCONF=https://distfiles.ariadne.space/pkgconf/pkgconf-2.5.1.tar.xz;              PKGCONF_SHA=cd05c9589b9f86ecf044c10a2269822bc9eb001eced2582cfffd658b0a50c243
GMP=https://ftpmirror.gnu.org/gmp/gmp-6.3.0.tar.xz;                                GMP_SHA=a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898
MPFR=https://ftpmirror.gnu.org/mpfr/mpfr-4.2.2.tar.xz;                             MPFR_SHA=b67ba0383ef7e8a8563734e2e889ef5ec3c3b898a01d00fa0a6869ad81c6ce01
GETTEXT=https://ftpmirror.gnu.org/gettext/gettext-0.26.tar.xz;                     GETTEXT_SHA=d1fb86e260cfe7da6031f94d2e44c0da55903dbae0a2fa0fae78c91ae1b56f00
MPC=https://ftpmirror.gnu.org/mpc/mpc-1.3.1.tar.gz;                                MPC_SHA=ab642492f5cf882b74aa0cb730cd410a81edcdbec895183ce930e706c1c759b8
BINUTILS=https://ftpmirror.gnu.org/binutils/binutils-2.47.tar.xz;                  BINUTILS_SHA=154ab23b60070e8f27013c22977f1129425d67d1e8acd6e13010e617811e4cff
GCC=https://ftpmirror.gnu.org/gcc/gcc-15.2.0/gcc-15.2.0.tar.xz;                    GCC_SHA=438fd996826b0c82485a29da03a72d71d6e3541a83ec702df4271f6fe025d24e
MINGWW64=https://downloads.sourceforge.net/project/mingw-w64/mingw-w64/mingw-w64-release/mingw-w64-v14.0.0.tar.bz2
MINGWW64_SHA=6eaf921d9eb987d3820b364ea9775bc19b965ec81490b6fdd716526c28e1995c

# --- host tools (arm64) -----------------------------------------------------------------------------
autotools_host() { # <url> <sha> <stamp> [configure args...]
    local url=$1 sha=$2 stamp=$3; shift 3
    done_ "$stamp" && return
    log "host: $stamp"
    fetch "$url" "$sha"
    local dir; dir=$(unpack "$url")
    (cd "$dir" && ./configure --prefix="$HOST" "$@" >configure.log && make -j"$JOBS" >make.log && make install >install.log)
    mark "$stamp"
}

autotools_host "$BISON" "$BISON_SHA" bison --disable-nls
autotools_host "$PKGCONF" "$PKGCONF_SHA" pkgconf --with-pkg-config-dir="$HOST/lib/pkgconfig"
[ -e "$HOST/bin/pkg-config" ] || ln -s pkgconf "$HOST/bin/pkg-config"
# msgfmt: Wine compiles its translations (UTF-8 .po files; macOS iconv fails gettext's strict self-test but converts those fine) (po/*.po) into resources; without it the UI stays English.
autotools_host "$GETTEXT" "$GETTEXT_SHA" gettext --disable-shared --disable-java --disable-csharp --disable-openmp \
    --disable-libasprintf --disable-curses --without-emacs --without-git --without-bzip2 --without-xz --disable-acl \
    CFLAGS="-O2 -Wno-incompatible-function-pointer-types" am_cv_func_iconv_works=yes
autotools_host "$GMP" "$GMP_SHA" gmp --disable-shared --enable-static
autotools_host "$MPFR" "$MPFR_SHA" mpfr --disable-shared --enable-static --with-gmp="$HOST"
autotools_host "$MPC" "$MPC_SHA" mpc --disable-shared --enable-static --with-gmp="$HOST" --with-mpfr="$HOST"

# --- mingw-w64 GCC, one prefix per target (like Homebrew's formula) ----------------------------------
fetch "$BINUTILS" "$BINUTILS_SHA"
fetch "$GCC" "$GCC_SHA"
fetch "$MINGWW64" "$MINGWW64_SHA"

build_target() {
    local arch=$1 target=$1-w64-mingw32
    local prefix=$MINGW/toolchain-$arch
    local work=$BUILD/mingw-$arch
    mkdir -p "$prefix" "$work"
    export PATH="$prefix/bin:$PATH"

    if ! done_ "binutils-$arch"; then
        log "$target: binutils"
        rm -rf "$work/binutils"; mkdir -p "$work/binutils"
        tar -xf "$SRC/$(basename "$BINUTILS")" -C "$work/binutils" --strip-components 1
        (cd "$work/binutils" && mkdir -p build && cd build &&
            ../configure --target="$target" --prefix="$prefix" --with-sysroot="$prefix" \
                --enable-targets="$target" --disable-multilib --disable-nls --disable-werror \
                --without-zstd --disable-gdb --disable-gprofng >configure.log &&
            make -j"$JOBS" >make.log && make install >install.log)
        mark "binutils-$arch"
    fi

    if ! done_ "headers-$arch"; then
        log "$target: mingw-w64 headers"
        rm -rf "$work/mingw"; mkdir -p "$work/mingw"
        tar -xf "$SRC/$(basename "$MINGWW64")" -C "$work/mingw" --strip-components 1
        (cd "$work/mingw/mingw-w64-headers" && mkdir -p build && cd build &&
            ../configure --host="$target" --prefix="$prefix/$target" >configure.log && make install >install.log)
        ln -sfn "$target" "$prefix/mingw"
        mark "headers-$arch"
    fi

    if ! done_ "gcc-$arch"; then
        log "$target: gcc (C compiler)"
        rm -rf "$work/gcc"; mkdir -p "$work/gcc"
        tar -xf "$SRC/$(basename "$GCC")" -C "$work/gcc" --strip-components 1
        (cd "$work/gcc" && mkdir -p build && cd build &&
            ../configure --target="$target" --prefix="$prefix" --with-sysroot="$prefix" \
                --enable-languages=c --disable-multilib --disable-nls --enable-threads=win32 \
                --with-gmp="$HOST" --with-mpfr="$HOST" --with-mpc="$HOST" --without-isl --without-zstd --with-system-zlib \
                --with-ld="$prefix/bin/$target-ld" --with-as="$prefix/bin/$target-as" \
                --disable-libssp --disable-libquadmath --disable-libgomp --disable-libatomic \
                --disable-shared --with-pkgversion="Cider toolchain" >configure.log &&
            make -j"$JOBS" all-gcc >make-gcc.log && make install-gcc >install-gcc.log)
        mark "gcc-$arch"
    fi

    if ! done_ "crt-$arch"; then
        log "$target: mingw-w64 CRT"
        local lib; [ "$arch" = i686 ] && lib="--enable-lib32 --disable-lib64" || lib="--disable-lib32 --enable-lib64"
        (cd "$work/mingw/mingw-w64-crt" && rm -rf build && mkdir build && cd build &&
            ../configure --host="$target" --prefix="$prefix/$target" --with-sysroot="$prefix/$target" $lib \
                CC="$target-gcc" >configure.log &&
            make -j"$JOBS" >make.log && make install >install.log)
        mark "crt-$arch"
    fi

    if ! done_ "libgcc-$arch"; then
        log "$target: libgcc"
        (cd "$work/gcc/build" && make -j"$JOBS" all-target-libgcc >make-libgcc.log && make install-target-libgcc >install-libgcc.log)
        mark "libgcc-$arch"
    fi
}

for arch in i686 x86_64; do (build_target "$arch"); done

for arch in i686 x86_64; do
    "$MINGW/toolchain-$arch/bin/$arch-w64-mingw32-gcc" --version | head -1
done
log "toolchain ready: $ROOT"
echo "PATH=$HOST/bin:$MINGW/toolchain-i686/bin:$MINGW/toolchain-x86_64/bin:\$PATH"
