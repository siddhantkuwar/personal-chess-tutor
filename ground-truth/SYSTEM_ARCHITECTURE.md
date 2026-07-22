# System Architecture

```text
React presentation
  routes, components, board, settings, charts
            │ typed local API / IPC / HTTP
C++ application services
  import, analysis orchestration, review assembly, profiles
        ┌───┴───────────────────────────┐
C++ chess domain                Systems infrastructure
board, legality, PGN,           Stockfish, scheduler, cache,
variations, patterns            storage, networking, logging
```

## Layers

### Presentation

React renders state and sends user intent. It does not derive chess truth.

### Application

C++ use cases such as:

- ImportGameFromUrl
- SyncRecentGames
- AnalyzeGame / CancelAnalysis
- GetReview
- CreateVariation / ExtendVariation
- GetPlayerProfile
- UpdateAnalysisSettings

### Domain

Game, Move, Position, Variation, Evaluation, Classification, Opening, Pattern, Weakness, Drill, AnalysisConfiguration.

### Infrastructure

Stockfish adapter, Chess.com client, storage, thread pool, cache, metrics, and the local transport layer.

## Suggested interfaces

```cpp
class IChessEngine;
class IGameImporter;
class IGameRepository;
class IAnalysisRepository;
class IAnalysisScheduler;
class IOpeningBook;
class IPatternDetector;
class IPlayerModelStore;
class ICoachingProvider;
```

Adapt names to the existing code; preserve the separation.

## Frontend contract

React consumes structured objects and explicit enums, not parsed logs.

```json
{
  "analysisId": "a_123",
  "gameId": "g_456",
  "status": "classifying",
  "completedUnits": 51,
  "totalUnits": 72,
  "message": "Classifying move quality"
}
```

## Canonical game versus variations

```text
ImportedGame (immutable canonical move sequence)
└── AnalysisSession
    └── VariationTree
        ├── Branch A
        └── Branch B
```

A branch stores a root position plus legal moves. It never rewrites the PGN.

## Failure boundaries

- Stockfish failure cannot crash the UI.
- Import failure cannot corrupt history.
- Cancellation ends in an explicit cancelled state.
- Malformed PGN produces a structured error.
- Storage distinguishes incomplete tails from valid records.
- React renders partial and failed states deliberately.
