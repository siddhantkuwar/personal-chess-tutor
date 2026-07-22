# Ground-Truth Implementation Report

Date: 2026-07-21

## Outcome

The requested staged vertical slice and dedicated repair pass are complete in the working tree:

Recent Games → explicit Analyze → observable C++ progress → completed board-first review →
classification/accuracy/opening evidence → durable Retry → persistent Variation Explorer → exact
return to canonical review.

No commit or push was performed.

## Architecture and contracts

- The existing Vite/TypeScript DOM stack remains; no dependency or framework migration was added.
- C++ owns PGN/SAN/FEN, legality, Stockfish, classification, accuracy, scheduling, persistence,
  retry validation, and variation evaluation. TypeScript owns presentation and input intent only.
- Import persists canonical games without implicitly launching analysis.
- Accuracy is versioned as `tutor-expected-points-squared-v1` and explicitly labeled as independent
  from Chess.com Accuracy.
- Durable review attempts use append-only `ReviewAttempted` events and survive snapshot/compaction
  replay.
- Variation trees remain separate from canonical games, survive restart, and can request an
  explicit lower-priority Stockfish evaluation through the existing analyzer/cache.

Added or extended API routes:

- `GET|POST /api/games/{game}/variations`
- `GET|DELETE /api/games/{game}/variations/{variation}`
- `POST /api/games/{game}/variations/{variation}/moves`
- `POST /api/games/{game}/variations/{variation}/cursor`
- `POST /api/games/{game}/variations/{variation}/reset`
- `POST /api/games/{game}/variations/{variation}/analysis`
- `GET /api/games/{game}/retry-attempts`
- `POST /api/games/{game}/moves/{ply}/retry`

## Findings fixed

All High findings H1–H5 and Medium findings M1–M5 in `REVIEW_FINDINGS.md` were repaired:

- removed TypeScript FEN/UCI move application and dead review correctness helpers;
- persisted C++-validated retry attempts;
- added on-demand C++ variation evaluation;
- selected newest per-game jobs so stale WebSocket events cannot replace newer work;
- made Summary the default inspector and added an opt-in local engine-line preference;
- made Recent Games player perspective explicit or honestly unavailable, with real per-row states;
- added a keyboard-accessible command palette and shortcut help;
- replaced inline trend heights with bounded CSS classes;
- added a prominent, retryable failed-engine state.

## Verification commands and results

```sh
cmake --build /tmp/plywise-groundtruth-build -j 4
ctest --test-dir /tmp/plywise-groundtruth-build --output-on-failure
cmake -S . -B /tmp/plywise-groundtruth-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DPCT_ENABLE_SANITIZERS=ON -DPCT_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/plywise-groundtruth-asan -j 4
ctest --test-dir /tmp/plywise-groundtruth-asan --output-on-failure
cd web && npm run check && npm test && npm run build
node scripts/check-release-links.js
git diff --check
```

Results:

- normal C++ build: pass;
- CTest: 3/3 targets, 166/166 core tests pass;
- ASan/UBSan warnings-as-errors build and all 3 CTest targets: pass;
- TypeScript check: pass;
- frontend geometry, review-state, and insights tests: pass;
- Vite production build: pass;
- release-link audit: 25 files checked, pass;
- `git diff --check`: pass.

The first sandboxed CTest attempt could not bind its temporary loopback WebSocket port. The exact
same suite passed fully with loopback binding allowed; this was an environment permission issue,
not a product failure.

## Real-app browser evidence

An isolated service on port 17891 used real `/usr/local/bin/stockfish`. A second isolated service
on port 17892 used the crash fixture to prove the failed-engine state. Browser QA verified:

- empty/populated Recent Games and explicit Analyze;
- truthful running and completed progress;
- real classified Blunder selection;
- engine line hidden by default and shown only through the Line tab;
- accepted Retry persisted through the C++ endpoint;
- Variation entry before the initial ply and before a classified blunder;
- legal branch extension, on-demand Stockfish evaluation, exact return state, and recovery with
  the `Bc4` breadcrumb after a full service restart;
- settings runtime facts, default-off engine-line preference, disabled coaching styles, light/dark
  themes, command palette focus, 1280 desktop, and 390 mobile without horizontal overflow;
- explicit failed-engine copy, error detail, unavailable footer, and Retry action.

Evidence is local and ignored under `artifacts/playwright/2026-07-21-repair-pass/`. Key files:

- `01-recent-empty-light-1440.png`
- `03-analysis-running-light-1440.png`
- `05-analysis-blunder-light-1440.png`
- `07-retry-persisted.png`
- `08-variation-blunder-on-demand.png`
- `09-variation-initial-before.png`
- `10-variation-recovered-after-restart.png`
- `11-settings-dark-1280.png`
- `12-command-palette-dark-1280.png`
- `13-analysis-mobile-dark-390.png`
- `14-analysis-failed-1280.png`

Reduced-motion overrides are present in both legacy and workstation CSS blocks. The active browser
did not advertise reduced motion, so behavior was verified statically rather than by emulation.

## Deferred work and risks

- Persisted editable engine settings still require a versioned C++ settings contract.
- Coaching styles remain disabled until an `ICoachingProvider` boundary exists; no fake frontend
  coaching transformation was introduced.
- Browser coverage is evidence-backed manual automation, not yet a checked-in regression suite.
- `web/src/main.ts` remains large and should be split by view/state responsibility without moving
  chess-domain logic into TypeScript.
- Recent player perspective is intentionally unavailable unless the same inferred player is
  supported across at least two games; a configured-profile identity contract would be stronger.

## Next milestone

Define and migrate the persisted C++ settings and coaching-provider contracts, then expose only
those controls through typed endpoints. Add the recorded browser matrix as a repeatable local CI
suite while keeping screenshots and user data out of version control.
