# Analysis Visual Specification

## Target viewport

Primary design target:

```text
1440 × 900
```

Also verify:

```text
1280 × 800
1180 × 720
```

The first release remains desktop-first.

## Overall frame

Use a dark neutral canvas with modest outer padding.

Suggested structure:

```text
┌─────────┬─────────────────────────────────────────────────────┐
│ Sidebar │ Header                                              │
│         ├────────────────────────────────┬────────────────────┤
│         │ Evaluation + Board             │ Review rail        │
│         ├────────────────────────────────┴────────────────────┤
│         │ Playback controls                                   │
└─────────┴─────────────────────────────────────────────────────┘
```

At 1440 × 900:

- outer padding: 24–32 px;
- sidebar: 92–104 px;
- gap after sidebar: 28–36 px;
- header height: 72–84 px;
- board area: approximately 590–640 px square;
- evaluation rail: 56–72 px;
- right rail: 320–360 px;
- bottom controls: 78–94 px;
- major gaps: 20–28 px.

Do not let the board shrink to make room for prose.

## Background and surfaces

Suggested dark tokens:

```css
--canvas: #101112;
--surface-1: #17191b;
--surface-2: #1d1f22;
--surface-3: #23262a;
--text-1: #f4f4f2;
--text-2: #b8babd;
--text-3: #777b80;
--line: rgba(255, 255, 255, 0.08);
--line-strong: rgba(255, 255, 255, 0.13);
```

These are starting values. Tune them against the rendered reference.

## Neumorphic depth

Raised surface:

```css
box-shadow:
  -8px -8px 18px rgba(255, 255, 255, 0.025),
  10px 12px 28px rgba(0, 0, 0, 0.48);
```

Inset surface:

```css
box-shadow:
  inset 3px 3px 8px rgba(0, 0, 0, 0.42),
  inset -2px -2px 7px rgba(255, 255, 255, 0.025);
```

Use a thin border in addition to shadows:

```css
border: 1px solid rgba(255, 255, 255, 0.07);
```

Do not use heavy drop shadows on every row or label.

## Radius

- app frame and major surfaces: 24–30 px;
- sidebar/header/bottom bar: 24–28 px;
- review cards: 20–24 px;
- compact controls: 14–18 px;
- selected move row: 12–14 px.

## Typography

Use local system fonts:

```css
font-family:
  ui-sans-serif,
  -apple-system,
  BlinkMacSystemFont,
  "Segoe UI",
  sans-serif;
```

Preferred scale:

- game title: 16–18 px, 600;
- opening metadata: 15–16 px, 400;
- card label: 13–14 px, 500;
- verdict: 18–22 px, 650;
- selected move: 22–26 px, 600;
- explanation: 15–17 px, 400;
- move list: 14–15 px;
- navigation labels: 13–14 px.

Avoid oversized hero typography and repetitive uppercase monospace eyebrows.

## Sidebar

The sidebar is a quiet tool rail, not a mini dashboard.

Contents:

- product mark at top;
- Analysis;
- Review or Recent Games;
- Openings/Explore;
- Lessons or Progress;
- Library if it exists;
- Settings at bottom.

Rules:

- monochrome icons;
- one selected item receives a soft inset/raised state;
- no navy fill;
- no letter-in-square navigation;
- labels remain readable;
- sidebar does not compete with the board.

If existing product routes differ, map them cleanly without inventing fake features.

## Header

One compact raised bar.

Left side:

```text
Paul Morphy vs. Duke Karl  •  Italian Game: C50
```

Right side:

- compact Overview action if real;
- overflow menu;
- analysis status only if needed.

Remove:

- giant page title;
- marketing slogan;
- full-width import banner;
- large completion banner.

Import belongs on Recent Games, not above an active review.

## Board workspace

### Board container

The board sits inside one large raised surface with generous but controlled padding.

The board itself remains flat enough for piece readability.

Use grayscale board squares with sufficient contrast:

```css
--board-light: #46484b;
--board-dark: #2a2c2f;
```

Tune from the reference rather than accepting these blindly.

### Evaluation bar

The evaluation bar is visually attached to the board container.

It contains:

- positive value at top;
- negative value at bottom;
- clear white/black fill;
- subtle current marker;
- no detached spreadsheet styling.

### Board highlights

Use move-classification color only for chess meaning.

Examples:

- selected inaccurate move: amber outline;
- best move: green arrow or target;
- last move: neutral monochrome highlight unless classification is being taught;
- legal targets: subtle neutral dots.

## Right review rail

The right rail contains three primary surfaces.

### 1. Current Move

Content:

- small label;
- classification icon and title;
- selected move;
- one concise explanation.

Example:

```text
Current Move

Inaccuracy
14... Qe6

This move allows White to create threats.
```

### 2. Best Move

Content:

- classification icon and title;
- best move;
- one concise reason.

Example:

```text
Best Move
14... dxe6

Maintains balance and keeps the position equal.
```

### 3. Move List

Show only nearby moves by default.

Requirements:

- current row visibly raised or inset;
- selected move classification appears at row end;
- no full spreadsheet;
- allow “Show all moves” expansion;
- preserve keyboard navigation;
- keep list compact.

Opening and accuracy do not need their own permanent card in this design. Put opening in the header and accuracy in Overview or a compact secondary disclosure unless product testing proves otherwise.

## Bottom playback bar

One wide raised surface.

Controls:

- jump to start;
- previous;
- play/pause;
- next;
- jump to end;
- current move selector or move label.

Rules:

- large touch targets;
- monochrome;
- selected or active state through depth, not color;
- no clutter;
- no technical engine controls here.

## Classification colors

Only chess semantics receive chroma.

Suggested starting values:

```css
--brilliant: #34b6aa;
--great: #4b8ed8;
--best: #74b84a;
--excellent: #7fb65c;
--good: #969a9f;
--book: #9a83c2;
--inaccuracy: #f0a51a;
--mistake: #e8752e;
--blunder: #d84a4a;
--miss: #bb5a91;
```

Keep the rest of the UI monochrome.

## Interaction and motion

- hover: small elevation change, 100–140 ms;
- press: inset state, 70–100 ms;
- selected move: subtle raised row;
- board transition: 140–200 ms;
- panel content transition: short fade/translate, 140–180 ms;
- respect reduced motion.

No ambient animation.

## Hidden advanced analysis

Advanced content should open through Overview, overflow, or an inspector disclosure.

Possible content:

- principal variation;
- depth and nodes;
- alternate moves;
- pattern evidence;
- accuracy breakdown;
- variation tree.

Do not place these on the default screen.

## Responsive behavior

At narrower desktop widths:

1. preserve board size as long as possible;
2. reduce outer gaps;
3. narrow the sidebar;
4. reduce right-rail width slightly;
5. collapse labels only after preserving usability;
6. never move the primary explanation below the initial viewport unless width is genuinely insufficient.

No polished mobile implementation is required for this milestone.
