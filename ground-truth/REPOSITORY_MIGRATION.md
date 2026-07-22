# Repository Documentation Migration

## Objective

Preserve required public docs while moving stale planning Markdown into a local-only archive and making `ground-truth/` canonical.

## Preflight

1. Run `git status --short`.
2. Record uncommitted changes.
3. List tracked/untracked Markdown and symlinks.
4. Search references to Markdown files.
5. Read existing `AGENTS.md` files.
6. Stop if a move would overwrite user work.

## Classify

### Preserve publicly

README, license notices, contribution/security/code-of-conduct docs, GitHub templates, active build docs, and dependency licenses.

### Canonical

The new `ground-truth/` package.

### Historical

Old PRDs, abandoned architecture docs, duplicate UX plans, scratch prompts, superseded roadmaps, meeting notes, and generated status reports.

### Unclear

Leave in place and report for human review.

## Sequence

1. Add `/archive/` to `.gitignore`.
2. Create a dated folder such as `archive/2026-07-pre-ground-truth/`.
3. Preserve useful grouping.
4. Move only historical files.
5. Repair links.
6. Verify `archive/` is absent from `git status`.
7. Run link/doc checks if present.
8. Create `ground-truth/MIGRATION_REPORT.md` listing original and new paths.
9. Do not commit.

## Rollback

The migration report must make every move reversible. Never delete archived files.
