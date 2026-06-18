# Phase 1 Design System

The accepted references are [desktop](concepts/phase-1-desktop.png) and
[mobile](concepts/phase-1-mobile.png). They define the visual contract for the
game-analysis surface.

## Direction

The interface resembles an annotated chess study book: calm, rigorous, and
board-first. It uses open regions separated by rules rather than a dashboard of
floating cards. The background is a visibly warm white, not pure white.

## Tokens

| Role | Value |
| --- | --- |
| Canvas | `#f7f5ef` |
| Surface | `#fffefa` |
| Text | `#262823` |
| Muted text | `#66685f` |
| Rule | `#d7d3c8` |
| Sage | `#3f743e` |
| Sage soft | `#e3ebdf` |
| Rust | `#c54d35` |
| Rust soft | `#f7e4dd` |
| Board light | `#eadbb9` |
| Board dark | `#a98861` |
| Radius small | `4px` |
| Radius medium | `8px` |
| Content font | `Georgia, 'Times New Roman', serif` |
| Chrome font | `Inter, ui-sans-serif, system-ui, sans-serif` |

Spacing uses a 4px base with primary steps of 8, 12, 16, 24, 32, and 48px.
Controls have a minimum 44px hit target. Shadows are reserved for transient
overlays; persistent regions use 1px rules.

## Desktop Composition

- Quiet 58px header with brand at left and essential navigation at right.
- Full-width import row directly below the header.
- Three-column workspace: square board, move/analysis rail, review pane.
- The board remains the visual focal point and receives the largest column.
- Move selection uses sage; mistake annotations use rust.
- The review pane scrolls independently and keeps one lesson expanded.
- A slim footer states local-only behavior and engine readiness.

## Mobile Composition

- Stack header, import action, game status, board, navigation, and review.
- Preserve a full-width square board and 44px minimum navigation controls.
- Use `Game`, `Moves`, and `Review` views instead of compressing desktop rails.
- Show the next review content below the board so the vertical workflow remains clear.

## Component Families

- `AppHeader`: desktop navigation and compact mobile menu
- `ImportBar`: URL/PGN entry, progress, and error state
- `GameSummary`: players, result, and progressive analysis stages
- `ChessBoard`: coordinates, pieces, selected square, arrows, and highlights
- `MoveNavigator`: first, previous, next, last, and selected SAN
- `MoveList`: selected ply, keyboard navigation, and phase markers
- `AnalysisRail`: progress plus optional engine evaluation details
- `MistakeReview`: ranked mistake rows and one expanded explanation
- `LocalStatus`: data-location and engine-readiness information

## Allowed First-Viewport Copy

- Personal Chess Tutor
- Import game
- Study
- Settings
- Help
- Alex vs. Morgan
- 0-1
- Parsed
- Shallow scan
- Deep analysis 2 of 5
- Three moments to review
- Your bishop was left undefended
- Hanging piece
- Show better move
- Engine details
- Local-only storage and engine status text

Dynamic game data may replace the sample names, moves, result, evaluation, and
mistake explanation. No marketing claims or synthetic metrics belong in this view.
