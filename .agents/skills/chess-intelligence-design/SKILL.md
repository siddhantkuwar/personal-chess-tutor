---
name: chess-intelligence-design
description: Audit, redesign, implement, and visually verify the React UX of a C++-backed local chess analysis workstation while preserving C++ ownership of chess-domain truth. Use for Recent Games, Analysis, Explore, Progress, settings, variations, classifications, patterns, themes, and frontend QA.
---

# Chess Intelligence Design Skill

## Required context

Read root `AGENTS.md`, `ground-truth/00-READ-FIRST.md`, the relevant feature specs, `DESIGN_SYSTEM.md`, `ENGINEERING_RULES.md`, and `VISUAL_QA.md`.

## Workflow

1. **Audit** the route, components, state flow, C++ contract, styles, rendered screen, states, keyboard behavior, and tests.
2. **Define the task**: user goal, primary information, what changes, what stays visible, and what can be hidden.
3. **Protect the boundary**: classify every new datum as existing C++ data, a needed typed contract, or pure UI state.
4. **Choose hierarchy**. Analysis: board → current verdict → explanation → move sequence → optional engine detail. Recent Games: identity → status → action → metadata.
5. **Use the system**: navy accent, system themes, restrained surfaces, limited radius, original class visuals, progressive disclosure.
6. **Implement incrementally** with existing primitives and stack.
7. **Verify in browser** with real data, both themes, supported widths, keyboard, overflow, and screenshots.
8. **Report** files, user flow, contracts, tests, screenshots, and gaps.

## Prohibited outcomes

- Chat layout
- Generic dashboard
- Fake data or progress
- Copied Chess.com assets
- Move legality or classification in TypeScript
- Full-repo rewrite without staged verification
- Nested card labyrinth
- Dependency replacement without evidence

## References

- `references/frontend-checklist.md`
- `references/analysis-layouts.md`
- `references/state-inventory.md`
