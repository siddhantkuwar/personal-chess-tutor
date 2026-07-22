# Ground-Truth Implementation Plan

Date: 2026-07-21

## Validated baseline

The repository is a C++20/CMake application with a Vite 6 TypeScript presentation layer. Despite
the package language referring to React, `web/package.json` contains no React dependency and
`web/src/main.ts` renders string templates directly. This plan preserves that real stack.

The pre-redesign Analysis flow was validated against the real service and
`/usr/local/bin/stockfish` using an isolated data directory: Chess.com URL import, canonical game
display, move navigation, analysis launch/progress, completed classifications, best-move reveal,
and engine PV display worked. The exact baseline evidence and commands are recorded in
`ground-truth/AUDIT_REPORT.md`.

Actual commands:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
cmake -S . -B build-sanitizers -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPCT_ENABLE_SANITIZERS=ON -DPCT_WARNINGS_AS_ERRORS=ON
cmake --build build-sanitizers
cmake -E env ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitizers --output-on-failure
cd web && npm run check && npm test && npm run build
node scripts/check-release-links.js
```

There is no separate frontend lint script. C++ warnings-as-errors plus TypeScript checking are the
available static gates.

## Boundaries to preserve

C++ remains authoritative for import/sync, canonical PGN, SAN/FEN, legal moves, Stockfish,
scheduling, progress, cancellation, cache/storage, classification, opening recognition, accuracy,
patterns, variations, and player aggregation. Relevant ownership lives under `include/pct/` and
`src/`.

TypeScript remains responsible for layout, navigation, board input intent, rendering, themes,
keyboard/focus behavior, and transient disclosure state in `web/src/`. It may compare a
C++-validated canonical UCI move with C++-supplied candidates, but may not generate or validate
legal moves.

## Milestone 1 — Documentation migration

Files:

- `.gitignore`
- `README.md`
- `RESTRUCTURE_PLAN.md`
- `restructure-plan.html`
- `ground-truth/MIGRATION_REPORT.md`
- ignored `archive/2026-07-pre-ground-truth/`

Contract changes: none.

Rollback: restore each archived file to the original path listed in `MIGRATION_REPORT.md`, then
revert only the link edits and `/archive/` ignore rule.

Verification: Markdown inventory, `git status --short --untracked-files=all`, ignored-path check,
and `node scripts/check-release-links.js`.

At risk: public historical links and accidentally archiving active build/security documentation.

## Milestone 2 — Design foundation and application shell

Files:

- `web/src/design/tokens.css`
- `web/src/styles.css`
- `web/src/main.ts`

Contract changes: none. Theme and disclosure preferences are presentation-local until a persisted
settings API is introduced.

Rollback: remove the shell wrapper and token aliases while retaining existing mode/state handlers;
restore the prior CSS cascade as one independently reviewable frontend diff.

Verification: `npm run check`, `npm test`, `npm run build`; real-app sidebar and focus traversal;
system/light/dark at 1440×900, 1280×800, and 1180×720; reduced-motion emulation; empty/error/loading
states; no horizontal overflow.

At risk: duplicated old/new navigation, theme contrast, board compression, and overly broad CSS.

## Milestone 3 — Recent Games and explicit analysis launch

Files:

- `include/pct/app/ingest_manager.hpp`
- `src/app/ingest_manager.cpp`
- `src/service/http_server.cpp`
- `web/src/types.ts`
- `web/src/api.ts`
- `web/src/main.ts`
- `web/src/styles.css`
- `tests/ingest_manager_tests.cpp`
- `tests/chesscom_api_tests.cpp`
- `tests/phase2_e2e_tests.cpp`

Contract changes: import persists and returns a canonical game identity without implicitly creating
an analysis job. Existing `POST /api/games/{id}/analysis`, job snapshots, WebSocket updates, and
cancel remain the explicit runtime path. Batch analysis uses the existing bounded scheduler.

Rollback: restore import-triggered job creation on both API and tests, then remove only the Recent
Games launch/list/selection view. The canonical repository data is unaffected.

Verification: empty/populated/selected states, real Chess.com or fixture import, exact C++ request
path, single/batch Analyze, cancel, failed/offline display, light/dark screenshots, native API and
ingest tests.

At risk: code that assumed every import response contains `job`, stale job events after selecting a
different game, and incomplete player metadata.

## Milestone 4 — Analysis workstation vertical slice

Files:

- `include/pct/analysis/analyzer.hpp`
- `src/analysis/analyzer.cpp`
- `src/app/repository.cpp`
- `web/src/types.ts`
- `web/src/api.ts`
- `web/src/main.ts`
- `web/src/styles.css`
- `web/src/design/tokens.css`
- `web/src/review.ts`
- `tests/analysis_tests.cpp`
- `tests/repository_tests.cpp`
- `web/tests/review-state.test.ts`
- `web/tests/insights.test.ts`

Contract changes: versioned overall/White/Black accuracy, sample size, and scoring-version fields
are added to analysis JSON. Existing C++ progress enums/counts remain authoritative.

Rollback: remove accuracy fields with backward-compatible JSON defaults and restore the prior
workstation markup without changing stored canonical games or completed move assessments.

Verification: unanalysed/analyzing/completed/failed/blunder states; best, ordinary, and blunder
moves; keyboard navigation; engine hidden/shown; best-move reveal and arrow; accuracy serialization;
frontend/native suites and both-theme screenshot matrix.

At risk: engine detail disclosed by default, stale selected-ply context, inaccessible graph/board
state, and progress copy exceeding the actual C++ stage contract.

## Milestone 5 — Variation Explorer

Files:

- `include/pct/app/repository.hpp`
- `include/pct/storage/event_log.hpp`
- `src/app/repository.cpp`
- `src/storage/event_log.cpp`
- `src/service/http_server.cpp`
- `web/src/types.ts`
- `web/src/api.ts`
- `web/src/main.ts`
- `web/src/styles.css`
- `tests/repository_tests.cpp`
- `tests/api_tests.cpp`

Contract changes:

- `POST|GET /api/games/{game}/variations`
- `GET|DELETE /api/games/{game}/variations/{variation}`
- `POST /api/games/{game}/variations/{variation}/moves`
- `POST /api/games/{game}/variations/{variation}/cursor`
- `POST /api/games/{game}/variations/{variation}/reset`
- Structured error code `illegal_move`

An additional typed on-demand variation-analysis endpoint may be added only by delegating to the
existing C++ engine/cache at interactive priority; it must never be computed in TypeScript.

Rollback: stop exposing the routes, retain or ignore the append-only variation event records, and
remove frontend entry actions. Canonical imported games remain immutable throughout.

Verification: branch before the first ply, from a middlegame ply, and after a classified mistake;
two siblings; backward/forward; illegal move; exact canonical return; restart/compaction recovery;
optional evaluation when implemented; frontend/native tests and screenshots.

At risk: snapshot replay ordering, numeric JSON IDs, accidental canonical mutation, stale branch
cursor, and engine work competing with explicit analysis.

## Milestone 6 — Secondary screens and real settings

Files:

- `web/src/main.ts`
- `web/src/styles.css`
- `web/src/design/tokens.css`
- `web/src/insights.ts`
- `web/tests/insights.test.ts`
- future typed settings changes, if accepted, in `include/pct/app/`, `src/app/`,
  `src/service/http_server.cpp`, `web/src/types.ts`, and `web/src/api.ts`

Contract changes: none for real-data Explore/Progress. Editable engine/review/coaching settings
require a persisted C++ settings contract; unsupported controls must remain visibly unavailable.

Rollback: restore secondary screens independently while keeping the shell and Analysis vertical
slice. Settings migrations must be versioned and retain safe defaults.

Verification: real-data and empty states, supporting-position links, sample-size/freshness copy,
theme overrides, actual runtime facts, and no invented charts.

At risk: presenting frontend-derived inference as domain truth, generic dashboard drift, and settings
that appear editable without affecting C++.

## Milestone 7 — Review and repair

Files: every uncommitted production/test/documentation path from `git status`, with findings recorded
in `ground-truth/REVIEW_FINDINGS.md` and final evidence in `ground-truth/IMPLEMENTATION_REPORT.md`.

Rollback: each repair remains a focused diff tied to one finding; no bulk reset or destructive
cleanup is permitted.

Verification: full normal suite, ASan/UBSan warnings-as-errors build and tests, frontend checks,
documentation links, real Stockfish smoke, browser matrix, accessibility/focus, console logs,
restart persistence, and `git diff --check`.

At risk: tests that verify only JSON shape, flaky loopback-port tests, stale screenshots, dead
client-side chess helpers, and documentation overclaiming completion.

## Dependencies

No dependency additions are justified. The existing C++ core, local HTTP/WebSocket transport,
Vite/TypeScript stack, and in-app Playwright-equivalent browser tooling cover the work. Adding React,
a state library, a chess library, or a UI kit would increase migration risk without solving an
uncovered requirement.

## Decisions and defaults

Questions that would ordinarily require product-owner input, with conservative defaults used so
work can continue:

1. Keep the real vanilla TypeScript stack or migrate to React? Default: keep the existing stack.
2. Whose perspective defines opponent/color in unlinked PGNs? Default: use the configured/local
   player profile when known; otherwise show both players and label perspective unavailable.
3. Persist all settings now? Default: persist only controls backed by a clean C++ contract; label
   the rest unavailable.
4. Variation engine mode? Default: off with an explicit on-demand Analyze position action using
   interactive priority and compatible cache keys.
5. Coaching style before `ICoachingProvider` exists? Default: do not fake style transformations in
   TypeScript; expose the future contract as unavailable.

Each milestone is independently reviewable and must remain buildable before advancing.
