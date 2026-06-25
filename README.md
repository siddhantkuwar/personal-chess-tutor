# Personal Chess Tutor

A private, local-first chess improvement app. It imports Chess.com games or pasted PGN,
reconstructs every position with a C++ chess core, analyzes important moments with a local
Stockfish process, and turns recurring mistakes into lessons, drills, progress metrics, and
evidence-linked study recommendations.

The browser is a view over server-owned facts. Move legality, evaluations, scheduling,
profiles, and persistence all live in the C++ service.

## Current Scope

Phases 1 through 3 are implemented and backed by executable tests, release evidence, profiling,
security checks, and a clean macOS package smoke:

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
- Configurable isolated Stockfish worker pool with interactive/current/history priorities.
- Bounded game and engine queues, retry limits, deduplication, and reserved interactive capacity.
- Optional CC0 tactical corpus with provenance, rating/motif matching, and two-engine validation.
- Array/bitboard parity model and benchmark-backed production representation decision.
- Reproducible benchmarks, Instruments profiling, sanitizer/race workflows, and diagnostics API.
- Reproducible macOS app bundle, local backup/restore utility, and clean-install qualification.

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
             `-- isolated Stockfish child processes over UCI pipes
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

Phase 3 architecture, protocols, measurements, decisions, security evidence, and requirements
traceability are indexed in [`release/index.html`](release/index.html).
Optional full-corpus download and bounded conversion instructions are in
[`release/corpus.html`](release/corpus.html).

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
--workers count    isolated Stockfish and game workers (default: 2, maximum: 16)
--max-pending n    bounded job and engine admission limit (default: 256)
--retry-limit n    retries after an isolated engine failure (default: 1)
--tactical-corpus  optional provenance-tracked local corpus manifest
--no-tactical-corpus  disable supplemental public puzzles
```

Runtime queue and cache counters are available locally at
`http://127.0.0.1:8787/api/diagnostics`.

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
Stockfish recovery, analysis, storage faults, jobs, API contracts, training, the Phase 2
end-to-end workflow, and Phase 3 pool, corpus, representation-parity, and endurance behavior.

Additional qualification commands:

```sh
./build/pct-benchmarks
scripts/security-check.sh
scripts/race-check.sh
scripts/profile-benchmarks.sh
scripts/clean-install-smoke.sh
```

The first four checks have been run successfully on the current arm64 build. The clean-install
script has also been run successfully: it packages the app, launches it on loopback with an
isolated data directory, imports and analyzes the demo PGN, restarts, and reopens the persisted
game.

## macOS Package

```sh
packaging/macos/package.sh
```

This produces `dist/Personal-Chess-Tutor-macOS.zip`. The app includes the backend, CLI,
frontend, catalogs, tactical-corpus manifest, and `pct-data` backup/restore utility. Stockfish
remains an explicit free local dependency: install it with `brew install stockfish` or set
`PCT_STOCKFISH`.

Application data defaults to `~/Library/Application Support/Personal Chess Tutor`. The packaged
`Contents/Resources/bin/pct-data` command prints the location and supports backup, non-destructive
restore, confirmed reset, and uninstall guidance.

## Troubleshooting

- **Stockfish not found:** install it or pass `--stockfish /absolute/path/to/stockfish`.
- **macOS blocks the downloaded app:** the free release is ad-hoc signed rather than notarized;
  right-click the app, choose Open, and confirm the one-time local exception.
- **Frontend build missing:** run `npm ci --prefix web && npm run build --prefix web`.
- **Port already used:** pass `--port` with another loopback port.
- **Analysis appears paused:** `POST /api/jobs/resume` or use the queue control in Train.
- **Storage warning after interruption:** preserve the data directory, then inspect
  `/api/diagnostics`; incomplete trailing records recover automatically at startup.
- **Supplemental puzzles are empty:** they require a recurring weakness and two stable independent
  Stockfish validations. Personal-game drills remain preferred.

## Privacy

The service binds to loopback, stores data locally, and has no account, telemetry, remote
database, paid API, or LLM dependency. Product requirements, phase plans, and the interactive
developer guide live in `docs/`; that directory is intentionally ignored and must not be
committed publicly.
