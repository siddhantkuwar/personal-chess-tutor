# Chess Analysis Frontend North Star

This package converts the approved visual concept into an implementation contract for Codex.

It does **not** replace the existing C++ systems architecture. It narrows the next task to the production Analysis frontend and supplies one image as the visual source of truth.

## Install

Extract this package into the repository root.

It adds:

```text
ground-truth/
├── FRONTEND_NORTH_STAR.md
├── ANALYSIS_VISUAL_SPEC.md
├── VISUAL_ACCEPTANCE_CHECKLIST.md
├── CODEX_FRONTEND_REBUILD_PROMPT.md
├── CODEX_VISUAL_REPAIR_PROMPT.md
└── references/
    └── analysis-dark-north-star.png
```

These files augment the existing `ground-truth/` package.

Then run Codex from the repository root and paste the contents of:

```text
ground-truth/CODEX_FRONTEND_REBUILD_PROMPT.md
```

Do not give Codex the entire old multi-phase plan again. This task is deliberately frontend-only.
