# Phase 1 Architecture

## Runtime Shape

```text
Vanilla TypeScript UI
        |
        | HTTP + WebSocket on 127.0.0.1
        v
Local C++ Service
  |-- ImportService (libcurl, Chess.com, manual PGN)
  |-- JobManager (single background analysis queue)
  |     `-- Analyzer
  |           |-- AnalysisCache
  |           `-- Stockfish (persistent UCI subprocess)
  |-- Repository (replayed in-memory projections)
  `-- EventLog (checksummed append-only binary records)
```

The C++ process owns chess rules, import validation, engine orchestration,
classification, jobs, caching, and persistence. TypeScript renders server-owned
positions and analysis results; it does not calculate legal moves or evaluations.

## Module Boundaries

| Module | Responsibility |
| --- | --- |
| `pct::chess` | Array board, FEN, SAN, PGN, legal moves, make/unmake, repetition, hashing, perft |
| `pct::import` | Chess.com URL validation, PubAPI/page retrieval, PGN extraction, manual import |
| `pct::engine` | Persistent subprocess lifetime, UCI protocol, timeout, cancellation, restart, Multi-PV |
| `pct::analysis` | Two-pass evaluation, candidate ranking, taxonomy evidence, explanations, phase classification |
| `pct::storage` | Binary event encoding, CRC-32, durable append, replay, corruption reporting, tail recovery |
| `pct::app` | Game projection, deduplication, completed-analysis replay, background job state |
| `pct::service` | Structured JSON API, loopback HTTP, static assets, WebSocket progress |
| `web` | Board, move navigation, import dialog/drop target, status, review lessons, responsive layout |

Public headers live under `include/pct`; implementations live in matching `src`
directories. The CLI and service compose these modules without bypassing their
public interfaces.

## Analysis Flow

1. Import and validate PGN.
2. Reconstruct every ply with the chess core.
3. Persist `GameImported` and `GameParsed` before analysis starts.
4. Return the game to the UI immediately and enqueue analysis.
5. Shallow-analyze every unique normalized position, reporting progress.
6. Calculate evaluation loss from the mover's perspective and select at most five candidates.
7. Deep-analyze candidates with Multi-PV.
8. Rank at most three mistakes and generate evidence-backed deterministic explanations.
9. Append position, mistake, classification, explanation, and completion events.
10. Broadcast job completion; the UI reloads the stored projection.

Completed analyses are served from the repository after restart and do not need
Stockfish. Duplicate active or completed jobs return the existing job.

## Concurrency and Lifetime

- The HTTP accept loop dispatches local client connections independently.
- `JobManager` owns one `std::jthread` and one analysis queue in Phase 1.
- `Stockfish` owns one child process and its input/output pipes using RAII.
- Cancellation uses `std::stop_source` and `std::stop_token`.
- Repository, cache, event log, job state, and WebSocket client lists have explicit mutex ownership.
- Phase 3 may replace the single analysis worker with a priority worker pool without changing chess APIs.

## Storage Format

Every little-endian record contains:

```text
magic:u32 | schema:u16 | type:u16 | record_length:u32
event_id:u64 | timestamp_ms:i64 | payload_length:u32
payload:bytes | crc32:u32
```

The CRC covers the header and payload. Appends use `O_APPEND`, write the complete
record, and call `fsync` before acknowledging success. Replay skips a
checksummed-invalid record when its length is trustworthy, searches for the next
magic marker after malformed bytes, and preserves later valid records. Only an
incomplete trailing record may be truncated automatically.

The repository rebuilds its projection from `GameImported` and
`AnalysisCompleted` events. It atomically regenerates versioned `games.idx`,
`positions.idx`, and `mistakes.idx` files from that projection. These files are
derived and disposable; deleting them cannot destroy source data. Other analysis
events retain the audit trail for later projection expansion.

## Trust Boundaries

- The server binds to `127.0.0.1` only.
- Remote retrieval accepts only HTTPS Chess.com game URLs.
- libcurl follows at most three HTTPS redirects and caps responses at 10 MiB.
- HTTP headers are capped at 64 KiB and bodies at 10 MiB.
- Static paths reject `..` and backslashes.
- The browser receives a restrictive same-origin content security policy.
- No telemetry, cloud account, remote database, paid API, or LLM is used.

## Known Phase 1 Constraints

- Stockfish is an external prerequisite and is not redistributed.
- The official Chess.com PubAPI is archive-based; a URL with player/year/month
  context uses it directly, otherwise the importer attempts public-page PGN
  extraction and exposes manual PGN as the reliable fallback.
- The analysis queue has one worker. Batch history, priorities, and worker pools
  belong to later phases.
- Snapshots, compaction, richer indexes, and schema migration are Phase 2 work.
