#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BACKEND="$ROOT/build/personal-chess-tutor"
WEB_ROOT="$ROOT/web/dist"
STOCKFISH="${PCT_STOCKFISH:-stockfish}"
PORT="${PCT_REAL_STOCKFISH_SMOKE_PORT:-18788}"
TMP=$(mktemp -d)
PID=""

cleanup() {
  if [ -n "$PID" ]; then kill "$PID" 2>/dev/null || true; fi
  rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

if [ ! -x "$BACKEND" ]; then
  cmake --build "$ROOT/build"
fi
if [ ! -f "$WEB_ROOT/index.html" ]; then
  npm run build --prefix "$ROOT/web"
fi

"$BACKEND" --data-dir "$TMP/data" --web-root "$WEB_ROOT" \
  --tactical-corpus "$ROOT/resources/tactical-corpus.json" \
  --stockfish "$STOCKFISH" --port "$PORT" &
PID=$!

for attempt in $(seq 1 50); do
  if curl --fail --silent "http://127.0.0.1:$PORT/api/health" >"$TMP/health.json"; then break; fi
  sleep 0.1
done
grep -q '"status":"ok"' "$TMP/health.json"

node -e 'const fs=require("fs"); process.stdout.write(JSON.stringify({pgn:fs.readFileSync(process.argv[1],"utf8")}))' \
  "$ROOT/demo/sample-game.pgn" >"$TMP/import-body.json"
curl --fail --silent -X POST -H 'Content-Type: application/json' \
  --data @"$TMP/import-body.json" "http://127.0.0.1:$PORT/api/import" >"$TMP/import.json"
GAME_ID=$(node -e 'const fs=require("fs"); const body=JSON.parse(fs.readFileSync(process.argv[1],"utf8")); if (!body.game_id) process.exit(1); process.stdout.write(body.game_id)' "$TMP/import.json")

for attempt in $(seq 1 120); do
  curl --fail --silent "http://127.0.0.1:$PORT/api/games/$GAME_ID" >"$TMP/game.json"
  if node -e 'const fs=require("fs"); const game=JSON.parse(fs.readFileSync(process.argv[1],"utf8")); process.exit(game.analysis_status === "complete" ? 0 : 1)' "$TMP/game.json"; then
    break
  fi
  sleep 0.25
done

node -e 'const fs=require("fs"); const game=JSON.parse(fs.readFileSync(process.argv[1],"utf8")); if (game.analysis_status !== "complete") throw new Error("real Stockfish analysis did not complete"); if (!game.analysis || !game.analysis.mistakes.length) throw new Error("real Stockfish produced no mistake evidence")' "$TMP/game.json"
curl --fail --silent "http://127.0.0.1:$PORT/api/diagnostics" >"$TMP/diagnostics.json"
grep -q '"engine_completed":' "$TMP/diagnostics.json"

printf '%s\n' "real Stockfish server smoke passed on port $PORT"
