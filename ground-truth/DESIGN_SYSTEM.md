# Design System

## Direction

OpenAI-like restraint without copying OpenAI screens; premium consumer quality; serious chess workstation; quiet intelligence; system-native macOS behavior.

## Themes

Default to `system`, with explicit `light` and `dark` overrides.

## Palette

Use an original navy-led identity. Starting tokens:

```css
:root {
  color-scheme: light;
  --bg-canvas: #f5f6f8;
  --bg-surface: #ffffff;
  --bg-raised: #fbfcfd;
  --bg-subtle: #eef1f5;
  --text-primary: #121720;
  --text-secondary: #5b6575;
  --text-muted: #8791a1;
  --border-subtle: #dfe4eb;
  --border-strong: #c9d0da;
  --accent: #183a68;
  --accent-hover: #102e56;
  --accent-soft: #e7eef8;
  --focus-ring: #416d9f;
}
[data-theme="dark"] {
  color-scheme: dark;
  --bg-canvas: #111419;
  --bg-surface: #171b21;
  --bg-raised: #1d222a;
  --bg-subtle: #232a33;
  --text-primary: #f0f3f7;
  --text-secondary: #b1bac7;
  --text-muted: #7f8997;
  --border-subtle: #2a313b;
  --border-strong: #3a4350;
  --accent: #7397c3;
  --accent-hover: #8cadd2;
  --accent-soft: #1d314a;
  --focus-ring: #8fb4df;
}
```

Verify contrast in the real UI.

## Independent move-class colors

```css
--class-brilliant: #2b8c9e;
--class-great: #315f9e;
--class-best: #3974a8;
--class-excellent: #5d7f9f;
--class-good: #7b8794;
--class-book: #756b9e;
--class-inaccuracy: #bd8a2b;
--class-mistake: #c56b32;
--class-blunder: #b7484d;
--class-miss: #9a4f79;
```

Color is never the only indicator. Use original icons and labels; do not copy Chess.com glyphs or exact colors.

## Typography

Use local system stacks:

```css
--font-ui: ui-sans-serif, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
--font-mono: ui-monospace, "SFMono-Regular", Menlo, Monaco, Consolas, monospace;
```

Use tabular numerals for clocks and engine data.

## Spacing and radius

Base unit 4px; prefer 4, 8, 12, 16, 20, 24, 32, 40, 48. Controls 7–9px radius, panels 10–12px. Pills only for tags and compact states.

## Depth

Prefer borders and tonal separation to large shadows. Shadows belong to menus, popovers, dragged items, and modals.

## Motion

Motion explains state: analysis stages, panel reveal, move transitions, variation creation, and evaluation changes. Respect reduced motion. Avoid ambient animation.

## Board

Use low-noise flat squares, strong state highlights, readable arrows, and coordinates that do not compete with pieces. The default should support long review sessions, not resemble a luxury cutting board.

## Layout rules

- Board dominates Analysis.
- Controls sit near what they affect.
- Engine detail is progressively disclosed.
- Avoid nested cards.
- Prefer one strong surface with internal sections.
- Preserve dense scanning in move and game lists.
