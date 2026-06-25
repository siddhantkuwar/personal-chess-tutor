#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DATA="${PCT_DEMO_DATA:-$ROOT/demo/data}"
STOCKFISH="${PCT_STOCKFISH:-stockfish}"
PORT="${PCT_PORT:-8787}"

npm run build --prefix "$ROOT/web"
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "$ROOT/build"
mkdir -p "$DATA"
"$ROOT/build/personal-chess-tutor" --data-dir "$DATA" --web-root "$ROOT/web/dist" \
  --tactical-corpus "$ROOT/resources/tactical-corpus.json" --stockfish "$STOCKFISH" \
  --port "$PORT" &
PID=$!
trap 'kill "$PID" 2>/dev/null || true' EXIT INT TERM
for attempt in $(seq 1 50); do
  if curl --fail --silent "http://127.0.0.1:$PORT/api/health" >/dev/null; then break; fi
  sleep 0.1
done
BODY=$(node -e 'const fs=require("fs"); process.stdout.write(JSON.stringify({pgn:fs.readFileSync(process.argv[1],"utf8")}))' "$ROOT/demo/sample-game.pgn")
curl --fail --silent -X POST -H 'Content-Type: application/json' \
  --data "$BODY" "http://127.0.0.1:$PORT/api/import"
printf '\nOpen http://127.0.0.1:%s. Press Ctrl-C to stop.\n' "$PORT"
wait "$PID"
