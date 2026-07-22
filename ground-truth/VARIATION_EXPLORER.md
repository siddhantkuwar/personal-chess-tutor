# Variation Explorer

## Goal

Let the user play legal moves indefinitely from any point without changing the imported game.

## Model

```text
VariationTree
├── rootPositionId
├── rootPly
└── nodes
    ├── parentId
    ├── move
    ├── resultingPosition
    ├── children
    ├── evaluation
    └── annotations
```

C++ validates every move and owns resulting positions.

## Required actions

- Enter from any canonical move or branch
- Make legal moves
- Navigate backward/forward
- Create sibling branches
- Request engine evaluation
- Delete a local branch with confirmation
- Return to canonical game in one action
- Copy FEN or PGN fragment
- Reset to branch root

## UI

Use a subtle board-frame change, breadcrumb to the root, a visible `Variation` state, and a `Return to game` action. Do not deploy a full warning banner; the user has entered a branch, not launched a missile.

## Engine setting

Variation analysis may be off, on-demand, or continuous after idle. Continuous work is cancellable and lower priority than an explicit review.

## Persistence

Keep branches local, autosave meaningful changes, associate with game/root ply, and store the engine configuration used.

## Correctness

- Illegal moves return a structured C++ error.
- Branch navigation never mutates canonical moves.
- Position is reproducible from root plus branch moves.
- Transpositions may share cached evals while remaining distinct user branches.
- Undo does not delete sibling branches.

## Acceptance

A user can branch from any position, continue until termination, return exactly to the canonical state, and recover saved branches after restart when persistence is enabled.
