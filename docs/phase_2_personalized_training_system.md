# Phase 2: Personalized Training System

## Outcome

Turn the reliable analyzer into a tutor that teaches from the user's own history. The application creates interactive lessons and drills, schedules reviews, identifies recurring weaknesses, recommends relevant resources, and reports transparent progress trends.

This phase combines PRD development phases 10 through 13 and completes the core chess-improvement loop.

## User Experience at the Phase Gate

1. Import one game or a recent batch of Chess.com games.
2. Review a guided lesson for each major mistake.
3. Identify the opponent's threat, choose a response, see the strongest reply, and retry the position.
4. Receive graduated hints only after failed attempts.
5. Open a daily review queue scheduled from prior performance.
6. See recurring weaknesses by category, game phase, and opening.
7. Follow local, curated learning recommendations tied to observed evidence.
8. Track improvement using understandable metrics rather than a synthetic score.

## Ordered Workstreams

### 1. Complete the Mistake and Game-Phase Model

- Expand tactical classification to forks, pins, skewers, discovered attacks, overloaded defenders, removal of defender, and back-rank weaknesses.
- Add high-confidence king-safety, opening-principle, passed-pawn, endgame, and time-management rules.
- Implement combined opening, middlegame, and endgame classification using development, material, queen state, castling, pawn structure, and move number.
- Separate directly proven classifications from suggestive strategic heuristics.
- Store the evidence and classifier version with every result so classifications can be reproduced and migrated.
- Add ECO/opening-name recognition and departure-from-book detection from a local, versioned data source.

### 2. Guided Lessons and Exact-Position Drills

- Build the coaching sequence for every major mistake: changed threat, attacked pieces, candidate move, opponent response, explanation, and retry.
- Implement the hint ladder: no hint, relevant-piece highlight, candidate moves, then solution reveal.
- Generate exact-position drills from analyzed games before adding synthetic or corpus-derived positions.
- Validate every accepted drill move with the chess core and verify solutions with Stockfish.
- Record attempt result, response time, hint usage, retries, and demonstrated solution.
- Make drill sessions resumable after an application restart.

### 3. Spaced-Repetition Scheduler

- Define a documented scheduling algorithm with deterministic inputs and a testable clock.
- Prioritize failed, frequent, high-impact, and due concepts while preventing one weakness from monopolizing the queue.
- Store creation time, source game/position, category, difficulty, attempt history, success rate, last review, and next review.
- Support due, upcoming, new, and mastered drill states.
- Recalculate schedules from immutable drill-attempt events rather than mutating opaque state.

### 4. Persistent Player Profile

- Aggregate mistakes by category, opening, game phase, severity, and time period.
- Track recurrence rate, repeated-mistake interval, drill accuracy, retention, and analysis completion.
- Track opening performance and a lightweight personal repertoire based on positions the user actually reaches.
- Add endgame conversion and king-safety metrics only when their denominators are statistically meaningful.
- Version profile projection logic so historical snapshots can be rebuilt after classifier changes.
- Add periodic profile snapshots to keep startup bounded while preserving event-log replay as the source of truth.

### 5. Historical Import and Background Work

- Import recent Chess.com games in batches and deduplicate by normalized game identity.
- Queue shallow analysis before deep work so an initial profile appears quickly.
- Prioritize recent and unanalyzed games, pause/resume background work, and apply backpressure.
- Persist job state so completed work is retained and incomplete idempotent jobs can resume after restart.
- Show discovered, imported, duplicate, queued, completed, failed, positions analyzed, cache hits, and remaining-work estimates.
- Keep one Stockfish worker in this phase unless measured workloads prove it inadequate; the worker pool belongs to Phase 3.

### 6. Resource Recommendations

- Create a local, editable, versioned catalog for free videos, timestamps, book chapters, exercises, tactical themes, endgame modules, and repertoire lessons.
- Map resources to taxonomy categories, phase, rating band, and prerequisites.
- Rank recommendations using recurrence, severity, drill performance, repertoire relevance, prior completion, and time since study.
- Display the evidence behind each recommendation.
- Persist recommendation and completion events.

### 7. Dashboard and Storage Maturity

- Add drill queue, lesson flow, weakness profile, trends, opening performance, resource list, and batch progress to the frontend.
- Report raw counts, rates, denominators, and time windows; do not create a composite intelligence score.
- Add drill, profile, resource, rating, and snapshot event schemas and indexes.
- Implement explicit schema migrations with backward-compatibility tests.
- Implement atomic compaction that validates the replacement log before swapping it into place.
- Rebuild and validate indexes from the event log after deletion or corruption.

## Required Deliverables

- Full guided lesson and exact-position drill workflow
- Tested spaced-repetition review queue
- Expanded evidence-backed mistake taxonomy
- Game-phase and local opening recognition
- Persistent recurring-weakness profile
- Historical Chess.com batch import and resumable scheduler
- Local curated resource catalog and recommendation engine
- Transparent progress dashboard
- Snapshots, migrations, indexes, and atomic compaction
- Documented formulas for all user-visible metrics

## Verification

- Fixture-based classifier tests cover every supported category with positive and negative examples.
- Drill integration tests prove position legality, accepted solution correctness, hint progression, event recording, and restart recovery.
- Scheduler tests use a fake clock and cover success, failure, hints, overdue reviews, starvation prevention, and deterministic replay.
- Profile tests compare event replay with snapshot-plus-tail replay and verify every dashboard denominator.
- Migration tests load each prior schema version and preserve logical state.
- Compaction fault-injection tests interrupt every swap stage and retain either the old valid log or the new valid log.
- Batch tests cover duplicates, cancellation, retry, application restart, partial analysis, and cache hits across transpositions.
- End-to-end tests cover import, analysis, lesson, drill attempt, next review, profile update, and recommendation.

## Exit Criteria

Phase 2 is complete only when:

- A major mistake produces a correct, restart-safe guided drill.
- Review scheduling changes predictably from attempts and can be reconstructed from the event log.
- Multiple games produce recurring-weakness and trend views whose values are explainable from source events.
- Batch import can be paused, resumed, restarted, and deduplicated without losing completed work.
- Recommendations cite the weakness evidence that caused them to appear.
- Migration, compaction, recovery, and end-to-end suites pass on representative user data.

## Deferred to Phase 3

- Multiple concurrent Stockfish workers
- Corpus-derived and transformed-position drills
- Bitboard implementation
- Deep performance and memory optimization
- Final benchmark, profiling, packaging, and portfolio artifacts

