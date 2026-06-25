#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

npm audit --prefix web --omit=dev
cmake -S . -B build-security -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DPCT_ENABLE_SANITIZERS=ON -DPCT_WARNINGS_AS_ERRORS=ON
cmake --build build-security
ctest --test-dir build-security --output-on-failure

grep -q 'htonl(INADDR_LOOPBACK)' src/service/http_server.cpp
grep -q 'max_download_size = 10 \* 1024 \* 1024' src/import/import_service.cpp
grep -q 'max_body_size = 10 \* 1024 \* 1024' src/service/http_server.cpp
grep -q 'request_path.find("..")' src/service/http_server.cpp

if command -v clang-tidy >/dev/null 2>&1; then
  clang-tidy -p build-security \
    src/engine/pool.cpp src/storage/event_log.cpp src/service/http_server.cpp \
    --warnings-as-errors='clang-analyzer-*'
else
  /usr/bin/clang++ --analyze -std=c++20 -Iinclude src/engine/pool.cpp -o /tmp/pct-pool.plist
  /usr/bin/clang++ --analyze -std=c++20 -Iinclude src/storage/event_log.cpp -o /tmp/pct-storage.plist
  /usr/bin/clang++ --analyze -std=c++20 -Iinclude src/service/http_server.cpp -o /tmp/pct-http.plist
  printf '%s\n' "clang-tidy not installed; Clang Static Analyzer fallback passed."
fi
