# Documentation Migration Report

Date: 2026-07-21

## Scope

The supplied Chess Intelligence ground-truth package was installed at the repository root. The
archive is local-only through `/archive/` in `.gitignore`; visual QA evidence is likewise ignored.
No documentation was deleted.

## Canonical installation

| Package path | Repository path |
| --- | --- |
| `chess-intelligence-ground-truth/AGENTS.md` | `AGENTS.md` |
| `chess-intelligence-ground-truth/ground-truth/` | `ground-truth/` |
| `chess-intelligence-ground-truth/.agents/skills/chess-intelligence-design/` | `.agents/skills/chess-intelligence-design/` |

The package README, manifest, checksum list, ignore snippet, and stray `.Rhistory`/`.DS_Store`
files are retained under `archive/2026-07-pre-ground-truth/package/`.

## Historical moves

Every move below is reversible by moving the archived file back to its original path.

| Original path | Local archive path | Reason |
| --- | --- | --- |
| `RESTRUCTURE_PLAN.md` | `archive/2026-07-pre-ground-truth/RESTRUCTURE_PLAN.md` | Superseded implementation brief |
| `docs/personal_chess_tutor_prd.md` | `archive/2026-07-pre-ground-truth/docs/personal_chess_tutor_prd.md` | Superseded PRD |
| `docs/phase_1_trustworthy_game_analyzer.md` | `archive/2026-07-pre-ground-truth/docs/phase_1_trustworthy_game_analyzer.md` | Completed phase plan |
| `docs/phase_2_1_chronological_game_review_rebuild.md` | `archive/2026-07-pre-ground-truth/docs/phase_2_1_chronological_game_review_rebuild.md` | Completed phase plan |
| `docs/phase_2_2_openai_chess_studio.md` | `archive/2026-07-pre-ground-truth/docs/phase_2_2_openai_chess_studio.md` | Superseded visual direction |
| `docs/phase_2_personalized_training_system.md` | `archive/2026-07-pre-ground-truth/docs/phase_2_personalized_training_system.md` | Completed phase plan |
| `docs/phase_3_scale_hardening_and_portfolio_release.md` | `archive/2026-07-pre-ground-truth/docs/phase_3_scale_hardening_and_portfolio_release.md` | Completed phase plan |

## Preserved publicly

- `README.md`
- `web/public/pieces/ASSET_AUDIT.md`
- `web/public/pieces/lasker/ATTRIBUTION.md`
- `web/public/pieces/lasker/LICENSE.md`
- `.github/` templates and workflows
- Active release, build, security, and packaging HTML documentation

## Left for human review

- `restructure-plan.html` is tracked and may still be useful historical visual context. It remains
  in place but now identifies `ground-truth/REDESIGN_PLAN.md` as canonical.
- Private generated content under ignored `docs/` is not Markdown and was not migrated.

## Link repairs and verification

- `README.md` now identifies `ground-truth/` as canonical and distinguishes local historical docs.
- `restructure-plan.html` no longer points to the archived Markdown brief.
- `web/public/pieces/lasker/ATTRIBUTION.md` continues to resolve its local license link.
- `archive/` does not appear in `git status --short --untracked-files=all`.
