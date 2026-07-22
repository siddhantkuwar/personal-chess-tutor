# Read First

## Product in one sentence

A local-first macOS chess intelligence workstation with a C++ runtime and React interface that provides unlimited Stockfish analysis, branching exploration from any position, personal pattern recognition, and long-term skill tracking.

## Product areas

### Analysis

Import or refresh games, press Analyze, observe real backend stages, review every move, reveal best continuations, retry mistakes, and create unlimited legal branches.

### Explore

Lessons and reference material organized around the user's games: openings, middlegames, endgames, tactical and positional patterns, and later drills.

### Progress

A local profile combining Chess.com rating history with accuracy, move quality, game phases, openings, personal patterns, and future training outcomes.

## Invariants

1. C++ is the source of chess truth.
2. React does not decide legality, evaluation, classification, or pattern truth.
3. Imported games are immutable canonical records.
4. Variations are branches, never edits to the canonical game.
5. Fixed engine/configuration/classifier versions should produce reproducible stored results.
6. Core functionality remains local.
7. Analysis progress is event-driven.
8. Local AI remains optional future scope.
9. Archived documentation is historical only.

## Scope now

- Documentation cleanup
- Design system and application shell
- Recent Games
- Real analysis-progress experience
- Analysis workstation
- Variation mode
- Settings and coaching styles
- Visual and regression QA

## Next

- Automatic recent-game refresh
- Stronger classification
- Pattern intelligence
- Personal weakness aggregation
- Explore and Progress backed by real data

## Future

- Local coaching model
- Edge-AI experiments
- Additional engines
- Native macOS packaging refinements
