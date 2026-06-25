#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$ROOT" -B "$ROOT/build-tsan" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DPCT_ENABLE_THREAD_SANITIZER=ON -DPCT_WARNINGS_AS_ERRORS=ON
cmake --build "$ROOT/build-tsan"
ctest --test-dir "$ROOT/build-tsan" --output-on-failure
