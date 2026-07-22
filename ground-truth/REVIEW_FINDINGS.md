# Dedicated Review Findings

Date: 2026-07-21

This review covers every uncommitted production, test, and documentation change against
`AGENTS.md`, the C++ boundary, Engineering Rules, and Visual QA. Findings are recorded before the
repair pass.

## Critical

No critical canonical-game corruption, unsafe remote-input handling, or fake timer-driven progress
was found. Variation nodes store independent FENs and canonical imported plies remain untouched.

## High

### H1 — Dormant TypeScript chess-domain implementation remains

`web/src/chess.ts` still contains UCI move application and FEN mutation helpers, and frontend tests
continue to bless them. The current UI no longer calls them, but retaining a tested second move
application path conflicts with the C++ ownership invariant and creates an easy regression route.

Repair: remove the unused move-application helpers and their tests; retain only presentation FEN
parsing, square geometry, and overlay geometry.

### H2 — Retry attempts are not durable

Analysis Retry currently creates a temporary variation, validates the move through C++, compares
the canonical UCI with C++ analysis candidates, then deletes the variation. The user-visible result
is correct, but the attempt is not recorded for future training as required.

Repair: add a typed C++ retry endpoint that validates from `fen_before`, compares against stored
analysis candidates, persists a versioned attempt event, and returns canonical structured data.

### H3 — Variation Explorer has no on-demand engine evaluation

Legal branching, siblings, navigation, structured illegal errors, deletion, and persistence exist,
but the required optional engine evaluation action is missing.

Repair: expose a bounded C++ position-analysis call through the existing Analyzer/cache at lower
priority than explicit review, return structured engine data, and render it only on user request.

### H4 — Same-game stale job events can replace newer state

The WebSocket handler filters by game ID but not by current job ID. A delayed terminal update from
an older cancelled/retried job for the same game can replace a newer active job in UI state.

Repair: track the job collection, select the newest relevant job deterministically, and reject an
older job update when a newer same-game job is already active.

### H5 — Engine line is the default inspector tab

`inspectorTab` starts as `line`, exposing PV notation by default despite beginner-first progressive
disclosure requirements.

Repair: default to Summary, provide an explicit engine-line tab, and add a local preference for
users who intentionally want engine lines first.

## Medium

### M1 — Recent Games does not present player perspective completely

Rows show both players, result, date, time control, opening, and analysis status, but not explicit
opponent, user color, or ratings. When no configured player can be inferred, perspective must be
labeled unavailable rather than guessed.

### M2 — Recent Games does not project all job states per row

Only the active workstation job is available to row rendering. Queued, running, cancelled, and
failed states should come from the real job list for every game.

### M3 — Shell primitives omit the specified command palette/help surface

The shared sidebar, dialog, buttons, inputs, panels, status, empty, and error treatments exist, but
there is no command palette or visible shortcut help. Tooltip behavior relies on native `title`.

### M4 — Dynamic trend bars use inline style attributes

The progress chart emits inline height styles. Replace them with bounded presentation classes so
visual state remains in CSS and sanitizer/browser behavior is consistent.

### M5 — Some old review helpers are dead

`isAcceptedTry` and `acceptedSquareMove` remain exported and tested but are no longer used by the
application. They invite client-side correctness drift.

## Verification gaps

- ASan/UBSan warnings-as-errors suite has not been rerun on the full redesigned worktree.
- Variation has been verified at one canonical opening position, but not yet before the first ply,
  from a middlegame node, and after a classified mistake in one recorded matrix.
- Blunder-selected, engine-hidden/shown, failed Recent Games, reduced-motion, and focus-order
  screenshots/checks need refreshed evidence.
- The implementation report currently describes the broad vertical slice but must be amended with
  this repair pass and must not overclaim deferred persisted settings/coaching work.

## Repair resolution

All High and Medium findings above are fixed in the working tree. Normal native, frontend, and
ASan/UBSan suites pass. Real Stockfish browser evidence covers a selected Blunder, default-hidden
and explicitly shown engine lines, durable Retry, initial and classified-mistake variations,
on-demand evaluation, exact return, restart recovery, light/dark/responsive layouts, command
palette focus, and an isolated engine-failure state.

Reduced-motion CSS is present and audited, but the active browser did not advertise reduced motion;
that one browser state remains static evidence rather than emulated runtime evidence. Persisted
editable engine settings and coaching styles remain deliberately deferred pending C++ contracts.
