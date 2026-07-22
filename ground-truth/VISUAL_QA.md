# Visual QA

## Tool

Use Playwright or equivalent against the real running app.

## Matrix

Themes: system light and system dark.

Viewports: 1440×900, 1280×800, documented minimum.

Recent Games: populated, empty, selected, analyzing, failed.

Analysis: not analyzed, progress, completed, blunder selected, engine hidden/shown, retry, variation, pattern expanded, error.

Settings: defaults, advanced engine controls, theme override, coaching style.

## Interactions

Keyboard move navigation, focus order, visible focus, board input, variation enter/exit, retry, cancel, batch selection, sidebar, command palette, theme, reduced motion, and scroll behavior.

## Visual checks

- No clipping or accidental horizontal scroll
- Board remains square
- Inspector does not crush board
- Current move stays visible
- Numeric alignment
- Class colors distinguishable with labels/icons
- Errors readable
- Empty states actionable
- Tooltips do not hide critical controls
- Equivalent hierarchy in both themes
- No nested card farm
- No fake charts or copied assets

## Accessibility

Semantic controls, icon labels, color-independent classification, contrast, keyboard access, reduced motion, accessible evaluation/progress text, and non-disruptive completion announcements.

## Regression

Game URL import, playback, best move, analysis launch/result, persisted games, C++ bridge errors, and restart.

## Loop

Run, capture, compare, log defects, fix high-impact hierarchy/interaction issues, repeat, and report evidence. “Looks better” is not a test result.
