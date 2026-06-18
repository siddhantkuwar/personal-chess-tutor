# Phase 3: Scale, Hardening, and Portfolio Release

## Outcome

Harden the complete tutor for a large personal history, optimize only verified bottlenecks, and publish a reproducible portfolio-grade macOS release with evidence of correctness, reliability, and systems-engineering depth.

This phase combines PRD development phases 14 through 16 and closes every remaining success criterion.

## User Experience at the Phase Gate

1. Import a substantial game history without blocking interactive use.
2. Continue a lesson while background workers analyze older games.
3. Pause, resume, or cancel work and recover cleanly from engine or application crashes.
4. Receive additional validated drills from recurring motifs where useful.
5. Launch the packaged local application with documented setup and no paid service dependency.
6. Inspect clear product and engineering documentation explaining how and why the system works.

## Ordered Workstreams

### 1. Establish Performance Baselines

- Create representative small, medium, and large local datasets with sanitized or synthetic games.
- Benchmark FEN/PGN parsing, legal move generation, perft, make/unmake, hashing, event serialization/replay, snapshot load, indexes, cache lookups, and single-worker analysis.
- Profile CPU, memory, allocations, queue latency, lock contention, and cache-hit rates.
- Define explicit latency, throughput, memory, and startup targets from observed user workloads.
- Preserve benchmark inputs and commands so results are reproducible.

### 2. Stockfish Worker Pool and Priority Scheduler

- Add a configurable pool of isolated persistent Stockfish workers.
- Implement producer-consumer queues with high-priority interactive, medium-priority current-game, and low-priority historical jobs.
- Support cancellation, pause/resume, bounded queues, backpressure, retry limits, and job deduplication.
- Route engine crashes to the affected job only and restart the failed worker without stalling the pool.
- Prevent background work from degrading board navigation or interactive lesson latency.
- Measure scaling at one, two, and additional useful worker counts; choose conservative defaults for the user's Mac.

### 3. Advanced Drill Sources

- Integrate an optional public tactical corpus with recorded license, provenance, version, and one-time download instructions.
- Match corpus positions to recurring user motifs and skill level.
- Use structurally similar positions from the user's own history before generating transformations.
- If transformed positions are enabled, require legal-position validation and tactical-equivalence verification with independent engine checks.
- Exclude ambiguous or unstable drills and retain the validation evidence for every generated item.

### 4. Bitboards Behind the Board Interface

- Implement a bitboard board representation without changing external chess-core behavior.
- Run the complete unit, property, and perft suites against both implementations.
- Compare array and bitboard performance using identical benchmark fixtures.
- Adopt bitboards in production paths only where measured gains justify added complexity.
- Retain the array board as the readability reference and correctness oracle where practical.

### 5. Storage and Runtime Optimization

- Add memory-mapped index or event reads only after profiling identifies replay or lookup pressure.
- Use zero-copy record views only with explicit lifetime and bounds guarantees.
- Tune snapshot cadence, compaction thresholds, cache size, and eviction from measured data.
- Bound all queues and caches and expose understandable local diagnostics.
- Test low-disk-space, corrupted-cache, interrupted-snapshot, interrupted-compaction, and stale-index recovery paths.
- Verify that optimization does not weaken checksums, migration guarantees, or deterministic replay.

### 6. Security, Privacy, and Packaging

- Confirm the service binds only to loopback and rejects unintended remote exposure.
- Validate imported URLs, response sizes, PGN sizes, record lengths, WebSocket messages, and filesystem paths.
- Run dependency and static-analysis checks for C++ and TypeScript dependencies.
- Confirm all game history, profiles, drills, and analytics remain local and telemetry-free.
- Package the backend, frontend assets, local catalog, configuration defaults, and Stockfish discovery/setup for macOS.
- Document data location, backup, restore, upgrade, reset, and complete uninstall behavior.
- Perform a clean-machine installation and first-run test.

### 7. Reliability and Release Qualification

- Add fault injection for process termination, worker crash, timeout, truncated records, checksum failures, partial snapshots, and interrupted compaction.
- Run long-duration batch analysis with concurrent interactive drills.
- Check for leaks, data races, deadlocks, unbounded growth, duplicate events, and lost progress.
- Verify application restart during every major job type.
- Maintain a release checklist mapping all PRD success criteria to automated tests or recorded manual evidence.

### 8. Portfolio Documentation

- Write the final architecture specification, storage format, HTTP/WebSocket protocols, and system diagrams.
- Record engineering decisions for array boards versus bitboards, append-only storage, worker scheduling, caching, and deterministic coaching.
- Publish perft results, benchmark tables, profiles, flamegraphs, and optimization conclusions, including changes that were rejected.
- Provide a polished README with build, test, run, architecture, demo, and troubleshooting sections.
- Create a demo dataset, demo script/video, technical write-up, and concise recruiter-facing summary.
- Ensure every performance or reliability claim is reproducible from repository commands and artifacts.

## Required Deliverables

- Configurable prioritized Stockfish worker pool
- Optional, provenance-tracked tactical corpus integration
- Validated advanced drill generation
- Correctness-equivalent bitboard implementation and comparison report
- Measured storage, caching, and runtime optimizations
- Fault-injection and endurance test suites
- Reproducible macOS package and clean-install instructions
- Security/privacy verification checklist
- Architecture, storage, protocol, decision, benchmark, profiling, and demo artifacts
- Final requirements traceability matrix

## Verification

- Both board implementations pass identical unit, property, SAN/FEN, and perft suites.
- Worker-pool tests prove priority ordering, cancellation, bounded backpressure, job deduplication, crash isolation, and restart recovery.
- Load tests combine historical analysis, WebSocket progress, API reads, and interactive drills without violating defined latency targets.
- Sanitizer and race-detection runs report no actionable memory, undefined-behavior, or concurrency defects.
- Storage fault injection preserves all valid acknowledged events under every tested interruption point.
- Advanced drills pass legality and tactical-equivalence gates and reject deliberately ambiguous fixtures.
- A clean macOS environment can build, test, install, launch, analyze a sample game, and reopen persisted results using only documented steps.
- Published benchmark and profiling commands reproduce the checked-in summaries within documented environmental variance.

## Exit Criteria

Phase 3 and the project are complete only when:

- All twelve PRD success criteria are mapped to passing automated tests or explicit release evidence.
- Interactive work remains responsive during a representative historical batch analysis.
- Crashed workers and interrupted writes recover without corrupting valid data or silently losing acknowledged work.
- Optimization choices are backed by before/after measurements and preserve chess correctness.
- The packaged application works locally on the supported macOS target without cloud storage, paid APIs, telemetry, or an LLM dependency.
- A new developer can understand, build, test, run, and evaluate the system from the repository documentation.

## Definition of Final Release

The final release is not defined by implementing every possible heuristic. It is defined by a trustworthy end-to-end tutoring loop, safe local persistence, responsive historical analysis, transparent metrics, and reproducible engineering evidence. Any remaining speculative classifier, recommendation, or optimization should be recorded as future work rather than delaying a reliable release.

