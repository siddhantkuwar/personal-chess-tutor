# Codex Master Prompt

Run this from the repository root.

---

You are restructuring the UX and frontend architecture of this repository. This is a C++ systems project with a React presentation layer. Do not turn it into a TypeScript chess engine, a chat app, or a generic AI dashboard.

## Inspect first

Read root `AGENTS.md` and `ground-truth/` in the required order.

Audit the real repository before modifying it. Identify:

- Frontend framework, package manager, build commands
- Routes, state management, components, chessboard implementation
- C++ build/runtime topology
- Frontend-to-C++ transport and API types
- Tests and visual tooling
- All Markdown files
- Current uncommitted changes

Run the app and verify the reported working Analysis flow: import Chess.com link, display game, navigate playthrough, show best move, and launch/display analysis.

Do not begin a broad refactor until you can explain how these behaviors work in actual files.

## Migrate documentation

Follow `ground-truth/REPOSITORY_MIGRATION.md`.

- Add `/archive/` to `.gitignore` first.
- Create a local ignored archive.
- Preserve required public docs.
- Move only obsolete/duplicate/draft/superseded Markdown.
- Leave ambiguous files and report them.
- Repair links.
- Verify archive is absent from Git status.
- Create `ground-truth/MIGRATION_REPORT.md`.
- Never commit or push.

## Preserve the C++ boundary

C++ owns import/sync, PGN/SAN/FEN, legality, Stockfish, scheduling/progress, cache/storage, classification, opening recognition, pattern detection, variations, player modeling, and future local-model interfaces.

React owns presentation and interaction.

When UI needs new information, extend a typed C++ contract. Never recreate chess-domain logic in React.

## Product direction

Recent Games → select game(s) → press Analyze → real backend progress → review every move → move list/eval/class/opening/accuracy → optional engine detail → retry → unlimited legal variation → return to canonical game.

Default theme follows macOS system appearance; original navy-led identity; beginner-friendly but capable for streamers.

Do not copy Chess.com's layout, icons, palette, sounds, classification glyphs, copy, or algorithm. Do not copy ChatGPT's conversational layout.

## Configurable behavior

Engine lines, best-move reveal, retry, variation assistance, automatic patterns, technical detail, explanation depth, analysis depth, MultiPV, CPU threads, hash memory, and coaching style.

Styles: Beginner and friendly; Cynical and hard; Devil's advocate.

Explanations are deterministic now. Preserve a future local `ICoachingProvider` boundary.

## Staged implementation

### A. Audit

Produce a report with file paths, commands, contracts, behavior, risks, and current screenshots.

### B. Documentation migration

Complete the ignored local archive and canonical ground truth.

### C. Foundation

Consolidate themes, tokens, typography, focus, primitives, shell, sidebar, errors, and empty states. Reuse the existing stack unless a dependency change is justified.

### D. One vertical slice

Implement and verify:

Recent game → Analyze → real progress from C++ → completed review → navigate → evaluation/class/opening/accuracy → reveal best move → enter variation → play legal moves → return.

Do not redesign all secondary pages before this works.

### E. Secondary screens

Then improve Explore, Progress, and Settings using real data only.

## Verification

Run existing tests and add focused tests. Use Playwright or equivalent on the real app in both themes at 1440×900, 1280×800, and the minimum width. Exercise import, analyze, cancel, navigation, retry, variation, settings, and errors. Follow `ground-truth/VISUAL_QA.md`.

## Final report

Report audit, Markdown migration, files changed, architecture decisions, API changes, tests, screenshots, performance notes, gaps, risks, and exact next stage.

Do not commit or push.

Begin with the audit and implementation plan. Do not make sweeping code changes in the first response.

---
