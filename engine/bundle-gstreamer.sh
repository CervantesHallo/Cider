#!/bin/bash
# Bundles the GStreamer runtime Wine's media stack needs into an engine (docs/plan/00 ADR-012: users must not
# have to install GStreamer.framework themselves):
#   <engine>/frameworks/gstreamer/{lib, lib/gstreamer-1.0, libexec/gstreamer-1.0}
# mirrors the framework layout, so the libraries' own @loader_path/@executable_path rpaths keep working.
# Only x86_64 slices are kept (Engine R runs under Rosetta); thinned files are re-signed ad hoc.
#
#   engine/bundle-gstreamer.sh out/engines/<id> [/Library/Frameworks/GStreamer.framework/Versions/1.0]
set -euo pipefail
ENGINE=$(cd "$1" && pwd)
SRC=${2:-/Library/Frameworks/GStreamer.framework/Versions/1.0}
DEST=$ENGINE/frameworks/gstreamer
[ -f "$SRC/lib/libgstreamer-1.0.0.dylib" ] || { echo "no GStreamer runtime at $SRC" >&2; exit 1; }

# Elements winegstreamer creates itself: decodebin, videoconvert, audioconvert, audioresample (base), videoflip
# (good: videofilter), deinterlace, capsfilter (core). Plus decoders, demuxers and converters for games'
# cut-scenes and launchers: MPEG-1/2 and WMV (KiriKiri and other visual novels), H.264/HEVC/VP8/9/AV1 for
# modern titles, and the usual audio codecs.
PLUGINS="coreelements typefindfunctions playback app videoconvertscale audioconvert audioresample volume
         libav mpegpsdemux mpegtsdemux asf isomp4 matroska ogg vorbis opus theora flac wavparse audioparsers
         videoparsersbad mpg123 vpx dav1d applemedia id3demux pbtypes rawparse deinterlace videorate videofilter"

rm -rf "$DEST"
mkdir -p "$DEST/lib/gstreamer-1.0" "$DEST/libexec/gstreamer-1.0"
queue=()

copy_thin() { # <src> <dest>
    if lipo -archs "$1" 2>/dev/null | grep -qw arm64 && lipo -archs "$1" | grep -qw x86_64; then
        lipo -thin x86_64 "$1" -output "$2"
    else
        cp "$1" "$2"
    fi
    chmod u+w "$2"
    codesign -f -s - "$2" 2>/dev/null || true
    queue+=("$2")
}

for p in $PLUGINS; do copy_thin "$SRC/lib/gstreamer-1.0/libgst$p.dylib" "$DEST/lib/gstreamer-1.0/libgst$p.dylib"; done
copy_thin "$SRC/libexec/gstreamer-1.0/gst-plugin-scanner" "$DEST/libexec/gstreamer-1.0/gst-plugin-scanner"
queue+=("$ENGINE/engine/lib/wine/x86_64-unix/winegstreamer.so")

# Dependency closure over @rpath references.
i=0
while [ $i -lt ${#queue[@]} ]; do
    for dep in $(otool -L "${queue[$i]}" | awk 'NR > 1 && $1 ~ /^@rpath\// {print substr($1, 8)}'); do
        [ -f "$DEST/lib/$dep" ] && continue
        [ -f "$SRC/lib/$dep" ] || continue           # ntdll.so and other Wine-internal references
        copy_thin "$SRC/lib/$dep" "$DEST/lib/$dep"
    done
    i=$((i + 1))
done

# winegstreamer.so: bundled libraries first, the system framework only as a fallback.
so=$ENGINE/engine/lib/wine/x86_64-unix/winegstreamer.so
install_name_tool -delete_rpath /Library/Frameworks/GStreamer.framework/Versions/1.0/lib "$so" 2>/dev/null || true
install_name_tool -delete_rpath @loader_path/../../../../frameworks/gstreamer/lib "$so" 2>/dev/null || true
install_name_tool -add_rpath @loader_path/../../../../frameworks/gstreamer/lib "$so"
install_name_tool -add_rpath /Library/Frameworks/GStreamer.framework/Versions/1.0/lib "$so"
codesign -f -s - "$so" 2>/dev/null || true

echo "bundled $(ls "$DEST/lib/gstreamer-1.0" | wc -l | tr -d ' ') plugins, $(ls "$DEST/lib" | grep -c dylib) libraries: $(du -sh "$DEST" | cut -f1)"
