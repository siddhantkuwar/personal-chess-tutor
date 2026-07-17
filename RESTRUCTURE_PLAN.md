# Personal Chess Tutor — Restructure Plan

## Product decision

Refocus the product around one job: **load a Chess.com game, understand the critical
moments, and learn directly on a trustworthy board without a Game Review paywall.**

The current drill/review system is not being deleted from storage or backend contracts in this
plan. Its navigation surface is removed for now and the **Train** destination becomes
**Explore**. That preserves optional future training work without making it the product's first
priority.

Visual direction: **Obsidian Study** — a minimal dark interface with restrained frosted surfaces,
warm ivory type, graphite boards, and one precise analysis accent. It takes inspiration from the
clarity and calm of ChatGPT's dark UI, not a copy of its branding. The board is the visual center;
glass is a surface treatment, never the product's main visual effect.

## Delivery status

| Phase | Status | Updated |
| --- | --- | --- |
| Phase 1 — Frictionless Chess.com ingest | **Complete** | 2026-07-16 |
| Phase 2 — Review as the product | **Complete** | 2026-07-16 |
| Phase 3 — Explore and visual progress | Planned | 2026-07-16 |

Phase 1 shipped as a backend and reliability phase with no UI changes. Its implementation includes
public-page player discovery, exact archive resolution, local archive indexing, resumable 7/30/90
day synchronization, interactive scheduling priority, structured status/recovery APIs, WebSocket
progress, cancellation, retries, and restart-safe persistence. The supplied game URL now resolves
successfully from a fresh instance without a preconfigured username.

## Reality check: resolved URL import defect

Originally observed on 2026-07-15 with the local app and this exact URL:

```text
https://www.chess.com/game/live/171626462440
POST /api/import → 400
"the public game page did not contain an extractable PGN"
```

`ImportService::from_url()` currently tries the public page for a normal shared link. It only uses
the Chess.com monthly archive endpoint if `player`, `year`, and `month` have been supplied as URL
query parameters. The public game page now returns HTML without the embedded PGN this importer
expects, so an opaque game ID is not enough for a reliable archive request.

The completed fix does not use a brittle authenticated-page scraper. The app is now a local,
user-configured **public account/archive sync client**: the user provides their public Chess.com
username once, then the app indexes that account's public archive locally and resolves shared game
links from archive PGNs. A bare link discovers the public players, tries their bounded recent
archives, and retains a specific recovery response when the PGN is unavailable.

---

## Phase 1 — Frictionless Chess.com ingest

**Status: Complete — 2026-07-16**

### Outcome

Paste a recent Chess.com link and land in Review. Connect a public Chess.com username once and
sync/reload an entire recent history without needing to find or paste PGN text.

### Product work

- Replace the one-field import modal with an import sheet offering:
  - **Paste game link** (default);
  - **Connect public Chess.com username** (local preference; no credential storage);
  - **Sync recent games** (7 / 30 / 90 days);
  - a compact manual-PGN fallback as the last option, not the main flow.
- Store a local `ChessComProfile` projection containing username, selected time controls, archive
  cursor, last successful sync, and a non-sensitive error summary.
- Resolve pasted links in this order:
  1. canonicalize and deduplicate the link;
  2. look up the game ID in the local archive index;
  3. if a connected username exists, scan relevant monthly public archives with bounded pagination
     and cache the resolved PGN;
  4. use the public game page only as a best-effort resolver, never the sole source of truth;
  5. if unresolved, show a recovery panel: choose username, search a date window, retry, or paste
     PGN — with the actual cause stated plainly.
- Add a first-run demo path: paste a link → username request only if needed → local sync → game
  opens automatically.

### Engineering work

- Introduce `ChessComArchiveClient` behind the existing import boundary. It owns archive discovery,
  monthly-game retrieval, bounded concurrency, retry/backoff, ETag/Last-Modified handling where
  available, and source provenance.
- Add persistent `archive_game_id → pgn/source/month` indexes. All data remains local.
- Add an API surface for profile setup, sync status, archive search, and import resolution; stream
  progress over the existing WebSocket contract.
- Keep hard limits: HTTPS-only allowlist, capped remote bodies, capped archive scan window,
  cancellation, and no cookies, passwords, or browser-session scraping.
- Extend tests with the exact failing opaque-link shape, archive indexing/resolution, changed page
  markup, duplicate links, partial sync/restart, rate-limit handling, and recovery copy.

### Acceptance criteria

- A connected user's pasted public game link imports from the local/archive index with no PGN step.
- A new link has clear resolver progress and either imports or gives a specific recovery action.
- A 30-day sync is resumable, deduplicated, and cannot starve the currently opened game.
- The existing example link has a regression test proving either successful archive resolution or a
  useful username/date recovery path — never the vague extract-PGN error.

### Completion evidence

- The exact example link resolves live from a fresh no-profile data directory and imports the PGN
  from `superking116`'s public archive.
- Normal unit, ingest, and integration targets pass, including repeated lifecycle/concurrency runs.
- AddressSanitizer, UndefinedBehaviorSanitizer, and ThreadSanitizer suites pass.
- Restart, cached duplicate import, rate-limit, cancellation, bounded-body, redirect-binding,
  archive-checkpoint, and interactive-priority behavior are covered by executable tests.
- No files under `web/` changed during Phase 1.

---

## Phase 2 — Review as the product

**Status: Complete — 2026-07-16**

### Outcome

The main screen feels like a calm, high-quality analysis workspace. Board movement, candidate
arrows, evaluation, and deep-analysis status are obvious at a glance.

### Information architecture

```text
Review                       Explore                     Progress
Import → Board → Mistakes    Opening / middlegame /      Rating / games / analysis
          → Engine evidence  endgame knowledge base      history over time
```

- Remove **Train** from primary navigation and replace it with **Explore**.
- Keep Review focused: import affordance, board, move rail, mistake/evidence panel, and a
  collapsible Engine Work panel. Avoid competing dashboard cards.
- Use a real 2D SVG chess set with consistent Staunton-inspired silhouettes, crisp vector edges,
  correct sizing, and strong black/white contrast. Do not use emoji or pseudo-3D imagery as the
  in-game piece set.
- Build board overlays in one SVG coordinate system: source/destination rings, precise candidate
  arrows, legal move dots, threat squares, and keyboard-focus treatment. Arrows must derive from
  UCI/FEN square coordinates and update only after the board position changes.

### Analysis visibility — transparent, not pretend reasoning

Do **not** fabricate Stockfish chain-of-thought or deliberately pretend the engine is still
working after it has finished. Instead, make observable analysis telemetry legible:

- staged skeleton loading for importing, reconstructing, shallow scanning, and deep analysis;
- a short minimum visual dwell (about 250–450 ms) only to prevent flicker, with an accessible
  reduced-motion path;
- an animated, non-textual waveform/pulse for active work;
- deep-analysis detail: current depth, nodes/second, MultiPV count, candidate positions complete,
  queue state, engine worker state, elapsed time, and latest evaluation line;
- expandable stage log that states facts such as “selected 3 candidate positions by evaluation
  swing”, not hidden reasoning;
- terminal failure states with Retry / Restart engine / View diagnostics actions.

### Visual system

- **Background:** near-black graphite with a very subtle board-grid texture.
- **Surfaces:** one translucent charcoal layer with a low-opacity border; use blur sparingly.
- **Type:** warm off-white hierarchy, muted gray metadata, one mint/cyan “analysis live” signal,
  and one amber “attention” signal. Red is reserved for loss/failure.
- **Motion:** board transitions 160–220 ms, overlay draw 220 ms, stage pulse 1.2 s; honour
  `prefers-reduced-motion`.
- **Responsiveness:** desktop is board-first with a fixed evidence rail; mobile keeps a square
  board and swaps Review / Moves / Engine panels rather than compressing them.

### Acceptance criteria

- “Show better move” always displays the intended move from the correct source square to the
  correct destination square; automated visual/coordinate tests cover promotion, captures, and
  reversed orientation.
- A user can tell whether deep analysis is queued, active, finished, failed, or unavailable
  without opening developer tools.
- The UI no longer presents train/daily-review as the core workflow.
- Review loading feels deliberate without obscuring or faking engine activity.

### Completion evidence

- Review now uses a board-first graphite workspace with SVG pieces, a fixed evidence rail, and
  responsive Review / Moves / Engine panel switching that leaves the mobile board visible.
- The URL importer consumes the asynchronous Phase 1 resolution contract and shows truthful
  link, archive, reconstruction, and analysis stages with a 320 ms anti-flicker dwell.
- Engine Work distinguishes queued, active, complete, and failed states and exposes only available
  operational facts: configured depth, completed lines, nodes, MultiPV, elapsed time, workers,
  queue size, position progress, retry, and diagnostics. It explicitly does not imitate hidden
  reasoning; live Stockfish depth/nodes remain unavailable until the backend emits them.
- Better-move overlays share the board coordinate system and are covered for capture-shaped moves,
  promotion parsing, and white/black orientation. Browser smoke confirmed the arrow mirrors after
  a board flip without changing the board position.
- The supplied Chess.com URL resolved in a fresh temporary instance to `superking116 vs.
  CartaaaaZ`, reached deep analysis, and displayed completed depth/node telemetry.
- Frontend type-check/build, native unit/ingest/integration suites, npm audit, sanitizer tests, and
  static security checks pass.

---

## Phase 3 — Explore and visual progress

### Outcome

Explore becomes an elegant local reference library. Progress becomes a concise personal story of
rating, activity, and analyzed games—not a second generic dashboard.

### Explore

- Create a local knowledge base with three first-class paths: **Openings**, **Middlegames**, and
  **Endgames**.
- Start with versioned, source-attributed Markdown/JSON content and a small taxonomy: theme,
  position/FEN, key plans, typical mistakes, model moves, and links to your imported games.
- Use an interactive mini-board and move tree for each entry; provide search, filters, and “seen
  in your game” links. Do not ship opaque generated opening advice without a source and version.
- Seed a focused v1: 12–20 high-value concepts rather than a fake encyclopaedia.

### Progress

- Add a clear Rapid rating hero with current rating, 30-day delta, and a high-contrast line chart.
  Pull public profile/stat data during the Phase 1 sync and persist dated snapshots locally.
- Add a visual activity ring only for transparent, named measures (for example, games played,
  games analyzed, and concepts explored). Never synthesize an “overall chess ability” score.
- Use a small activity timeline below the hero: games, rating movement, analysis completion, and
  notable mistakes. Each datum links back to its source game or archive snapshot.
- Design the page around two or three strong visualizations rather than tables of empty cards.

### Acceptance criteria

- Explore has useful opening/middlegame/endgame content, sources, and interactive board examples.
- Progress shows a meaningful 30-day rating/history visualization when synced data exists and a
  well-designed empty state when it does not.
- All displayed rating and activity claims have a local source timestamp and explainable formula.

---

## Delivery order and guardrails

1. Ship Phase 1 before the visual redesign: frictionless ingest is the product's highest-value
   fix and gives the redesigned Review screen real data.
2. Ship Phase 2 as a cohesive review-surface replacement, not isolated CSS tweaks.
3. Ship Phase 3 after the new navigation and sync model are stable.
4. Preserve the existing C++ chess/engine/storage boundaries. The redesign should not move chess
   truth into TypeScript.
5. Re-run the normal C++/frontend/package/real-Stockfish smoke checks after each phase. Repair the
   sanitizer and ThreadSanitizer linker qualification before claiming a fully green release again.

## Primary files likely to change

| Concern | Likely locations |
| --- | --- |
| Chess.com archive integration | `src/import/`, `include/pct/import/`, `src/app/`, `src/service/` |
| Import/sync API and WebSocket state | `src/service/http_server.cpp`, `web/src/api.ts`, `web/src/types.ts` |
| Review and board redesign | `web/src/main.ts`, `web/src/styles.css`, `web/src/chess.ts`, `web/src/icons.ts` |
| Explore knowledge base | `resources/`, `src/training/` or a new local content module, `web/src/` |
| Rating history/progress | `src/app/repository.cpp`, event schemas/storage tests, `web/src/main.ts` |
| Regression coverage | `tests/import_tests.cpp`, `tests/api_tests.cpp`, E2E tests, browser/visual checks |

## Out of scope for this restructure

- Chess.com credential storage, account automation, or scraping authenticated pages.
- A fake real-time Stockfish “thought stream” or hidden-chain-of-thought display.
- Replacing the local C++ chess/analysis core.
- A massive opening encyclopaedia before the Explorer's content format is proven.
