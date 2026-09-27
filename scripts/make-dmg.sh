#!/bin/bash
# Packs out/Cider.app into out/Cider-<version>.dmg (drag-to-Applications layout).
# Signing and notarization need a Developer ID (docs/plan/00 ADR-007); until then the app is ad-hoc signed and
# Gatekeeper asks the user to confirm the first launch (right-click → Open).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/out/Cider.app"
[ -d "$APP" ] || "$ROOT/scripts/build-app.sh"
VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$APP/Contents/Info.plist")
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/应用程序"
DMG="$ROOT/out/Cider-$VERSION.dmg"
rm -f "$DMG"
hdiutil create -volname "Cider $VERSION" -srcfolder "$STAGE" -fs APFS -format ULFO -quiet "$DMG"
shasum -a 256 "$DMG" | tee "$DMG.sha256"
echo "built $DMG"
