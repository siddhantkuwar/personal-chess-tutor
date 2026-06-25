#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/build-profile"
TRACE="${TMPDIR:-/tmp}/pct-benchmark.trace"
XML="${TMPDIR:-/tmp}/pct-time-profile.xml"

cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build "$BUILD" --target pct-benchmarks
rm -rf "$TRACE"
xcrun xctrace record --template "Time Profiler" --output "$TRACE" \
  --launch -- "$BUILD/pct-benchmarks"
xcrun xctrace export --input "$TRACE" \
  --xpath '/trace-toc/run[@number="1"]/data/table[@schema="time-profile"]' \
  --output "$XML"
node "$ROOT/scripts/xctrace-flamegraph.js" "$XML" "$ROOT/release/profiles/flamegraph.svg"
printf '%s\n' "$ROOT/release/profiles/flamegraph.svg"
