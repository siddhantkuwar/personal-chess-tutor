# Frontend North Star

## Authority

For the production Analysis screen, this image is the primary visual reference:

```text
ground-truth/references/analysis-dark-north-star.png
```

When older design documents conflict with this visual direction, this document and the reference image win for the Analysis frontend.

## Product feeling

The interface should feel like:

- a calm, premium local tool;
- OpenAI/Vercel-like in restraint;
- soft neumorphism used with discipline;
- immediately understandable to a beginner;
- quiet enough for long analysis sessions;
- precise enough to feel like serious software.

It must not feel like:

- a generic AI dashboard;
- an admin portal;
- a gaming launcher;
- a chat application;
- a grid of unrelated cards;
- a copy of Chess.com;
- a design-token demo assembled by an agent at 3 a.m.

## Visual language

### Monochrome first

Use black, charcoal, graphite, soft gray, and white.

Do not use navy as a global accent.

Color is reserved for move classifications and critical chess meaning:

- Brilliant
- Great
- Best
- Excellent
- Good
- Book
- Inaccuracy
- Mistake
- Blunder
- Miss

Navigation, controls, focus states, and general UI remain monochrome.

### Soft neumorphism

Use soft elevation and inset depth to clarify hierarchy.

Neumorphism should be subtle:

- broad, low-opacity shadows;
- restrained highlights;
- rounded but not inflated surfaces;
- no glossy plastic;
- no glassmorphism;
- no glowing borders;
- no floating bubbles everywhere.

The screen should still have readable boundaries and accessible contrast.

### Board first

The chessboard is the visual center of gravity.

Everything else exists to explain the selected position.

The user should understand the screen in this order:

1. Board position
2. Current move verdict
3. Better move
4. Nearby move history
5. Navigation controls
6. Optional deeper analysis

## Information architecture

The approved composition contains:

- slim vertical sidebar;
- compact horizontal game header;
- large board/evaluation workspace;
- simple right-side review rail;
- bottom playback control bar.

The initial viewport should contain the complete primary workflow without vertical scrolling.

## Beginner-first behavior

Default visible information:

- current move classification;
- one-sentence explanation;
- best move;
- one-sentence explanation of why;
- recent move list;
- evaluation;
- game identity and opening;
- playback controls.

Hidden or secondary by default:

- principal variations;
- nodes and depth;
- pattern evidence;
- multiple alternatives;
- technical diagnostics;
- player model statistics;
- long prose;
- coaching configuration.

## Functional truth

This visual redesign must preserve the existing C++ ownership boundary.

C++ remains authoritative for:

- legal moves;
- FEN/PGN/SAN;
- Stockfish;
- classifications;
- accuracy;
- openings;
- variations;
- analysis progress;
- retry validation;
- persistence.

The frontend renders these results. It does not recreate them.
