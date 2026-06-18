# Phase 1: Trustworthy Game Analyzer

## Outcome

Deliver the first complete vertical slice: a user can import one standard Chess.com game, inspect every reconstructed position, watch Stockfish analysis arrive progressively, and review the three most consequential mistakes. Results survive an application restart.

This phase establishes correctness before personalization or optimization. It combines PRD development phases 1 through 9.

## User Experience at the Phase Gate

1. Start the local application on macOS.
2. Paste a Chess.com game URL or paste PGN manually.
3. See the board and move list as soon as parsing completes.
4. Navigate the game while analysis continues in the background.
5. See shallow-scan and deep-analysis progress.
6. Review the top three mistakes with beginner-friendly explanations, better moves, and optional engine details.
7. Close and reopen the application without losing the imported game or completed analysis.

## Ordered Workstreams

### 1. Repository and Build Foundation

- Establish `CMake` targets for the C++ application, libraries, tests, and benchmarks.
- Define source boundaries for chess core, import, engine integration, analysis, storage, HTTP service, and frontend assets.
- Add structured logging, typed errors, configuration, formatting, linting, and CI.
- Add a small CLI harness for parsing positions, running perft, importing PGN, and querying Stockfish without the UI.
- Record the supported compiler, macOS version, Stockfish installation path, and build commands.

### 2. Correct Array-Based Chess Core

- Implement the 64-square board, pieces, moves, make/unmake, attack maps, check detection, and legal-move filtering.
- Implement castling, en passant, promotion, repetition state, material accounting, and Zobrist hashing.
- Implement FEN parse/serialize and SAN parse/generate.
- Validate legal move generation against known perft positions before any analysis code depends on it.
- Keep the board API representation-neutral so a bitboard implementation can be added in Phase 3.

### 3. PGN and Single-Game Import

- Parse standard-chess PGN headers, movetext, comments, results, and common annotations.
- Reconstruct and retain the position at every ply.
- Fetch PGN through the Chess.com public API, with game-page extraction as a fallback.
- Support manual PGN paste as the reliable fallback path.
- Reject malformed PGN, illegal moves, and unsupported variants with actionable errors.
- Normalize game identity and prevent duplicate imports.

### 4. Persistent Stockfish Integration

- Manage one long-lived Stockfish subprocess using RAII.
- Implement UCI startup, readiness checks, command serialization, response parsing, and clean shutdown.
- Support analysis by depth or move time, Multi-PV, cancellation, timeout, and position reuse.
- Detect a stalled or crashed process, restart it, and retry only safe idempotent work.
- Isolate engine protocol types from product-level analysis types.

### 5. Progressive Analysis and Coaching Data

- Run a shallow pass over every position and emit incremental progress.
- Capture evaluations, principal variations, material changes, checks, captures, phase transitions, and tactical features.
- Select a bounded set of suspicious positions for deeper analysis.
- Rank the top three mistakes using evaluation loss, forced outcomes, material impact, and practical severity.
- Implement the first high-confidence mistake categories: hanging piece, hanging queen, missed free capture, failed recapture, ignored attack, missed check, missed mate, and failed mate-threat response.
- Generate deterministic beginner explanation templates grounded in board facts and Stockfish lines.
- Mark uncertainty instead of presenting weak heuristic classifications as facts.
- Cache engine results using the normalized position and engine settings.

### 6. Append-Only Storage Foundation

- Define a versioned binary event envelope with magic number, event type, length, ID, timestamp, payload, and checksum.
- Persist `GameImported`, `GameParsed`, `PositionAnalyzed`, `MistakeDetected`, `MistakeClassified`, and `ExplanationCreated` events.
- Replay the event log deterministically to reconstruct application state.
- Recover valid records after a partial trailing write and report checksum corruption without deleting prior valid data.
- Add initial game, position, and mistake indexes plus a position-analysis cache.
- Use a simple full replay initially; add snapshots, compaction, and migrations when data volume requires them in Phase 2.

### 7. Local Service and Vanilla TypeScript UI

- Bind the C++ service to localhost only.
- Expose the minimum HTTP API for import, game retrieval, moves, analysis, mistakes, jobs, and settings.
- Stream job and analysis updates over WebSocket, including reconnect-safe status snapshots.
- Serve the frontend as static files from the local application.
- Build an accessible chessboard, move list, import form, progress display, mistake cards, arrows/highlights, and expandable engine details.
- Keep chess rules, analysis decisions, and persistence logic out of TypeScript.

## Required Deliverables

- Buildable C++ application and vanilla TypeScript frontend
- Array-based chess core with perft evidence
- Chess.com URL and manual PGN import
- Persistent single-process Stockfish adapter
- Two-pass progressive analysis pipeline
- Initial deterministic mistake classifier and explanations
- Replayable checksummed event log
- Localhost HTTP/WebSocket service
- End-to-end single-game analysis UI
- Architecture and API notes for the implemented slice

## Verification

- Unit tests cover FEN, SAN, move encoding, special moves, attack maps, checks, hashing, material accounting, event serialization, and checksums.
- Property tests cover FEN round trips, make/unmake restoration, hash restoration, serialization round trips, and deterministic replay.
- Perft node counts match trusted reference positions, including castling, en passant, and promotion cases.
- Integration tests cover PGN-to-position reconstruction, UCI communication, engine timeout/restart, shallow/deep analysis, cache reuse, log replay, and HTTP/WebSocket behavior.
- A restart test proves that a completed analysis is loaded without rerunning Stockfish.
- A corruption test proves that a truncated final event does not destroy earlier valid history.

## Exit Criteria

Phase 1 is complete only when:

- A real Chess.com standard game can be imported and every move is reconstructed legally.
- The board becomes interactive before deep analysis finishes.
- Analysis progress is visible and reconnect-safe.
- The displayed top three mistakes are traceable to engine output and deterministic board features.
- Engine crashes and interrupted log writes recover without losing valid completed work.
- All correctness and integration suites pass on a clean macOS build.

## Deferred to Later Phases

- Interactive coaching drills and spaced repetition
- Long-term weakness profiles and resource recommendations
- Historical batch import and multi-worker analysis
- Full mistake taxonomy and advanced strategic classification
- Snapshots, compaction, and production migration tooling
- Bitboards and performance-driven optimization

