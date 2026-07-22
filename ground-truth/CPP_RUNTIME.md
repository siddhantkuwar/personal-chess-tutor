# C++ Runtime

## Role

The C++ runtime is the technical center of the project. React exists to expose it cleanly.

## Subsystems

### Game ingestion

Accept URL, obtain public game data, parse and normalize PGN, validate variant, deduplicate, and persist a canonical record.

### Chess core

- Position representation
- Legal move generation
- Make/unmake move
- Check and mate detection
- SAN/UCI conversion
- FEN parse/serialize
- PGN reconstruction
- Attack maps
- Draw-state support
- Variation validation

Correctness precedes bitboards, SIMD, and other résumé perfume. Use perft and fixtures first.

### Stockfish UCI manager

- Spawn and supervise processes
- Perform handshake and apply options
- Send positions and search commands
- Parse `info` and `bestmove`
- Handle centipawn and mate scores
- Support MultiPV
- Cancel safely
- Restart unhealthy workers
- Persist engine version and settings

### Scheduler

Support single and batch analysis, bounded concurrency, priorities, retries, cancellation, and graceful shutdown.

Suggested states:

```text
queued
preparing
reconstructing
evaluating
classifying
detecting_patterns
persisting
completed
cancelled
failed
```

### Cache

Include position, side to move, engine identity/version, search limit, MultiPV, and relevant options in the key.

### Persistence

The append-only binary format should define record version/type, payload length, checksum, identifiers, replay, truncated-tail recovery, snapshots or compaction, and migration.

### Progress events

Emit real work units such as reconstructed positions, evaluated positions, classified moves, detectors completed, and records persisted.

```cpp
struct AnalysisProgress {
    AnalysisId analysis_id;
    AnalysisStage stage;
    std::uint32_t completed_units;
    std::uint32_t total_units;
    std::string message;
};
```

## Concurrency

- Bound workers.
- Avoid oversubscribing Stockfish threads across processes.
- Use cooperative cancellation.
- Use RAII for processes, pipes, files, locks, and mappings.
- Avoid shared mutable state where possible.
- Measure queue time separately from execution time.

## Determinism

Store engine, search, classifier, and detector versions with the review.

## Tests

PGN fixtures, SAN/FEN round trips, perft, legal/illegal variations, UCI parser fixtures, process lifecycle, cancellation, queue order, cache compatibility, classification/pattern fixtures, storage recovery, and import-to-review smoke tests.

## Benchmarks

PGN parsing, reconstruction, move generation, cache lookup, storage append/replay, scheduler overhead, and review assembly.
