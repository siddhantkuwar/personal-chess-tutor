#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD="$ROOT/build-package-arm64"
DIST="$ROOT/dist"
FINAL_APP="$DIST/Personal Chess Tutor.app"
FINAL_ZIP="$DIST/Personal-Chess-Tutor-macOS.zip"
mkdir -p "$DIST"
STAGING=$(mktemp -d "$DIST/.pct-package.XXXXXX")
APP="$STAGING/Personal Chess Tutor.app"
CONTENTS="$APP/Contents"
RESOURCES="$CONTENTS/Resources"

npm ci --prefix "$ROOT/web"
npm run build --prefix "$ROOT/web"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DPCT_WARNINGS_AS_ERRORS=ON
cmake --build "$BUILD"
ctest --test-dir "$BUILD" --output-on-failure

mkdir -p "$CONTENTS/MacOS" "$RESOURCES/bin" "$RESOURCES/web"
cp "$ROOT/packaging/macos/Info.plist" "$CONTENTS/Info.plist"
cp "$ROOT/packaging/macos/launcher" "$CONTENTS/MacOS/launcher"
cp "$ROOT/packaging/macos/pct-data" "$RESOURCES/bin/pct-data"
cp "$BUILD/personal-chess-tutor" "$BUILD/pct-cli" "$RESOURCES/bin/"
cp -R "$ROOT/web/dist/." "$RESOURCES/web/"
cp "$ROOT/resources/catalog.json" "$ROOT/resources/openings.json" \
   "$ROOT/resources/tactical-corpus.json" "$RESOURCES/"
chmod +x "$CONTENTS/MacOS/launcher" "$RESOURCES/bin/pct-data"

if command -v codesign >/dev/null 2>&1; then
  codesign --force --deep --sign - "$APP"
  codesign --verify --deep --strict "$APP"
fi

STAMP=$(date +%Y%m%d%H%M%S)
if [ -e "$FINAL_APP" ]; then mv "$FINAL_APP" "$DIST/Personal Chess Tutor.previous-$STAMP.app"; fi
mv "$APP" "$FINAL_APP"
rmdir "$STAGING"
if [ -e "$FINAL_ZIP" ]; then mv "$FINAL_ZIP" "$FINAL_ZIP.previous-$STAMP"; fi
if command -v ditto >/dev/null 2>&1; then
  ditto -c -k --sequesterRsrc --keepParent "$FINAL_APP" "$FINAL_ZIP"
else
  cmake -E tar cf "$FINAL_ZIP" --format=zip "$FINAL_APP"
fi
printf '%s\n' "$FINAL_ZIP"
