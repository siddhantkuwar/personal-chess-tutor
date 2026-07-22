# Codex Prompt: Visual Repair Pass

Use this only after the first production implementation.

Open these side by side:

- `ground-truth/references/analysis-dark-north-star.png`
- the latest 1440 × 900 production Analysis screenshot.

Do not add features.

Do not modify C++, APIs, persistence, or chess behavior.

Perform a visual-difference review and list the ten most important mismatches in:

- page proportions;
- board size;
- sidebar width;
- header height;
- right-rail density;
- shadow softness;
- border contrast;
- typography;
- spacing;
- move-row treatment;
- playback-control proportions;
- unnecessary content.

Then fix the five highest-impact mismatches.

Rules:

- preserve monochrome UI;
- keep color limited to move classifications;
- preserve the exact existing behavior;
- do not add dependencies;
- do not create another design variant;
- do not introduce new panels;
- do not write a long report before editing.

Capture a new 1440 × 900 screenshot, compare again, and perform one final focused correction pass.

Stop when the implementation is materially closer to the reference, not merely different from the previous UI.

Report only:

- mismatches fixed;
- files changed;
- screenshot path;
- tests run;
- remaining three highest-impact mismatches.

Do not commit.
