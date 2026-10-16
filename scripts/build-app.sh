#!/bin/bash
# Builds GUI, command client, and forwarding-only XPC broker with SwiftPM.
# Usage: scripts/build-app.sh [debug|release]   (default: release)
set -euo pipefail

CONFIG="${1:-release}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/out"
APP="$OUT/Cider.app"
VERSION="0.0.1"

mkdir -p "$OUT"
# Serialize publishers. A lock left by SIGKILL requires checking the retained stage before removal.
LOCK="$OUT/.build-app.lock"
mkdir "$LOCK" 2>/dev/null || { echo "another build owns $LOCK (check for an interrupted build)" >&2; exit 1; }
STAGE=""
PUBLICATION_STARTED=0
cleanup() {
    local status=$?
    # Once publication begins, retain the stage even if interrupted: an atomic swap may already
    # have put the previous app there. Never delete either side of an uncertain publication.
    if [ "$PUBLICATION_STARTED" -eq 0 ] && [ -n "$STAGE" ]; then
        rm -rf "$STAGE" || true
    fi
    rmdir "$LOCK" || true
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP

[ ! -L "$APP" ] && { [ ! -e "$APP" ] || [ -d "$APP" ]; } || {
    echo "refusing to replace a symlink or non-directory at $APP" >&2; exit 1;
}
# The stage and publication target share a filesystem. After a swap this directory holds the
# previous app as a recoverable backup, without a second move that could fail after publication.
mkdir -p "$OUT/backups"
STAGE="$(mktemp -d "$OUT/backups/Cider-$(date +%Y%m%d-%H%M%S)-XXXXXX")"
STAGED_APP="$STAGE/Cider.app"

# Darwin's atomic directory exchange keeps the old app present until the complete new one takes
# its place. First publication uses RENAME_EXCL, so an unexpected destination cannot be clobbered.
cat > "$STAGE/publish.swift" <<'SWIFT'
import Darwin
let args = CommandLine.arguments
guard args.count == 4, args[1] == "swap" || args[1] == "install" else { exit(64) }
let flags = args[1] == "swap" ? UInt32(RENAME_SWAP) : UInt32(RENAME_EXCL)
if renamex_np(args[2], args[3], flags) != 0 {
    perror("app publication")
    exit(1)
}
SWIFT
swiftc "$STAGE/publish.swift" -o "$STAGE/publish"

ICON="$ROOT/App/Resources/AppIcon.icns"
[ -f "$ICON" ] || { swift "$ROOT/scripts/make-icon.swift" "$ROOT"
                    iconutil -c icns "$OUT/Cider.iconset" -o "$ICON"; }

cd "$ROOT/App"
swift build -c "$CONFIG"
BIN="$(swift build -c "$CONFIG" --show-bin-path)/Cider"
swift build --package-path "$ROOT/Tools/ciderctl" -c "$CONFIG"
CLI="$(swift build --package-path "$ROOT/Tools/ciderctl" -c "$CONFIG" --show-bin-path)/ciderctl"
swift build --package-path "$ROOT/Tools/CiderBridge" -c "$CONFIG"
BRIDGE="$(swift build --package-path "$ROOT/Tools/CiderBridge" -c "$CONFIG" --show-bin-path)/CiderXPCBridge"

mkdir -p "$STAGED_APP/Contents/MacOS" "$STAGED_APP/Contents/Resources"
cp "$BIN" "$STAGED_APP/Contents/MacOS/Cider"
cp "$CLI" "$STAGED_APP/Contents/MacOS/ciderctl"
cp "$BRIDGE" "$STAGED_APP/Contents/MacOS/CiderXPCBridge"
cp "$ICON" "$STAGED_APP/Contents/Resources/AppIcon.icns"
# Bundled compatibility data (docs/plan/06); the signed data channel will update it later.
cp -R "$ROOT/data" "$STAGED_APP/Contents/Resources/data"

cat > "$STAGED_APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleIdentifier</key><string>org.cider.app</string>
  <key>CFBundleName</key><string>Cider</string>
  <key>CFBundleDisplayName</key><string>Cider</string>
  <key>CFBundleExecutable</key><string>Cider</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleDevelopmentRegion</key><string>zh-Hans</string>
  <key>LSMinimumSystemVersion</key><string>14.0</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSSupportsAutomaticGraphicsSwitching</key><true/>
  <key>NSMicrophoneUsageDescription</key><string>Windows 程序（例如游戏语音）需要使用麦克风。</string>
  <key>NSCameraUsageDescription</key><string>Windows 程序需要使用摄像头。</string>
  <key>NSLocalNetworkUsageDescription</key><string>局域网联机和部分启动器需要访问本地网络。</string>
</dict>
</plist>
PLIST

test -x "$STAGED_APP/Contents/MacOS/Cider"
test -x "$STAGED_APP/Contents/MacOS/ciderctl"
test -x "$STAGED_APP/Contents/MacOS/CiderXPCBridge"
test -s "$STAGED_APP/Contents/Resources/AppIcon.icns"
test -s "$STAGED_APP/Contents/Resources/data/engines/index.json"
plutil -lint "$STAGED_APP/Contents/Info.plist"
codesign --force --sign - --timestamp=none --options runtime "$STAGED_APP/Contents/MacOS/CiderXPCBridge"
codesign --force --sign - --timestamp=none --options runtime "$STAGED_APP/Contents/MacOS/ciderctl"
codesign --force --sign - --timestamp=none --options runtime --entitlements "$ROOT/App/Resources/Cider.entitlements" "$STAGED_APP"
codesign --verify --strict "$STAGED_APP/Contents/MacOS/CiderXPCBridge"
codesign --verify --strict "$STAGED_APP/Contents/MacOS/ciderctl"
codesign --verify --deep --strict "$STAGED_APP"

[ ! -L "$APP" ] && { [ ! -e "$APP" ] || [ -d "$APP" ]; } || {
    echo "publication destination changed to a symlink or non-directory: $APP" >&2; exit 1;
}
MODE=install
if [ -d "$APP" ]; then MODE=swap; fi
PUBLICATION_STARTED=1
if ! "$STAGE/publish" "$MODE" "$STAGED_APP" "$APP"; then
    echo "publication failed; retained stage at $STAGE" >&2
    exit 1
fi
# There are no fallible package/signing checks after the atomic publication. On swap the previous
# app is already at this backup path; on first install the stage only contains the small publisher.
if [ "$MODE" = swap ]; then echo "previous app: $STAGED_APP"; fi
echo "built $APP"
