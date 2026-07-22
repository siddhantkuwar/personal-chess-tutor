# Visual Acceptance Checklist

The redesign is not accepted merely because it compiles.

## Composition

- [ ] Board is the dominant object.
- [ ] Entire primary workspace fits at 1440 × 900.
- [ ] Header is compact.
- [ ] No hero copy.
- [ ] No import banner above an active game.
- [ ] No large analysis-complete banner.
- [ ] Right rail contains only beginner-relevant content.
- [ ] Playback bar is visually calm.
- [ ] Evaluation bar belongs to the board.

## Style

- [ ] General UI is black, white, and gray.
- [ ] Navy accent is removed.
- [ ] Color appears only for chess classifications and chess meaning.
- [ ] Neumorphic depth is subtle and consistent.
- [ ] Borders remain visible enough for accessibility.
- [ ] No glassmorphism.
- [ ] No gradients used as decoration.
- [ ] No glowing borders.
- [ ] No excessive pills.
- [ ] No uppercase monospace label system.

## Information

- [ ] Current move classification is visible.
- [ ] Current move explanation is one concise sentence.
- [ ] Best move is visible.
- [ ] Best-move reason is one concise sentence.
- [ ] Nearby move history is visible.
- [ ] Opening appears in the header.
- [ ] Advanced engine details are hidden by default.
- [ ] No invented data is displayed.

## Functionality

- [ ] Existing game navigation works.
- [ ] Keyboard navigation works.
- [ ] Best-move reveal works.
- [ ] Retry still works.
- [ ] Variation mode still works.
- [ ] Canonical game state remains untouched.
- [ ] C++ contracts are unchanged.
- [ ] No chess-domain logic was added to TypeScript.

## Browser verification

- [ ] Reference image opened beside implementation.
- [ ] 1440 × 900 dark screenshot captured.
- [ ] 1280 × 800 dark screenshot captured.
- [ ] 1180 × 720 dark screenshot captured.
- [ ] Light theme remains functional, even if dark is the north star.
- [ ] No horizontal overflow.
- [ ] No clipped card content.
- [ ] Board remains square.
- [ ] Focus rings are visible.
- [ ] Reduced motion is respected.

## Subjective gate

The screen should be rejected if it still resembles:

- an internal dashboard;
- a documentation site;
- a table with a board attached;
- six separate cards competing for attention;
- a lightly restyled version of the previous screen.
