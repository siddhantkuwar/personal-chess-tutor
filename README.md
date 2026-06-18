# Personal Chess Tutor

A fully local macOS chess tutor that imports Chess.com games, reconstructs every
position with a custom C++ chess core, analyzes mistakes with Stockfish, and
presents the results through a vanilla TypeScript interface.

The implementation follows the [product requirements](docs/personal_chess_tutor_prd.md)
and the [Phase 1 delivery plan](docs/phase_1_trustworthy_game_analyzer.md).
Implemented contracts are documented in the [architecture](docs/architecture.md)
and [local API reference](docs/api.md).

## Prerequisites

- macOS or Linux with a C++20 compiler
- CMake 3.25 or newer
- Node.js 20 or newer
- Stockfish available on `PATH`, or configured with `--stockfish <path>`

On macOS, Stockfish can be installed with `brew install stockfish`.

## Build and Test

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Frontend

```bash
npm install --prefix web
npm run build --prefix web
```

For frontend-only development, run `npm run dev --prefix web`. The production
build is served by the local C++ application.

## Run

```bash
./build/personal-chess-tutor --data-dir ./data --web-root ./web/dist
```

Open [http://127.0.0.1:8787](http://127.0.0.1:8787). The service binds to
loopback only. Application data remains in the configured local data directory.

## Command-Line Tools

```bash
./build/pct-cli fen '<fen>'
./build/pct-cli perft '<fen>' 4
./build/pct-cli pgn path/to/game.pgn
./build/pct-cli analyze path/to/game.pgn
```

## Repository Layout

- `include/pct/`: public C++ module interfaces
- `src/`: C++ implementations and local service
- `tests/`: unit, property, perft, storage, and integration tests
- `web/`: vanilla TypeScript application
- `design/`: accepted UI concepts and design-system specification
- `docs/`: PRD and chronological implementation phases
