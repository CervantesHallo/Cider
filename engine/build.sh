#!/bin/bash
# Builds a Cider Wine engine from a recipe (docs/plan/02-engine-r-wine.md):
#
#   engine/build.sh engine/recipes/cider-cx26.json          # full build → out/engines/<id>/ + tarball
#   engine/build.sh <recipe> --step configure|make|package  # redo from one step (incremental work)
#
# Needs engine/toolchain.sh and engine/deps.sh. Wine itself is configured and built under `arch -x86_64`
# (the tree's Unix side is x86_64-only); PE modules use the mingw-w64 GCC cross compilers. Debug info is
# kept in this local build so Rosetta processes can be profiled; release packaging strips it.
set -euo pipefail

REPO=$(cd "$(dirname "$0")/.." && pwd)
RECIPE=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); shift
STEP=all
[ "${1:-}" = --step ] && STEP=$2

TC=${CIDER_TOOLCHAIN:-$HOME/Library/Caches/Cider/toolchain}
CACHE=$HOME/Library/Caches/Cider
DOWNLOADS=$CACHE/downloads
JOBS=$(sysctl -n hw.ncpu)
mkdir -p "$DOWNLOADS"

r() { python3 -c "import json,sys; d=json.load(open('$RECIPE')); v=eval(sys.argv[1]); print(v if isinstance(v,str) else '\n'.join(v) if isinstance(v,list) else json.dumps(v))" "$1"; }
ID=$(r "d['id']")
WORK=$CACHE/engine-build/$ID
SRCDIR=$WORK/src
BUILDDIR=$WORK/build
OUT=$REPO/out/engines/$ID
log() { printf '\n==> %s\n' "$*"; }

fetch() { # <url> <sha256|-> → path
    local file="$DOWNLOADS/$(basename "$1")"
    [ -s "$file" ] || curl -fsSL --retry 3 -o "$file.part" "$1" && { [ -s "$file" ] || mv "$file.part" "$file"; }
    [ "$2" = - ] || echo "$2  $file" | shasum -a 256 -c --quiet - >&2 || { echo "checksum mismatch: $file" >&2; exit 1; }
    echo "$file"
}

export MACOSX_DEPLOYMENT_TARGET=14.0
# SDK: build against an SDK matching the deployment target. A newer SDK makes configure see functions the
# running macOS may not have (Xcode 27's macOS 27 SDK declares pipe2; weak-linked, it is NULL on macOS 26
# and wineboot crashed at pc=0). Override with CIDER_SDK.
if [ -z "${CIDER_SDK:-}" ]; then
    CIDER_SDK=$(ls -d /Library/Developer/CommandLineTools/SDKs/MacOSX15.*.sdk 2>/dev/null | sort -V | tail -1)
fi
[ -d "${CIDER_SDK:-}" ] || { echo "need a macOS 15 SDK (Command Line Tools) or CIDER_SDK=<path>" >&2; exit 1; }
export SDKROOT=$CIDER_SDK
export PATH="$TC/host/bin:$TC/mingw/toolchain-i686/bin:$TC/mingw/toolchain-x86_64/bin:/usr/bin:/bin:/usr/sbin:/sbin"
unset CC CXX CFLAGS CXXFLAGS LDFLAGS CPPFLAGS DYLD_LIBRARY_PATH DYLD_FALLBACK_LIBRARY_PATH

# --- source + patches ------------------------------------------------------------------------------
if [ "$STEP" = all ] || [ "$STEP" = source ]; then
    log "$ID: source"
    tarball=$(fetch "$(r "d['source']['url']")" "$(r "d['source']['sha256']")")
    rm -rf "$WORK/unpack" "$SRCDIR"; mkdir -p "$WORK/unpack"
    subdir=$(r "d['source']['subdir']")
    tar -xzf "$tarball" -C "$WORK/unpack" "$subdir"
    mv "$WORK/unpack/$subdir" "$SRCDIR"; rm -rf "$WORK/unpack"
    for p in $(r "d['patches']"); do
        echo "applying $p"
        patch -d "$SRCDIR" -p1 --forward --quiet < "$REPO/engine/patches/$p"
    done
    STEP=all
fi

# --- configure ---------------------------------------------------------------------------------------
# GStreamer's devel payload (engine/deps.sh) has relocatable .pc files (prefix=${pcfiledir}/../..); its
# libraries carry @rpath/lib/… install names, resolved at run time through the framework rpath below.
GSTPC=$TC/gstreamer/lib/pkgconfig
if [ "$STEP" = all ] || [ "$STEP" = configure ]; then
    log "$ID: configure"
    rm -rf "$BUILDDIR"; mkdir -p "$BUILDDIR"
    # winegstreamer links the installed framework's dylibs (the devel payload's glib is static-only):
    # headers from the devel payload, -l names resolved through symlinks to /Library/Frameworks.
    gstlib=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib
    gstlinks=$WORK/gst-links; rm -rf "$gstlinks"; mkdir -p "$gstlinks"
    for l in gstvideo-1.0 gstaudio-1.0 gsttag-1.0 gstbase-1.0 gstreamer-1.0 gobject-2.0 glib-2.0; do
        ln -s "$gstlib/lib$l.0.dylib" "$gstlinks/lib$l.dylib"
    done
    ln -s "$gstlib/libintl.8.dylib" "$gstlinks/libintl.dylib"
    gst_cflags=$(PKG_CONFIG_PATH="$GSTPC" PKG_CONFIG_LIBDIR="$GSTPC" pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0 gstreamer-audio-1.0 gstreamer-tag-1.0)
    gst_libs="-L$gstlinks -lgstvideo-1.0 -lgstaudio-1.0 -lgsttag-1.0 -lgstbase-1.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lintl"
    # rpaths: frameworks/ relative to lib/wine/x86_64-unix/*.so, bin/wine and CiderWineHost.app/Contents/MacOS/wine.
    rpaths="-Wl,-rpath,@loader_path/../../../../frameworks -Wl,-rpath,@loader_path/../../frameworks -Wl,-rpath,@loader_path/../../../frameworks -Wl,-rpath,/Library/Frameworks/GStreamer.framework/Versions/1.0/lib"
    (cd "$BUILDDIR" && arch -x86_64 "$SRCDIR/configure" --prefix="$OUT/engine" $(r "d['configure']") \
        CC="clang -arch x86_64" CXX="clang++ -arch x86_64" CFLAGS="-g -O2" CPPFLAGS="-I$TC/x86_64/include" LDFLAGS="-L$TC/x86_64/lib $rpaths" \
        PKG_CONFIG_PATH="$TC/x86_64/lib/pkgconfig:$GSTPC" PKG_CONFIG_LIBDIR="$TC/x86_64/lib/pkgconfig:$GSTPC" \
        BISON="$TC/host/bin/bison" GSTREAMER_CFLAGS="$gst_cflags" GSTREAMER_LIBS="$gst_libs" \
        i386_CC=i686-w64-mingw32-gcc x86_64_CC=x86_64-w64-mingw32-gcc \
        CROSSCFLAGS="-g -O2 -Wno-incompatible-pointer-types" > configure.log 2>&1) \
        || { tail -30 "$BUILDDIR/configure.log"; exit 1; }
    grep -E "^configure: (WARNING|OpenCL|libgnutls|FreeType|GStreamer|SDL|Vulkan)" "$BUILDDIR/configure.log" || true
    STEP=all
fi

# --- make ----------------------------------------------------------------------------------------------
if [ "$STEP" = all ] || [ "$STEP" = make ]; then
    log "$ID: make -j$JOBS (log: $BUILDDIR/make.log)"
    (cd "$BUILDDIR" && arch -x86_64 make -j"$JOBS" > make.log 2>&1) || { grep -E "error|Error" "$BUILDDIR/make.log" | head -30; exit 1; }
    STEP=all
fi

# --- package: engine layout, frameworks, addons, components, manifest -----------------------------------------------
if [ "$STEP" = all ] || [ "$STEP" = package ]; then
    log "$ID: package → $OUT"
    rm -rf "$OUT"; mkdir -p "$OUT/frameworks"
    (cd "$BUILDDIR" && arch -x86_64 make install-lib > install.log 2>&1) || { tail -20 "$BUILDDIR/install.log"; exit 1; }
    for lib in $(r "d['frameworks']"); do
        cp -L "$TC/x86_64/lib/$lib" "$OUT/frameworks/$lib"
    done
    # Anything still pointing into the toolchain is a packaging bug (it would load from the build machine).
    if otool -L "$OUT"/engine/lib/wine/x86_64-unix/*.so "$OUT/engine/bin/wine" "$OUT/frameworks"/*.dylib | grep -E "$TC|/usr/local|/opt/homebrew"; then
        echo "absolute toolchain paths in the engine" >&2; exit 1
    fi

    addons=$SRCDIR/dlls/appwiz.cpl/addons.c
    mono=$(sed -n 's/^#define MONO_VERSION "\(.*\)"/\1/p' "$addons")
    gecko=$(sed -n 's/^#define GECKO_VERSION "\(.*\)"/\1/p' "$addons")
    mkdir -p "$OUT/engine/share/wine/mono" "$OUT/engine/share/wine/gecko"
    tar -xJf "$(fetch "https://dl.winehq.org/wine/wine-mono/$mono/wine-mono-$mono-x86.tar.xz" -)" -C "$OUT/engine/share/wine/mono"
    for a in x86 x86_64; do
        tar -xJf "$(fetch "https://dl.winehq.org/wine/wine-gecko/$gecko/wine-gecko-$gecko-$a.tar.xz" -)" -C "$OUT/engine/share/wine/gecko"
    done

    n=$(r "len(d.get('components', []))")
    for i in $(seq 0 $((n - 1))); do
        name=$(r "d['components'][$i]['name']")
        file=$(fetch "$(r "d['components'][$i]['url']")" "$(r "d['components'][$i]['sha256']")")
        echo "component $name"
        tar -xzf "$file" -C "$OUT/$(r "d['components'][$i]['into']")" --strip-components "$(r "str(d['components'][$i]['strip'])")"
    done

    if [ "$(r "d.get('gstreamer', '')")" = bundle ]; then
        "$REPO/engine/bundle-gstreamer.sh" "$OUT"
    fi

    version=$("$OUT/engine/bin/wine" --version 2>/dev/null || echo "$(r "d['wine']['version']")")
    python3 - "$RECIPE" "$OUT/manifest.json" "$version" <<'EOF'
import json, sys, hashlib
recipe, out, version = sys.argv[1], sys.argv[2], sys.argv[3]
d = json.load(open(recipe))
json.dump({
    "schemaVersion": 1, "id": d["id"], "flavor": "R", "channel": d.get("channel", "devel"),
    "wine": {"version": version, "tree": d["wine"]["tree"]},
    "cpu_backend": "rosetta-x86_64", "root": "engine",
    "requires": {"macos": "14.0", "rosetta": True},
    "source": {"origin": "local build of engine/recipes/" + recipe.split("/")[-1],
               "sha256": hashlib.sha256(open(recipe, "rb").read()).hexdigest()},
    "libraryPaths": ["frameworks"],
}, open(out, "w"), indent=2, sort_keys=True)
EOF
    du -sh "$OUT"
    log "engine ready: $OUT  (install: ciderctl engine install --tree $OUT)"
fi
