#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$ROOT/packaging/macos/package.sh"
APP="$ROOT/dist/Personal Chess Tutor.app"
BACKEND="$APP/Contents/Resources/bin/personal-chess-tutor"
TMP=$(mktemp -d)
PORT=18787
PID=""
cleanup() {
  if [ -n "$PID" ]; then kill "$PID" 2>/dev/null || true; fi
  rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

"$BACKEND" --help >/dev/null
"$BACKEND" --data-dir "$TMP/data" --web-root "$APP/Contents/Resources/web" \
  --tactical-corpus "$APP/Contents/Resources/tactical-corpus.json" \
  --stockfish "$ROOT/build-package-arm64/pct-fake-stockfish" --port "$PORT" &
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
for attempt in $(seq 1 100); do
  if curl --fail --silent "http://127.0.0.1:$PORT/api/games/$GAME_ID" >"$TMP/game-before-restart.json" &&
    node -e 'const fs=require("fs"); const game=JSON.parse(fs.readFileSync(process.argv[1],"utf8")); process.exit(game.analysis_status === "complete" ? 0 : 1)' "$TMP/game-before-restart.json"; then
    break
  fi
  sleep 0.1
done
node -e 'const fs=require("fs"); const game=JSON.parse(fs.readFileSync(process.argv[1],"utf8")); if (game.analysis_status !== "complete") throw new Error("sample game analysis did not complete before restart")' "$TMP/game-before-restart.json"

kill "$PID"
wait "$PID" || true
PID=""

"$BACKEND" --data-dir "$TMP/data" --web-root "$APP/Contents/Resources/web" \
  --tactical-corpus "$APP/Contents/Resources/tactical-corpus.json" \
  --stockfish "$ROOT/build-package-arm64/pct-fake-stockfish" --port "$PORT" &
PID=$!
for attempt in $(seq 1 50); do
  if curl --fail --silent "http://127.0.0.1:$PORT/api/diagnostics" >"$TMP/diagnostics.json"; then break; fi
  sleep 0.1
done
grep -q '"engine_workers":2' "$TMP/diagnostics.json"
curl --fail --silent "http://127.0.0.1:$PORT/api/games/$GAME_ID" >"$TMP/game-after-restart.json"
node -e 'const fs=require("fs"); const game=JSON.parse(fs.readFileSync(process.argv[1],"utf8")); if (game.analysis_status !== "complete") throw new Error("persisted sample game analysis was not reopened after restart")' "$TMP/game-after-restart.json"
printf '%s\n' "clean install, sample analysis, and restart smoke passed"
