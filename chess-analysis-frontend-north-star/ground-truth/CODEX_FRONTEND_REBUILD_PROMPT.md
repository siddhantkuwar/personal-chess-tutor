# Codex Prompt: Build the Approved Analysis Frontend

Use the `chess-intelligence-design` skill.

Read:

- `AGENTS.md`
- `ground-truth/FRONTEND_NORTH_STAR.md`
- `ground-truth/ANALYSIS_VISUAL_SPEC.md`
- `ground-truth/VISUAL_ACCEPTANCE_CHECKLIST.md`
- `ground-truth/references/analysis-dark-north-star.png`

The image is the visual source of truth for the production Analysis screen.

## Goal

Replace the current Analysis screen's visual composition with the approved calm, monochrome, softly neumorphic workstation.

This is no longer a concept sprint.

Implement the selected direction in the real production Analysis route using the repository's actual frontend stack and real data.

## Non-negotiable boundaries

- Do not modify C++.
- Do not modify HTTP or WebSocket contracts.
- Do not modify persistence.
- Do not change classification rules.
- Do not change variation or retry behavior.
- Do not migrate to React.
- Do not add a UI framework.
- Do not add a new state-management library.
- Do not invent metrics.
- Do not copy Chess.com assets.
- Do not commit or push.

The audited repository uses Vite with vanilla TypeScript string/template rendering. Preserve that stack.

You may split oversized frontend files into focused presentation modules when doing so does not change behavior.

## Remove from the current production Analysis screen

- giant “Review every decision” header;
- page-level marketing language;
- full-width Import Game row;
- giant analysis-complete banner;
- navy global accent;
- uppercase monospace eyebrow labels;
- letter-in-square sidebar icons;
- spreadsheet-like full-height move table;
- repeated bordered rectangles;
- opening/accuracy card if it competes with the beginner explanation;
- technical engine detail from the default view.

## Build this composition

At 1440 × 900:

1. Slim left sidebar, approximately 92–104 px.
2. Compact game header, approximately 72–84 px tall.
3. Large central board surface with integrated evaluation rail.
4. Right review rail, approximately 320–360 px.
5. Bottom playback surface, approximately 78–94 px tall.

The board should be approximately 590–640 px square and remain the dominant object.

## Default right rail

Render exactly three primary sections:

### Current Move

- classification;
- selected move;
- one concise sentence.

### Best Move

- best move;
- one concise reason.

### Move List

- nearby moves only;
- current row clearly selected;
- classification at row end;
- “Show all moves” disclosure.

Advanced engine lines, depth, nodes, alternatives, pattern evidence, and technical diagnostics remain hidden behind existing disclosure behavior.

## Visual direction

- black, charcoal, gray, and white;
- no navy global accent;
- color only for move classifications;
- subtle neumorphic elevation and inset controls;
- soft but readable borders;
- system fonts;
- no gradients, glass, glow, card grid, or chat layout;
- no giant typography.

Use the supplied image to match:

- composition;
- density;
- proportions;
- hierarchy;
- softness;
- control sizing;
- surface depth.

Do not attempt to reproduce generated image artifacts or impossible chess-piece details. Match the interface design.

## Functional preservation

Before editing, record the current behavior of:

- move navigation;
- keyboard navigation;
- evaluation display;
- current classification;
- best move;
- retry;
- variation entry and return;
- engine-line disclosure;
- analysis status.

After editing, verify all of them again.

C++ remains the source of every chess fact.

## Implementation sequence

1. Capture a “before” screenshot at 1440 × 900.
2. Inspect the real Analysis route and identify the smallest presentation-only refactor.
3. Create or consolidate monochrome visual tokens.
4. Implement the page frame, sidebar, header, board surface, right rail, and playback bar.
5. Bind existing real data.
6. Preserve all hidden advanced features through disclosure or overflow controls.
7. Run the application.
8. Compare the result side-by-side with the reference image.
9. Correct the ten largest visual mismatches.
10. Repeat the screenshot comparison once more.
11. Run existing frontend checks and relevant browser flows.

Do not stop after producing a plan. Implement the screen.

## Required screenshots

Save locally ignored evidence for:

- dark 1440 × 900;
- dark 1280 × 800;
- dark 1180 × 720;
- current move selected;
- best move revealed;
- move list expanded;
- variation mode;
- retry mode.

Also confirm that light mode still renders without broken contrast, but dark mode is the visual north star.

## Done when

- the production Analysis route visibly follows the reference composition;
- the board is the dominant object;
- no initial vertical scroll is needed at 1440 × 900;
- no navy global accent remains;
- only classification semantics use color;
- current and best move explanations are visible immediately;
- move history is compact;
- advanced engine detail is hidden by default;
- all existing behavior still works;
- no C++ or API files changed;
- screenshots and test results are reported.

Finish with:

- files changed;
- functions or components moved;
- confirmation that C++ and APIs were untouched;
- exact verification commands;
- screenshot paths;
- remaining visual mismatches.

Do not commit.
