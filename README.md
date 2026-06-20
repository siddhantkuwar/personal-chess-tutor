# Personal Chess Tutor

A private, local-first chess improvement app. It imports Chess.com games or pasted PGN,
reconstructs every position with a C++ chess core, analyzes important moments with a local
Stockfish process, and turns recurring mistakes into lessons, drills, progress metrics, and
evidence-linked study recommendations.

The browser is a view over server-owned facts. Move legality, evaluations, scheduling,
profiles, and persistence all live in the C++ service.

## Current Scope

Phase 1 and Phase 2 are implemented:

- Legal FEN, SAN, and PGN handling with make/unmake, repetition, hashing, and perft tests.
- Chess.com URL and manual PGN import with bounded input and response sizes.
- Persistent Stockfish UCI process with Multi-PV analysis, timeouts, cancellation, and restart.
- Fast shallow analysis followed by bounded deep analysis of the most important mistakes.
- Evidence-backed mistake categories, opening recognition, and exact-position drills.
- Deterministic spaced repetition, graduated hints, retry tracking, and measured response time.
- Player profile, activity trends, opening performance, weakness recurrence, and raw denominators.
- Versioned local learning catalog with explainable resource recommendations.
- Batch import, shallow-first background scheduling, pause/resume, and restart recovery.
- Append-only checksummed event storage, replay, snapshots, disposable indexes, and compaction.
- Responsive TypeScript interface served by the loopback-only C++ HTTP/WebSocket server.

## Architecture

```text
Browser (TypeScript)
    | HTTP requests + WebSocket progress
    v
C++ local service (127.0.0.1:8787)
    |-- import + chess core       validates and reconstructs games
    |-- job manager + analyzer    schedules shallow/deep work
    |-- training                  drills, profiles, schedules, resources
    |-- repository               current in-memory projections
    `-- event log                durable source of truth
             |
             `-- Stockfish child process over UCI pipes
```

Module ownership:

| Path | Responsibility |
| --- | --- |
| `include/pct/chess`, `src/chess` | Board rules, legal moves, FEN, SAN, and PGN |
| `include/pct/import`, `src/import` | Chess.com and manual PGN ingestion |
| `include/pct/engine`, `src/engine` | Stockfish lifecycle and UCI protocol |
| `include/pct/analysis`, `src/analysis` | Analysis pipeline, classifications, openings, cache |
| `include/pct/training`, `src/training` | Drills, scheduler, profile metrics, recommendations |
| `include/pct/app`, `src/app` | Repository projections and background jobs |
| `include/pct/storage`, `src/storage` | Checksummed event log, recovery, snapshots, compaction |
| `include/pct/service`, `src/service` | Local HTTP, WebSocket, API routing, static files |
| `web` | Browser UI; renders C++-owned state |

Cancellation uses the small shared-atomic abstraction in
`include/pct/common/cancellation.hpp`. This preserves cooperative cancellation without relying
on C++20 stop-token APIs that are unavailable in some Apple libc++ versions used by CI.

## Requirements

- CMake 3.25 or newer
- A C++20 compiler
- Ninja or another CMake generator
- libcurl development files
- Stockfish available on `PATH`, or its path passed with `--stockfish`
- Node.js 20+ and npm for the browser build

## Build And Run

Build the browser assets:

```sh
npm ci --prefix web
npm run build --prefix web
```

Build the C++ application and tests:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Start the local application:

```sh
./build/personal-chess-tutor
```

Then open `http://127.0.0.1:8787`. Runtime state is written under `data/` by default.

Useful server options:

```text
--data-dir path    event log, snapshots, and regenerated indexes
--web-root path    built frontend directory (default: web/dist)
--stockfish path   Stockfish executable (default: stockfish)
--port number      loopback HTTP port (default: 8787)
```

## CLI

```sh
./build/pct-cli fen '<fen>'
./build/pct-cli perft '<fen>' <depth>
./build/pct-cli pgn game.pgn
./build/pct-cli analyze game.pgn [stockfish-path]
```

## Verification

Native CI builds and runs the C++ test executable on macOS. Frontend CI type-checks and builds
the Vite application on Node.js 20. The native suite covers chess rules, PGN/import behavior,
Stockfish recovery, analysis, storage faults, jobs, API contracts, training, and the Phase 2
end-to-end workflow.

## Privacy

The service binds to loopback, stores data locally, and has no account, telemetry, remote
database, paid API, or LLM dependency. Product requirements, phase plans, and the interactive
developer guide live in `docs/`; that directory is intentionally ignored and must not be
committed publicly.
