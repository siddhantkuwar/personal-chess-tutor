# Repository Instructions

## Identity

This is a **C++ systems project with a React interface**. It is a local-first macOS chess intelligence workstation, not a React app that occasionally launches Stockfish.

## Required reading

Before product, frontend, or architecture work, read:

1. `ground-truth/00-READ-FIRST.md`
2. `ground-truth/PRODUCT_VISION.md`
3. `ground-truth/SYSTEM_ARCHITECTURE.md`
4. `ground-truth/CPP_RUNTIME.md`
5. The relevant feature spec
6. `ground-truth/DESIGN_SYSTEM.md`
7. `ground-truth/ENGINEERING_RULES.md`
8. `ground-truth/VISUAL_QA.md`

Treat `ground-truth/` as authoritative. Never use `archive/` as current requirements.

## Ownership boundary

C++ owns:

- Chess.com import, refresh, and normalization
- PGN, SAN, FEN, board reconstruction, legal moves
- Stockfish UCI lifecycle
- Analysis queues, workers, cancellation, real progress events
- Evaluation caching and persistent local storage
- Move classification, opening recognition, pattern detection
- Variation validation and player-profile aggregation
- Future local-model interfaces

React owns:

- Layout, navigation, rendering, board interaction
- Presentation of analysis and progress
- Settings, charts, educational UI
- Accessibility, themes, keyboard controls
- Transient UI state that does not define chess truth

Never duplicate move legality, classification, opening truth, or pattern truth in TypeScript. Extend a typed C++ contract instead.

## Existing behavior to protect

Audit and preserve the reportedly working Analysis flow:

- Import Chess.com game by URL
- Show game and playthrough
- Navigate moves
- Show a best move
- Launch or display analysis

## Documentation migration

Follow `ground-truth/REPOSITORY_MIGRATION.md`:

- Inventory Markdown files.
- Preserve README, licenses, contribution/security docs, and GitHub templates.
- Add `/archive/` to `.gitignore` before moving anything.
- Move only obsolete, duplicated, draft, or superseded docs.
- Never delete historical docs merely to tidy the tree.
- Repair links and produce a migration report.
- Do not commit or push.

## Product defaults

- Recent Games is the launch view.
- Analysis starts only after the user presses Analyze.
- Batch analysis is supported.
- Progress reflects actual C++ stages, never a timer.
- Analysis is board-first with a contextual inspector.
- Move list, eval bar, move class, opening, and accuracy are visible by default.
- Engine lines, retry mode, automatic pattern display, and variation assistance are settings.
- Users may branch from any historical position and play legal variations indefinitely.
- Theme follows macOS system appearance by default.
- Accent is navy blue.
- Explanations are deterministic now, with a future local-model boundary.

## Design constraints

The interface should feel calm, intelligent, premium, and workstation-like. It must remain understandable to beginners and useful to streamers.

Do not produce:

- A ChatGPT clone
- A generic AI dashboard or card farm
- Fake progress
- Excessive gradients, glass, glow, pills, or giant whitespace
- Copied Chess.com layouts, icons, sounds, board palette, or classification glyphs
- A sweeping frontend rewrite that breaks C++ behavior

## Working method

1. Inspect the real repository.
2. Record current uncommitted work.
3. Run the current app and tests.
4. Capture current screenshots.
5. Write a plan tied to actual files.
6. Refactor incrementally.
7. Keep one working vertical slice.
8. Run tests and browser verification.
9. Report files, tests, screenshots, gaps, and risks.

## Git rules

- Do not commit.
- Do not push.
- Do not delete uncommitted work.
- Do not overwrite files before inspection.
- Keep `archive/` ignored and local.
