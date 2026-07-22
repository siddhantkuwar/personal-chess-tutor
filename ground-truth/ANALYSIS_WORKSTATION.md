# Analysis Workstation

## Purpose

This is the flagship experience. The board, move sequence, evaluation, and explanation must behave as one coordinated instrument.

## Default layout

```text
┌───────────────────────────────────────────────────────────────┐
│ Game identity · Opening · Accuracy · Analysis status          │
├──────┬────────────────────────────┬───────────────────────────┤
│ Eval │                            │ Move list / Inspector      │
│ bar  │        Chessboard          │ Context changes by mode   │
├──────┴────────────────────────────┴───────────────────────────┤
│ Playback · Retry · Variation · Display controls               │
└───────────────────────────────────────────────────────────────┘
```

Move list and inspector may use a split or tabs, but the board remains dominant.

## Always visible

- Chessboard
- Evaluation bar
- Move list
- Move classification
- Opening name
- Accuracy
- Playback controls
- Analysis state

## Configurable

Engine lines, technical evaluation data, best-move arrows, retry before reveal, automatic pattern panel, threat overlays, material, critical moments, variation tree, and engine diagnostics.

## Modes

### Review

Selecting a move updates board, evaluation, classification, explanation, best move, opening state, patterns, and relevant engine line.

### Retry

1. Hide the answer.
2. Return to the position before the mistake.
3. Accept a legal user move.
4. Compare the attempt.
5. Offer progressive hints when enabled.
6. Reveal the recommended line.
7. Record the attempt for future training.

### Variation

Branch from any canonical or variation node. See `VARIATION_EXPLORER.md`.

## Analysis launch

Analysis begins only after explicit action. Show truthful stages such as:

```text
Preparing game
Reconstructing 72 positions
Evaluating positions
Classifying 36 moves
Detecting personal patterns
Saving review
Ready
```

Every stage and count comes from C++ events. Batch mode shows current game, completed games, remaining games, overall progress, and cancel.

## Explanation hierarchy

Beginner default:

1. Verdict
2. One-sentence explanation
3. What changed on the board
4. Better move
5. Personal pattern or lesson
6. Optional technical detail

Advanced disclosure may show evaluation before/after, search depth, nodes, MultiPV, engine version, and classification rationale.

## Keyboard

Suggested shortcuts:

- Left/right: previous/next ply
- Up/down: previous/next critical moment
- Home/end: start/end
- Space: play/pause
- V: variation mode
- R: retry
- B: reveal best move
- F: flip board
- Escape: leave transient mode

Expose shortcuts in help.

## Acceptance

- Existing import/playback/best-move/analysis still works.
- A beginner understands the main error without engine lines.
- The whole game is keyboard navigable.
- Retry is configurable.
- Branching works from any valid position.
- Progress matches backend events.
- Board remains usable at minimum desktop width.
