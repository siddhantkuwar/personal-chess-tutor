# Personal Chess Tutor

## Product Requirements Document

## 1. Product Definition

A fully local, free macOS chess tutor that imports games from Chess.com URLs, analyzes them with Stockfish, identifies the player’s most consequential mistakes, explains them at a beginner level, converts them into interactive drills, and builds a persistent profile of recurring weaknesses.

The product has two primary goals:

1. Improve the user’s chess through personalized feedback derived from their own games.
2. Serve as a portfolio-grade introduction to C++ systems engineering.

## 2. Core User Flow

```text
Paste Chess.com game URL
        ↓
Retrieve PGN through public API
        ↓
Fallback to game-page extraction
        ↓
Parse PGN and reconstruct every position
        ↓
Display board and move list immediately
        ↓
Run shallow analysis over the full game
        ↓
Detect suspicious positions and evaluation swings
        ↓
Deep-analyze candidate mistakes
        ↓
Select the three most consequential mistakes
        ↓
Generate beginner-friendly explanations
        ↓
Classify opening, middlegame, and endgame weaknesses
        ↓
Create personalized drills
        ↓
Schedule drills with spaced repetition
        ↓
Update long-term weakness and progress metrics
```

Analysis results appear progressively rather than blocking the interface until all engine work finishes.

## 3. Product Scope

The tutor should provide:

- The top three game-losing mistakes
- Beginner-friendly move-by-move explanations
- Opening, middlegame, and endgame feedback
- Recurring weakness detection
- Progress trends over time
- Personalized resource recommendations
- Interactive drills generated from the user’s own games
- Historical game import and batch analysis
- A local personal repertoire tracker
- Transparent chess-improvement metrics

The application is:

- Fully local
- Free to operate
- Designed only for the user’s Mac
- Not intended for public sale or commercial deployment
- Independent of paid APIs
- Independent of LLMs for core functionality

## 4. System Architecture

```text
Vanilla TypeScript frontend
        |
HTTP + WebSocket
        |
C++ local application server
        |
Analysis Coordinator
   ├── Import Service
   ├── Chess Core
   ├── Stockfish Worker Pool
   ├── Mistake Classifier
   ├── Coaching Engine
   ├── Drill Generator
   ├── Resource Recommender
   ├── Spaced-Repetition Scheduler
   └── Append-Only Storage Engine
```

All meaningful chess, scheduling, persistence, orchestration, and analysis logic lives in C++.

The TypeScript frontend handles:

- Board rendering
- Move navigation
- Arrows and highlights
- Drill interaction
- Progress displays
- Live analysis status
- Expandable engine details

## 5. Chess-Core Boundary

The application will implement the following internally:

- 64-square board representation
- FEN parsing and serialization
- Move representation
- Pseudo-legal move generation
- Legal-move filtering
- Attack maps
- Check detection
- Castling
- En passant
- Promotion
- SAN parsing and generation
- Material accounting
- Repetition tracking
- Zobrist hashing
- Tactical feature extraction
- Game-phase classification

Stockfish will handle:

- Positional evaluation
- Principal variations
- Best-move search
- Mate detection
- Tactical verification
- Search depth

The first implementation will use an array-based board representation for correctness and readability.

After perft correctness testing is complete, a bitboard implementation will be added as an optimization milestone and benchmarked against the array-based version behind a common board interface.

## 6. Import System

The user pastes a Chess.com game URL.

Import behavior:

1. Attempt to retrieve the PGN through Chess.com’s public API.
2. If unavailable, fall back to extracting the game from the public page.
3. Allow manual PGN paste as an emergency fallback.
4. Deduplicate imported games.
5. Store source URL, metadata, PGN, and normalized game identity.

The application should also support batch import of recent Chess.com games.

## 7. Progressive Analysis

The UI should become usable as soon as the game is parsed.

Example status flow:

```text
Parsing game           complete
Scanning 63 positions  complete
Deep analysis          2 of 5 candidates
Generating drills      waiting
```

The user can navigate the board and move list while deeper analysis continues.

## 8. Analysis Strategy

The system uses two analysis passes.

### 8.1 Pass One: Shallow Scan

Analyze every position quickly to collect:

- Evaluation before and after each move
- Material changes
- Checks and captures
- Tactical feature changes
- Game-phase transitions
- Candidate blunders
- Missed tactical opportunities
- King-safety changes
- Passed-pawn threats

### 8.2 Pass Two: Targeted Deep Analysis

Deeply analyze only positions where the shallow pass detects:

- Major evaluation loss
- Material loss
- Missed tactical opportunity
- Mate-threat failure
- Suspicious trade
- King-safety deterioration
- Passed-pawn danger
- Endgame conversion failure
- Opening-principle violation with concrete punishment

This reduces unnecessary Stockfish work and creates a legitimate scheduling, caching, and worker-pool problem.

## 9. Move Explanations

Every move receives a compact classification.

Possible classifications:

- Developing move
- Capture
- Check
- Recapture
- Threat
- Neutral move
- Inaccuracy
- Mistake
- Blunder

Important moments receive a short explanation.

Example:

> Black attacked your bishop with a6. Your next move did not move or defend it.

The top three mistakes receive a full lesson containing:

- Position before the mistake
- What changed on the opponent’s previous move
- The played move
- The opponent’s punishment
- One or more better candidate moves
- Beginner-friendly explanation
- Visual arrows and highlights
- Tactical or strategic category
- Retry drill
- Relevant learning resources

Advanced engine details remain hidden by default but can be expanded.

## 10. Coaching Interaction

For each major mistake:

1. Show the position before the move.
2. Ask what the opponent’s previous move changed.
3. Let the user identify attacked or hanging pieces.
4. Ask the user to choose a move.
5. Play the opponent’s strongest response.
6. Explain the mistake.
7. Require the user to retry the position correctly.

Hint sequence:

1. First attempt: no hint
2. Second attempt: highlight relevant pieces
3. Third attempt: show candidate moves
4. Then reveal and explain the solution

## 11. Drill System

Each major mistake supports:

- Replaying the exact position
- Identifying the opponent’s threat
- Identifying hanging or attacked pieces
- Selecting the best move
- Playing the opponent’s strongest response
- Retrying until the concept is demonstrated

Similar-position drills come from:

1. Exact positions from the user’s games
2. Matching motifs from a public tactical corpus
3. Structurally similar mistakes from the user’s own history
4. Carefully validated position transformations

The system must validate transformed positions for legality and tactical equivalence before using them.

## 12. Spaced Repetition

Each drill stores:

- Creation timestamp
- Source game
- Source position
- Mistake category
- Difficulty
- Attempt history
- Success rate
- Last reviewed time
- Next scheduled review
- Hint level used
- Response time

The scheduler should prioritize:

- Frequently repeated mistakes
- Recently failed drills
- High-impact tactical weaknesses
- Previously mastered concepts approaching review time
- Current opening and endgame priorities

## 13. Mistake Taxonomy

The finished product should classify at least:

### Material and Tactical Mistakes

- Hanging piece
- Hanging queen
- Missed free capture
- Failed recapture
- Ignored attack
- One-move tactical loss
- Missed check
- Missed mate
- Failed response to mate threat
- Fork
- Pin
- Skewer
- Discovered attack
- Removal of defender
- Overloaded defender
- Back-rank weakness

### King Safety

- Unsafe king
- Delayed castling
- Weakening pawn moves
- Open king position
- Missed mating threat

### Opening Mistakes

- Premature queen development
- Repeated piece movement
- Delayed development
- Failed center control
- Unnecessary flank-pawn moves
- Opening-theory deviation
- Poor piece coordination
- Uncastled king
- Early structural damage

### Strategic Mistakes

- Bad trade while behind
- Failed simplification while ahead
- Weak pawn structure
- Poor piece activity
- Ignored opponent plan
- Unnecessary pawn push
- Loss of key square
- Bad minor-piece exchange

### Passed-Pawn and Endgame Mistakes

- Ignored passed pawn
- Lost endgame
- Failed conversion
- Opposition error
- Promotion-race error
- Incorrect king activity
- Rook-behind-passed-pawn error
- Poor simplification decision

### Time Management

- Time-management failure
- Excessive early time use
- Instant-move blunder
- Repeated low-time collapse

The classifier combines:

- Deterministic board features
- Material state
- Tactical motifs
- Engine evaluation changes
- Principal variation analysis
- Game phase
- Historical player behavior

## 14. Opening System

The tutor should support:

- ECO code recognition
- Opening-name recognition
- Departure-from-book detection
- Opening-principle feedback
- Performance by opening
- Recurring errors within specific lines
- Personal repertoire tracking
- Opening-position drills
- Recommended variations based on past results

The repertoire system should remain lightweight and evidence-driven.

It should recommend deeper study only when the user repeatedly reaches or mishandles a given position.

## 15. Game-Phase Classification

Opening, middlegame, and endgame classification should use a combined deterministic heuristic based on:

- Move number
- Development state
- Material count
- Queen presence
- Castling status
- Pawn structure
- Remaining minor and major pieces
- Engine phase estimates where useful

The system should not classify phases solely by move number.

## 16. Resource Recommendation System

A local resource catalog maps weaknesses to:

- Free videos
- Specific video timestamps
- Book chapters
- Interactive exercises
- Tactical themes
- Endgame modules
- Repertoire lessons

Recommendations depend on:

- Current estimated skill level
- Recurrence frequency
- Recent drill performance
- Game-phase weakness
- Previous resource completion
- Current opening repertoire
- Time since the concept was last studied

Example:

```text
Recurring weakness: Hanging pieces
Occurred: 8 times in 14 games

Recommended:
- Chess Fundamentals: Undefended Pieces
- Lichess Practice: Hanging Pieces
- Bobby Fischer Teaches Chess: Exercises 1-18
```

The catalog should be local, curated, versioned, and editable.

## 17. Progress Dashboard

The dashboard reports transparent metrics only.

Tracked metrics:

- Chess.com rapid rating
- Games analyzed
- First major blunder move
- One-move material losses per game
- Missed threats per game
- Average centipawn loss
- Tactical-theme frequency
- Opening performance
- King-safety violations
- Endgame conversion rate
- Drill accuracy
- Drill retention
- Repeated-mistake interval
- Weakness trend by week and month
- Resource completion
- Analysis completion rate

The product should not generate a synthetic composite intelligence score.

## 18. Persistence Model

The application uses a custom append-only binary event log.

Core event types:

```text
GameImported
GameParsed
PositionAnalyzed
MistakeDetected
MistakeClassified
ExplanationCreated
DrillCreated
DrillAttempted
ResourceRecommended
ResourceCompleted
RatingObserved
ProfileSnapshotCreated
SchemaMigrated
LogCompacted
```

Example storage layout:

```text
data/
  events.log
  positions.idx
  games.idx
  mistakes.idx
  drills.idx
  snapshots/
  cache/
```

Required storage features:

- Checksummed records
- Versioned schemas
- Crash recovery
- Corruption detection
- Periodic snapshots
- Derived indexes
- Compaction
- Migration support
- Memory-mapped reads
- Position-hash cache
- Zero-copy record access where useful

## 19. Storage Record Design

Each event record should contain at minimum:

- Magic number
- Schema version
- Event type
- Record length
- Event identifier
- Timestamp
- Payload
- Checksum

The event log is immutable during normal operation.

Current state is reconstructed by:

1. Loading the newest valid snapshot
2. Replaying events after the snapshot offset
3. Rebuilding or validating indexes
4. Recovering from partial trailing records
5. Marking corrupted segments without destroying valid prior data

## 20. Compaction and Migration

Compaction should:

- Preserve logical history where required
- Remove superseded derived events
- Rebuild indexes
- Write a new compacted log atomically
- Verify checksums
- Replace the old log only after successful validation

Schema migration should:

- Detect old record versions
- Upgrade them through explicit migration functions
- Preserve backward compatibility where practical
- Include migration tests
- Avoid silent destructive changes

## 21. Stockfish Integration

The first version uses one persistent Stockfish subprocess.

Communication uses UCI over pipes.

The integration layer must support:

- Process startup
- UCI initialization
- Command serialization
- Response parsing
- Timeouts
- Cancellation
- Engine restart
- Crash recovery
- Configurable depth
- Configurable move time
- Multi-PV analysis
- Position reuse
- Analysis prioritization

Later, the system adds a configurable Stockfish worker pool for:

- Batch history analysis
- Parallel candidate evaluation
- Background deep analysis
- Priority queues
- Worker isolation
- Engine-crash recovery

## 22. Analysis Scheduling

The scheduler should support:

- High-priority interactive analysis
- Medium-priority current-game deep analysis
- Low-priority historical batch analysis
- Cancellation
- Pausing
- Resuming
- Progress updates
- Backpressure
- Retry policies
- Job deduplication

Potential job types:

- ParseGameJob
- ShallowAnalyzeGameJob
- DeepAnalyzePositionJob
- ClassifyMistakeJob
- GenerateDrillJob
- RebuildProfileJob
- CompactStorageJob
- RebuildIndexJob

## 23. Caching

The application should cache engine results by normalized position hash.

Cache key inputs may include:

- Zobrist position hash
- Side to move
- Castling rights
- En passant state
- Engine version
- Analysis depth
- Multi-PV count
- Relevant engine settings

The cache should prevent duplicate analysis across transpositions and repeated games.

## 24. Local Web Service

The C++ backend exposes:

- HTTP endpoints for commands and data retrieval
- WebSocket updates for live analysis progress
- Static-file serving for the TypeScript frontend

Example endpoint groups:

```text
/api/games
/api/games/{id}
/api/games/{id}/analysis
/api/games/{id}/moves/{ply}
/api/mistakes
/api/drills
/api/profile
/api/resources
/api/import
/api/jobs
/api/settings
```

The service binds only to localhost by default.

## 25. Frontend

The frontend uses vanilla TypeScript.

Responsibilities:

- Render the chessboard
- Show legal move navigation
- Display arrows and highlighted squares
- Show engine-analysis progress
- Present top mistakes
- Run drills
- Show hint stages
- Display weakness trends
- Show resource recommendations
- Expand advanced engine details
- Navigate game history

No important chess or persistence logic should live in the frontend.

## 26. Systems Concepts Represented

The project should demonstrate:

- RAII
- Ownership and lifetime management
- Smart pointers
- Subprocess management
- UCI inter-process communication
- Producer-consumer queues
- Thread pools
- Task prioritization
- Cancellation
- Background jobs
- Append-only storage
- Memory mapping
- Binary serialization
- Checksums
- Caching
- Zobrist hashing
- Schema migration
- Crash recovery
- Profiling
- Benchmarking
- CMake
- Unit testing
- Integration testing
- Property testing
- Perft correctness tests

## 27. Batch Analysis

The product supports importing historical Chess.com games and analyzing them in the background.

```text
Import recent Chess.com games
        ↓
Deduplicate by game and position hash
        ↓
Queue games for shallow analysis
        ↓
Prioritize recent and unanalyzed games
        ↓
Deep-analyze candidate mistakes
        ↓
Build initial weakness profile
```

The batch-analysis UI should show:

- Total games discovered
- Imported games
- Duplicate games
- Games queued
- Games completed
- Positions analyzed
- Cache hits
- Failed jobs
- Estimated remaining work

## 28. Reliability Requirements

The application should handle:

- Invalid Chess.com URLs
- Missing or malformed PGN
- Unsupported variants
- Illegal moves
- Truncated binary log records
- Checksum failures
- Engine timeouts
- Engine crashes
- Interrupted compaction
- Interrupted snapshots
- Duplicate imports
- Partial analysis
- Frontend disconnection
- Application restart during active jobs

The application should resume safely without losing valid completed work.

## 29. Testing Strategy

### Unit Tests

- FEN parsing
- FEN serialization
- Move encoding
- Attack maps
- Move generation
- Check detection
- Castling
- En passant
- Promotion
- SAN parsing
- SAN generation
- Zobrist hashing
- Material accounting
- Tactical feature detection
- Event serialization
- Checksums
- Snapshot loading
- Migration functions

### Perft Tests

Use known perft positions to validate legal move generation.

Track:

- Node count
- Captures
- En passant moves
- Castles
- Promotions
- Checks
- Checkmates

### Integration Tests

- PGN import to full position reconstruction
- Stockfish subprocess communication
- Full-game shallow scan
- Candidate deep analysis
- Mistake classification
- Drill creation
- Event-log replay
- Crash recovery
- Compaction
- HTTP and WebSocket behavior

### Property Tests

- FEN round-trip integrity
- Move make/unmake integrity
- Hash restoration after unmake
- Serialization round-trip
- Log replay determinism
- Snapshot equivalence
- Cache-key stability

## 30. Performance and Benchmarking

Benchmarks should include:

- FEN parse throughput
- Move generation throughput
- Perft speed
- Board copy cost
- Make/unmake speed
- Zobrist hashing speed
- PGN parse throughput
- Event serialization throughput
- Event replay throughput
- Snapshot load time
- Cache lookup latency
- Single-worker analysis throughput
- Worker-pool scaling
- Array-board versus bitboard performance

Profiling artifacts should include:

- CPU profiles
- Memory profiles
- Flamegraphs
- Allocation counts
- Lock contention
- Queue latency
- Cache-hit rates

## 31. Security and Privacy

The application:

- Stores all game history locally
- Sends no private data to third parties
- Binds the local service to localhost
- Uses no remote account system
- Uses no cloud database
- Uses no paid API
- Does not require telemetry

Remote resources should be limited to:

- Chess.com public game retrieval
- Optional one-time public puzzle-dataset download
- Optional resource-link navigation

## 32. Development Phases

### Phase 1: Project Foundation

- CMake setup
- Repository layout
- Logging
- Error types
- Test framework
- CI
- Formatting and linting
- Basic CLI harness

### Phase 2: Chess Model

- Board representation
- Piece model
- Move model
- Make/unmake
- Attack maps
- Check detection
- Legal move generation
- Special moves
- Perft validation

### Phase 3: Chess Notation and Parsing

- FEN
- SAN
- PGN
- Game metadata
- Position reconstruction
- Import validation

### Phase 4: Stockfish Integration

- Child-process manager
- UCI protocol
- Persistent engine process
- Timeouts
- Cancellation
- Restart behavior
- Engine tests

### Phase 5: Analysis Pipeline

- Shallow scan
- Candidate selection
- Deep analysis
- Progressive results
- Analysis jobs
- Position cache

### Phase 6: Mistake Classification

- Tactical features
- Strategic heuristics
- Game-phase classification
- Top-three mistake ranking
- Beginner explanation templates

### Phase 7: Append-Only Storage

- Binary record format
- Serialization
- Checksums
- Replay
- Snapshots
- Indexes
- Recovery
- Migration
- Compaction

### Phase 8: Local Service

- HTTP server
- WebSocket server
- API contracts
- Static file serving
- Job-progress updates

### Phase 9: TypeScript Frontend

- Interactive board
- Move list
- Analysis status
- Mistake cards
- Expandable engine details
- Progress dashboard

### Phase 10: Coaching and Drills

- Threat-identification drills
- Hanging-piece drills
- Best-move drills
- Hint sequence
- Exact-position replay
- Tactical corpus integration

### Phase 11: Spaced Repetition

- Review scheduling
- Drill history
- Retention metrics
- Review queue
- Weakness-priority weighting

### Phase 12: Resource Recommendations

- Local catalog
- Weakness mappings
- Book chapters
- Video timestamps
- Rating-aware recommendations
- Completion tracking

### Phase 13: Historical Analysis

- Batch Chess.com import
- Deduplication
- Background scheduling
- Profile generation
- Trend analysis

### Phase 14: Worker Pool

- Multiple engine workers
- Priority queue
- Cancellation
- Isolation
- Crash recovery
- Throughput benchmarks

### Phase 15: Bitboards and Optimization

- Bitboard board implementation
- Shared board interface
- Correctness parity
- Benchmark comparison
- Cache and memory optimization

### Phase 16: Portfolio Polish

- Architecture document
- Storage-format specification
- Engineering decision records
- Benchmarks
- Flamegraphs
- Demo video
- README
- Technical write-up
- Recruiter-facing summary

## 33. Portfolio Deliverables

The repository should include:

- Full PRD
- Architecture specification
- Storage-format specification
- Protocol definitions
- Engineering decision records
- System diagrams
- Perft results
- Benchmarks
- Profiling results
- Flamegraphs
- Crash-recovery tests
- Unit and integration tests
- Demonstration database
- Polished README
- Demo video
- Technical write-up
- Recruiter-facing project summary

## 34. Success Criteria

The project succeeds when it can:

1. Import a Chess.com game from a URL.
2. Reconstruct every legal position.
3. Analyze the game progressively with Stockfish.
4. Identify the three most consequential mistakes.
5. Explain those mistakes in beginner-friendly language.
6. Generate interactive drills from the user’s mistakes.
7. Persist all data through an append-only binary log.
8. Recover safely after an interrupted write or engine crash.
9. Track recurring weaknesses and progress over time.
10. Recommend relevant resources based on repeated weaknesses.
11. Analyze historical games in the background.
12. Demonstrate measurable systems-engineering depth through benchmarks, tests, profiling, and documented tradeoffs.

## 35. Non-Goals

The project will not initially attempt to:

- Build a competitive chess engine
- Replace Stockfish search
- Support online multiplayer
- Support public accounts
- Support commercial deployment
- Use cloud storage
- Depend on an LLM
- Provide social features
- Support every chess variant
- Optimize for mobile devices
- Implement a large frontend framework

## 36. Final Product Summary

The final product is a local-first personal chess tutor powered by a custom C++ systems pipeline.

It combines:

- Chess parsing and move generation
- Stockfish subprocess orchestration
- Progressive game analysis
- Personalized mistake classification
- Beginner coaching
- Interactive drills
- Spaced repetition
- Resource recommendations
- Historical trend analysis
- Append-only binary persistence
- Multithreaded background processing
- Performance benchmarking and reliability testing

The result should improve the user’s chess while functioning as a serious C++ systems portfolio project rather than a decorative wrapper around an engine.
