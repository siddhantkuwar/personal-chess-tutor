# Repository Audit

Date: 2026-07-21

## Stack and commands

- C++20, CMake 3.25+, libcurl, and Ninja. Main service: `personal-chess-tutor`.
- TypeScript with Vite 6 and npm (`web/package-lock.json`); there is no React dependency despite
  the original package assumption. The UI is currently string-template DOM rendering.
- Frontend: `npm run check`, `npm test`, `npm run build` from `web/`.
- Native: `cmake -S . -B <build> -G Ninja`, `cmake --build <build>`, and CTest.

## Frontend topology

- `web/src/main.ts`: application state, modes, rendering, event binding, WebSocket updates.
- `web/src/api.ts`: typed HTTP client.
- `web/src/types.ts`: C++ response contracts.
- `web/src/review.ts` and `web/src/review/state-machine.ts`: review transitions.
- `web/src/chess.ts`: presentation geometry and client-side application of engine PV moves.
- `web/src/styles.css` and `web/src/design/tokens.css`: global visual system.
- Navigation is an in-memory `game | explore | progress` mode, not a URL router.

## C++ topology and transport

- `src/service/http_server.cpp`: loopback HTTP routing, static assets, and WebSocket snapshots.
- `src/import/`: Chess.com and manual PGN ingestion.
- `src/chess/`: board, legal moves, SAN/FEN, PGN, make/unmake.
- `src/engine/`: persistent Stockfish UCI processes and worker pool.
- `src/analysis/`: staged shallow/deep analysis, openings, classification, explanations.
- `src/app/`: repositories, ingest orchestration, job scheduling, persistence projection.
- `src/storage/`: append-only event log and recovery.
- Progress is emitted by C++ as `stage`, `complete`, `total`, and `message`, then delivered through
  job JSON and WebSocket snapshots. TypeScript must not infer additional domain stages.

## Verified baseline flow

Using an isolated data directory, port 8899, the public game
`https://www.chess.com/game/live/171626462440`, and `/usr/local/bin/stockfish`:

1. The link resolved to `superking116 vs. CartaaaaZ`.
2. The canonical game appeared immediately and move navigation worked.
3. Real shallow/deep Stockfish progress reached completion.
4. The completed review exposed per-ply classifications, evaluation, opening data, evidence, best
   move, and engine PV.
5. Selecting `5...d6` showed a Blunder verdict; Reveal showed `exd4`.
6. The current Variation control replayed the engine PV and returned to the canonical move.

Screenshots are in `artifacts/ui-audit/2026-07-21-baseline/` (ignored locally).

## Baseline verification

- Frontend type-check: pass.
- Frontend geometry, review-state, and insight tests: pass.
- Frontend production build: pass.
- Clean C++ build: pass.
- Native suite: 162/163 core tests reached pass; all three CTest targets shared one failure because
  sandboxed loopback port allocation returned port zero in the WebSocket lifecycle fixture.
- The real app successfully bound and ran outside that restriction on an approved isolated port.

## Post-implementation status

Resolved in this worktree:

- Import persists a canonical game without starting analysis; Recent Games exposes explicit single
  and batch Analyze actions.
- Idle, queued, running, complete, cancelled, failed, and offline states are visually distinct and
  backed by C++ job/runtime facts.
- Recent Games is the launch surface. Recent Games, Analysis, Explore, Progress, and Settings share
  one persistent sidebar shell and system/light/dark token foundation.
- Opening and independently defined, versioned C++ accuracy are always visible in completed review.
- Variation is a persisted C++ tree with legality validation, sibling branches, cursor navigation,
  reset, FEN copy, canonical return, and deletion tombstones.
- Retry submits attempts to C++ legality before comparing the canonical move with analyzed engine
  candidates. TypeScript no longer applies engine PV moves or decides legality.
- Explore and Progress use real imported/analyzed data and link evidence back to Review.

Remaining risks and intentionally honest gaps:

- The presentation layer is still a large string-template TypeScript module rather than React;
  splitting it into view/state modules is the next frontend architecture stage.
- Only appearance is currently editable. Engine/review/coaching controls remain labeled unavailable
  until a persisted C++ settings contract exists.
- Retry legality and candidate matching work, but durable retry-attempt history is not yet connected
  to the training projection.
- A future local `ICoachingProvider` boundary is not implemented; current explanations remain
  deterministic C++ data.

## Implementation plan

1. Consolidate system/light/dark tokens, focus styles, reusable primitives, shell, sidebar, and
   truthful empty/error/progress states without replacing the existing stack.
2. Add Recent Games as the launch view with selection and an explicit Analyze action connected to
   existing C++ jobs and progress.
3. Refactor the board-first review workstation around the existing typed analysis contract; expose
   opening, evaluation, classification, and available accuracy evidence without deriving truth.
4. Add a typed C++ variation API for create/extend/navigate/delete/reset and connect board input to
   C++ legality; never use TypeScript as the authoritative move validator.
5. Add real Settings controls only where a persisted/runtime contract exists; label unavailable
   future controls honestly.
6. Improve Explore and Progress only after the vertical slice passes end-to-end verification.
7. Run focused tests and the visual matrix in both themes at 1440x900, 1280x800, and 1180x720.
