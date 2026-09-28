#!/bin/bash
# Builds App/ with SwiftPM and wraps the executable into out/Cider.app (ad-hoc signed for local use).
# Usage: scripts/build-app.sh [debug|release]   (default: release)
set -euo pipefail

CONFIG="${1:-release}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/out"
APP="$OUT/Cider.app"
VERSION="0.0.1"

ICON="$ROOT/App/Resources/AppIcon.icns"
[ -f "$ICON" ] || { swift "$ROOT/scripts/make-icon.swift" "$ROOT"
                    iconutil -c icns "$OUT/Cider.iconset" -o "$ICON"; }

cd "$ROOT/App"
swift build -c "$CONFIG"
BIN="$(swift build -c "$CONFIG" --show-bin-path)/Cider"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BIN" "$APP/Contents/MacOS/Cider"
cp "$ICON" "$APP/Contents/Resources/AppIcon.icns"
# Bundled compatibility data (docs/plan/06); the signed data channel will update it later.
rm -rf "$APP/Contents/Resources/data" && cp -R "$ROOT/data" "$APP/Contents/Resources/data"

cat > "$APP/Contents/Info.plist" <<PLIST
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

codesign --force --sign - --timestamp=none "$APP"
echo "built $APP"
