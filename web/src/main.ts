import "./styles.css";
import { advanceDrillHint, beginDrillSession, completeResource, generateSupplementalDrills, importBatch, importGameObservable, listGames, loadBatches, loadDiagnostics, loadDrills, loadGame, loadImportResolution, loadProfile, loadResources, loadRuntimeSettings, setQueuePaused, startAnalysis, submitDrillAttempt } from "./api";
import { moveOverlayGeometry, squaresFromFen, uciSquares } from "./chess";
import type { BoardOrientation } from "./chess";
import { icons } from "./icons";
import type { BatchProgress, Diagnostics, Drill, Job, Mistake, Profile, ProgressSocketMessage, ResourceRecommendation, RuntimeSettings, StoredGame } from "./types";

type MobileView = "review" | "moves" | "engine";
type AppMode = "game" | "training" | "explore" | "progress";

interface State {
  game: StoredGame | null;
  selectedPly: number;
  expandedMistake: number;
  highlightedUci: string;
  job: Job | null;
  error: string;
  busy: boolean;
  mobileView: MobileView;
  mode: AppMode;
  drills: Drill[];
  activeDrill: string;
  shownHint: number;
  drillStartedAt: number;
  profile: Profile | null;
  resources: ResourceRecommendation[];
  attemptMessage: string;
  batchMessage: string;
  showPunishment: boolean;
  batches: BatchProgress[];
  queuePaused: boolean;
  cacheHits: number;
  engineExpanded: boolean;
  jobStartedAt: number;
  importStage: "idle" | "link" | "resolving" | "reconstructing" | "analyzing";
  boardOrientation: BoardOrientation;
  diagnostics: Diagnostics | null;
  runtimeSettings: RuntimeSettings | null;
}

const initialFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const blackPieces = new Set(["♟", "♜", "♞", "♝", "♛", "♚"]);
const pieceKinds: Record<string, string> = {
  "♔": "king",
  "♕": "queen",
  "♖": "rook",
  "♗": "bishop",
  "♘": "knight",
  "♙": "pawn",
  "♚": "king",
  "♛": "queen",
  "♜": "rook",
  "♝": "bishop",
  "♞": "knight",
  "♟": "pawn",
};
const state: State = {
  game: null,
  selectedPly: 0,
  expandedMistake: 0,
  highlightedUci: "",
  job: null,
  error: "",
  busy: false,
  mobileView: "review",
  mode: "game",
  drills: [],
  activeDrill: "",
  shownHint: 0,
  drillStartedAt: Date.now(),
  profile: null,
  resources: [],
  attemptMessage: "",
  batchMessage: "",
  showPunishment: false,
  batches: [],
  queuePaused: false,
  cacheHits: 0,
  engineExpanded: false,
  jobStartedAt: 0,
  importStage: "idle",
  boardOrientation: "white",
  diagnostics: null,
  runtimeSettings: null,
};

const root = document.querySelector<HTMLDivElement>("#app");
if (!root) throw new Error("Application root is missing");
const app: HTMLDivElement = root;

app.addEventListener("click", async (event) => {
  const element = event.target instanceof Element ? event.target : null;
  if (element?.closest("[data-hint]")) {
    const updated = await advanceDrillHint(state.activeDrill);
    state.drills = state.drills.map((drill) => drill.id === updated.id ? updated : drill);
    state.shownHint = updated.hint_level;
    render();
  }
  if (element?.closest("[data-retry]")) {
    state.showPunishment = false;
    state.drillStartedAt = Date.now();
    render();
  }
});

app.addEventListener("submit", async (event) => {
  const form = event.target instanceof HTMLFormElement && event.target.matches(".attempt-form")
    ? event.target
    : null;
  if (!form) return;
  event.preventDefault();
  const move = form.querySelector<HTMLInputElement>("#drill-move")?.value.trim().toLowerCase() ?? "";
  try {
    const result = await submitDrillAttempt(state.activeDrill, move, Date.now() - state.drillStartedAt, state.shownHint);
    state.attemptMessage = result.attempt.correct
      ? `Correct. ${result.drill.explanation}`
      : `Not yet. The opponent's strongest reply is ${result.drill.punishment || "forcing"}. Retry the position.`;
    state.showPunishment = !result.attempt.correct;
    state.shownHint = result.drill.hint_level;
    await refreshTraining();
  } catch (error) {
    state.attemptMessage = error instanceof Error ? error.message : "Attempt failed.";
  }
  render();
});

function escapeHtml(value: string): string {
  return value.replace(/[&<>'"]/g, (character) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    "'": "&#39;",
    '"': "&quot;",
  })[character] ?? character);
}

function currentFen(): string {
  if (!state.game?.game.plies.length) return initialFen;
  return state.game.game.plies[state.selectedPly]?.fen_after ?? initialFen;
}

function activeUci(): string {
  if (state.highlightedUci) return state.highlightedUci;
  return state.game?.game.plies[state.selectedPly]?.uci ?? "";
}

function boardMarkup(): string {
  const highlighted = uciSquares(activeUci());
  const squares = squaresFromFen(currentFen(), state.boardOrientation);
  return `<div class="board-wrap">
    <div class="board" role="grid" aria-label="Chess position">
      ${squares.map((square, index) => {
        const light = (Math.floor(index / 8) + index) % 2 === 0;
        const selected = highlighted?.includes(square.name) ?? false;
        const moveClass = highlighted?.[0] === square.name ? "from-square" : highlighted?.[1] === square.name ? "to-square" : "";
        return `<div class="square ${light ? "light" : "dark"} ${selected ? "selected" : ""} ${moveClass}" role="gridcell" data-square="${square.name}">
          ${index % 8 === 0 ? `<span class="rank-coordinate">${square.rank}</span>` : ""}
          ${index >= 56 ? `<span class="file-coordinate">${square.file}</span>` : ""}
          ${pieceMarkup(square.piece, square.name)}
        </div>`;
      }).join("")}
      ${arrowMarkup(highlighted)}
    </div>
  </div>`;
}

function pieceMarkup(piece: string, squareName: string): string {
  if (!piece) return `<span class="piece empty-piece" aria-label="empty ${squareName}"></span>`;
  const pieceTone = blackPieces.has(piece) ? "black-piece" : "white-piece";
  const kind = pieceKinds[piece] ?? "pawn";
  const title = `${kind} on ${squareName}`;
  return `<span class="piece ${pieceTone}" aria-label="${title}">${pieceSvg(kind)}</span>`;
}

function pieceSvg(kind: string): string {
  const common = `viewBox="0 0 64 64" aria-hidden="true" focusable="false"`;
  const base = `<path class="piece-base" d="M18 53h28M22 47h20M25 41h14"/>`;
  const map: Record<string, string> = {
    pawn: `<svg ${common}><circle cx="32" cy="19" r="8"/><path d="M24 42c1.4-10.2 4.1-15 8-15s6.6 4.8 8 15Z"/>${base}</svg>`,
    rook: `<svg ${common}><path d="M20 14h7v6h10v-6h7v14H20Z"/><path d="M23 28h18l-3 14H26Z"/>${base}</svg>`,
    knight: `<svg ${common}><path d="M21 43c3.2-14.6 12.2-25.2 24-29 .7 7.5-1.1 13.5-5.3 18l4.3 10H30l-5 5Z"/><path d="M34 19l-3 4 5 1"/><circle cx="39" cy="22" r="1.6"/>${base}</svg>`,
    bishop: `<svg ${common}><circle cx="32" cy="13" r="5"/><path d="M24 42c1.2-12.8 3.9-22 8-22s6.8 9.2 8 22Z"/><path d="M35 21 28 34"/>${base}</svg>`,
    queen: `<svg ${common}><path d="M17 24 23 13l6 12 3-14 3 14 6-12 6 11-7 18H24Z"/><circle cx="17" cy="23" r="3"/><circle cx="23" cy="13" r="3"/><circle cx="32" cy="11" r="3"/><circle cx="41" cy="13" r="3"/><circle cx="47" cy="23" r="3"/>${base}</svg>`,
    king: `<svg ${common}><path d="M32 9v12M26 15h12"/><path d="M22 42c1.5-11.5 5.4-18 10-18s8.5 6.5 10 18Z"/><path d="M25 24h14"/>${base}</svg>`,
  };
  return map[kind] ?? map.pawn;
}

function arrowMarkup(highlighted: [string, string] | null): string {
  if (!state.highlightedUci || !highlighted) return "";
  const geometry = moveOverlayGeometry(state.highlightedUci, state.boardOrientation);
  if (!geometry) return "";
  const { x: x1, y: y1 } = geometry.source;
  const { x: x2, y: y2 } = geometry.destination;
  return `<svg class="move-arrow" viewBox="0 0 100 100" aria-hidden="true">
    <defs><marker id="arrowhead" markerWidth="6" markerHeight="6" refX="4.5" refY="3" orient="auto"><path d="M0 0 6 3 0 6Z"/></marker></defs>
    <circle class="arrow-origin" cx="${x1}" cy="${y1}" r="3.4"/>
    <line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" marker-end="url(#arrowhead)"/>
    <circle class="arrow-target" cx="${x2}" cy="${y2}" r="5.1"/>
  </svg>`;
}

function statusMarkup(): string {
  const progress = state.job?.progress;
  const complete = state.game?.analysis_status === "complete" || state.job?.status === "complete";
  const shallowDone = complete || progress?.stage === "deep_analysis" || progress?.stage === "complete";
  const parsedDone = Boolean(state.game);
  const status = (done: boolean, active: boolean, text: string) =>
    `<span class="analysis-step ${done ? "done" : ""} ${active ? "active" : ""}"><span class="status-mark">${done ? "✓" : ""}</span>${text}</span>`;
  const deepText = complete
    ? "Deep analysis"
    : progress?.stage === "deep_analysis"
      ? `Deep analysis ${progress.complete} of ${progress.total}`
      : "Deep analysis waiting";
  return `<div class="analysis-steps">
    ${status(parsedDone, !parsedDone, "Parsed")}
    ${status(shallowDone, progress?.stage === "shallow_scan", progress?.stage === "shallow_scan" ? `Shallow scan ${progress.complete} of ${progress.total}` : "Shallow scan")}
    ${status(complete, progress?.stage === "deep_analysis", deepText)}
  </div>
  <div class="progress-track"><span style="width:${progressPercent()}%"></span></div>`;
}

function progressPercent(): number {
  if (state.game?.analysis_status === "complete" || state.job?.status === "complete") return 100;
  if (!state.game) return 0;
  const progress = state.job?.progress;
  if (!progress || progress.total === 0) return 12;
  const fraction = progress.complete / progress.total;
  if (progress.stage === "shallow_scan") return 15 + fraction * 55;
  if (progress.stage === "deep_analysis") return 70 + fraction * 30;
  return 12;
}

function gameSummaryMarkup(): string {
  const tags = state.game?.game.tags ?? {};
  const white = tags.White ?? "No game imported";
  const black = tags.Black ?? "";
  const result = tags.Result ?? "";
  const serviceOffline = state.error === "Local API service is not running.";
  return `<section class="game-summary">
    <div class="players"><h2>${escapeHtml(white)}${black ? `<span> vs. </span>${escapeHtml(black)}` : ""}</h2>${result ? `<strong>${escapeHtml(result)}</strong>` : ""}</div>
    ${statusMarkup()}
    ${state.error && !serviceOffline ? `<p class="inline-error" role="alert">${escapeHtml(state.error)}</p>` : ""}
  </section>`;
}

function moveListMarkup(): string {
  const plies = state.game?.game.plies ?? [];
  const rows: string[] = [];
  for (let index = 0; index < plies.length; index += 2) {
    const white = plies[index];
    const black = plies[index + 1];
    rows.push(`<div class="move-row">
      <span class="move-number">${Math.floor(index / 2) + 1}</span>
      ${white ? `<button data-ply="${index}" class="move ${state.selectedPly === index ? "current" : ""}">${escapeHtml(white.san)}</button>` : "<span></span>"}
      ${black ? `<button data-ply="${index + 1}" class="move ${state.selectedPly === index + 1 ? "current" : ""}">${escapeHtml(black.san)}</button>` : "<span></span>"}
    </div>`);
  }
  return `<div class="move-table" aria-label="Game moves">
    <div class="move-row move-heading"><span>#</span><span>White</span><span>Black</span></div>
    <div class="move-scroll">${rows.join("") || `<p class="empty-copy">Import a game to see its moves.</p>`}</div>
  </div>`;
}

function evaluationMarkup(): string {
  const moves = state.game?.analysis?.moves ?? [];
  const current = moves[state.selectedPly];
  const values = moves.map((move) => Math.max(-600, Math.min(600, move.evaluation_after)));
  const points = values.map((value, index) => {
    const x = values.length <= 1 ? 0 : (index / (values.length - 1)) * 100;
    return `${x},${30 - value / 30}`;
  }).join(" ");
  return `<div class="evaluation">
    <p>Engine evaluation after ${escapeHtml(state.game?.game.plies[state.selectedPly]?.san ?? "selected move")}</p>
    <div class="evaluation-content"><strong>${formatEval(current?.evaluation_after)}</strong>
      <svg viewBox="0 0 100 60" preserveAspectRatio="none" aria-label="Evaluation through the game"><line x1="0" y1="30" x2="100" y2="30"/><polyline points="${points}"/></svg>
    </div>
    <div class="engine-meta"><span>Depth ${current ? state.game?.analysis?.mistakes[0]?.engine.lines[0]?.depth ?? "–" : "–"}</span><span>${current?.phase ?? "waiting"}</span></div>
  </div>`;
}

function engineWorkMarkup(): string {
  const job = state.job;
  const progress = job?.progress;
  const active = job?.status === "queued" || job?.status === "running";
  const failed = job?.status === "failed";
  const complete = state.game?.analysis_status === "complete" || job?.status === "complete";
  const currentMistake = state.game?.analysis?.mistakes[state.expandedMistake];
  const line = currentMistake?.engine.lines[0];
  const diagnostics = state.diagnostics;
  const elapsed = state.jobStartedAt ? Math.max(0, Math.round((Date.now() - state.jobStartedAt) / 1000)) : 0;
  const stage = !state.game ? "Waiting for a game" : failed ? "Engine stopped" : complete ? "Analysis complete" : progress?.stage === "deep_analysis" ? "Deep analysis active" : progress?.stage === "shallow_scan" ? "Shallow scan active" : job?.status === "queued" ? "Queued for engine" : "Preparing positions";
  const stageFacts = [
    state.game ? "PGN reconstructed into legal positions" : "Waiting for a valid PGN",
    progress?.stage === "shallow_scan" || progress?.stage === "deep_analysis" || complete
      ? `Shallow scan ${progress?.stage === "shallow_scan" ? `${progress.complete}/${progress.total}` : "complete"}`
      : "Shallow scan pending",
    progress?.stage === "deep_analysis"
      ? `Deep candidates ${progress.complete}/${progress.total}`
      : complete ? `${state.game?.analysis?.mistakes.length ?? 0} critical moments selected` : "Deep candidates pending",
  ];
  return `<section class="engine-work ${active ? "is-active" : ""} ${failed ? "is-failed" : ""}" aria-labelledby="engine-work-title">
    <button class="engine-work-heading" data-engine-toggle aria-expanded="${state.engineExpanded}">
      <span class="engine-orb" aria-hidden="true"><i></i><i></i><i></i></span>
      <span><small>Stockfish · local</small><strong id="engine-work-title">${escapeHtml(stage)}</strong></span>
      <span class="engine-badge">${active ? "Live" : failed ? "Failed" : complete ? "Ready" : "Idle"}</span>
      ${icons.chevron}
    </button>
    ${state.engineExpanded ? `<div class="engine-work-body">
      <div class="engine-telemetry">
        <div><small>Depth</small><strong>${active ? `target ${state.runtimeSettings?.deep_depth ?? "—"}` : line?.depth ?? "—"}</strong></div>
        <div><small>Nodes</small><strong>${line ? compactNumber(line.nodes) : "—"}</strong></div>
        <div><small>Workers</small><strong>${diagnostics ? `${diagnostics.engine_active}/${diagnostics.engine_workers}` : "—"}</strong></div>
        <div><small>Queue</small><strong>${diagnostics ? diagnostics.queued_interactive + diagnostics.queued_current_game + diagnostics.queued_historical : "—"}</strong></div>
        <div><small>MultiPV</small><strong>${line?.multipv ?? "—"}</strong></div>
        <div><small>Elapsed</small><strong data-engine-elapsed>${elapsed ? `${elapsed}s` : line ? `${Math.round(line.time_ms / 1000)}s` : "—"}</strong></div>
      </div>
      <ol class="stage-log">${stageFacts.map((fact, index) => `<li class="${index === 0 || (index === 1 && (progress?.stage === "shallow_scan" || progress?.stage === "deep_analysis" || complete)) || (index === 2 && (progress?.stage === "deep_analysis" || complete)) ? "complete" : ""}">${escapeHtml(fact)}</li>`).join("")}</ol>
      <div class="latest-line"><small>Latest principal variation</small><code>${escapeHtml(line?.moves.join(" ") ?? "Available when a critical position completes")}</code></div>
      ${failed ? `<div class="engine-actions"><button class="primary-action" data-analysis-retry>Retry analysis</button><a href="/api/diagnostics" target="_blank" rel="noreferrer">View diagnostics</a></div>` : ""}
      <p class="engine-disclosure">Shows observable engine telemetry and stage facts—not hidden reasoning.</p>
    </div>` : ""}
  </section>`;
}

function compactNumber(value: number): string {
  return new Intl.NumberFormat(undefined, { notation: "compact", maximumFractionDigits: 1 }).format(value);
}

function formatEval(value: number | undefined): string {
  if (value === undefined) return "–";
  const pawns = value / 100;
  return `${pawns > 0 ? "+" : ""}${pawns.toFixed(1)}`;
}

function navigationMarkup(): string {
  const plies = state.game?.game.plies ?? [];
  const selected = plies[state.selectedPly];
  return `<div class="move-navigation">
    <button data-nav="first" aria-label="First move" ${!plies.length ? "disabled" : ""}>|‹</button>
    <button data-nav="previous" aria-label="Previous move" ${state.selectedPly <= 0 ? "disabled" : ""}>‹</button>
    <div><strong>${selected ? `${Math.floor(state.selectedPly / 2) + 1}${state.selectedPly % 2 ? "..." : "."} ${escapeHtml(selected.san)}` : "Starting position"}</strong><span>${selected ? (state.selectedPly % 2 ? "White to move" : "Black to move") : "White to move"}</span></div>
    <button data-nav="next" aria-label="Next move" ${state.selectedPly >= plies.length - 1 ? "disabled" : ""}>›</button>
    <button data-nav="last" aria-label="Last move" ${!plies.length ? "disabled" : ""}>›|</button>
    <button data-flip aria-label="Flip board" title="Flip board">↻</button>
  </div>`;
}

function reviewMarkup(): string {
  const mistakes = state.game?.analysis?.mistakes ?? [];
  if (!mistakes.length) {
    const waiting = state.job?.status === "failed" ? state.job.error : "Analysis will add the most consequential moments here.";
    return `<section class="review-pane"><h2>Three moments to review</h2><div class="review-empty">${icons.book}<p>${escapeHtml(waiting)}</p></div></section>`;
  }
  return `<section class="review-pane"><h2>Three moments to review</h2>
    <div class="mistakes">${mistakes.map(mistakeMarkup).join("")}</div>
    <footer class="review-note">${icons.book}<p>Review each moment to learn what went wrong and how to play better next time.</p></footer>
  </section>`;
}

function mistakeMarkup(mistake: Mistake, index: number): string {
  const expanded = state.expandedMistake === index;
  const primary = mistake.engine.lines[0];
  return `<article class="mistake ${expanded ? "expanded" : ""}">
    <button class="mistake-heading" data-mistake="${index}" aria-expanded="${expanded}"><span class="rank">${mistake.rank}</span><span>${escapeHtml(mistake.san)}</span>${icons.chevron}</button>
    ${expanded ? `<div class="mistake-body">
      <h3>${escapeHtml(mistakeTitle(mistake.category))}</h3>
      <div class="lesson-meta"><span>${escapeHtml(mistake.category)}</span><span>${formatEval(mistake.evaluation_before)} to ${formatEval(mistake.evaluation_after)}</span></div>
      <p>${escapeHtml(mistake.explanation)}</p>
      <button class="primary-action" data-better="${escapeHtml(mistake.better_moves[0] ?? "")}">Show better move ${icons.chevron}</button>
      <details><summary>Engine details ${icons.chart}</summary><dl><div><dt>Depth</dt><dd>${primary?.depth ?? "–"}</dd></div><div><dt>Nodes</dt><dd>${primary?.nodes.toLocaleString() ?? "–"}</dd></div><div><dt>Line</dt><dd>${escapeHtml(primary?.moves.join(" ") ?? "Not available")}</dd></div></dl></details>
    </div>` : ""}
  </article>`;
}

function mistakeTitle(category: string): string {
  const lower = category.toLowerCase();
  if (lower.includes("queen")) return "Your queen was left undefended";
  if (lower.includes("piece")) return "Your piece was left undefended";
  if (lower.includes("mate")) return "A forcing mate was available";
  if (lower.includes("capture")) return "A stronger capture was available";
  return "This move allowed a tactical swing";
}

function mobileTabsMarkup(): string {
  return `<nav class="mobile-tabs" role="tablist" aria-label="Review panels">
    ${(["review", "moves", "engine"] as const).map((view) => `<button role="tab" aria-selected="${state.mobileView === view}" aria-controls="${view}-panel" data-view="${view}" class="${state.mobileView === view ? "active" : ""}"><span>${view[0]?.toUpperCase()}${view.slice(1)}</span></button>`).join("")}
  </nav>`;
}

function importLoadingMarkup(): string {
  if (!state.busy) return "";
  const stages = [
    ["link", "Reading link"],
    ["resolving", "Finding public archive"],
    ["reconstructing", "Reconstructing positions"],
    ["analyzing", "Starting Stockfish"],
  ] as const;
  const activeIndex = Math.max(0, stages.findIndex(([key]) => key === state.importStage));
  return `<section class="import-loading" aria-live="polite"><div class="loading-wave" aria-hidden="true">${Array.from({ length: 7 }, () => "<i></i>").join("")}</div><div><small>Preparing your review</small><strong>${stages[activeIndex]?.[1] ?? "Reading game"}</strong></div><ol>${stages.map(([key, label], index) => `<li class="${index < activeIndex ? "done" : index === activeIndex ? "active" : ""}" data-stage="${key}">${label}</li>`).join("")}</ol></section>`;
}

function render(): void {
  if (state.mode === "explore") {
    app.innerHTML = exploreShellMarkup();
    bindTrainingEvents();
    return;
  }
  if (state.mode !== "game") {
    app.innerHTML = trainingShellMarkup();
    bindTrainingEvents();
    return;
  }
  const serviceOffline = state.error === "Local API service is not running.";
  app.innerHTML = `<div class="app-shell">
    <header class="app-header"><a href="/" class="brand"><span class="brand-mark" aria-hidden="true">${pieceSvg("knight")}</span><span>Personal Chess Tutor<small>Local analysis studio</small></span></a><nav><button data-mode="game" class="active" aria-current="page"><span>Review</span></button><button data-mode="explore"><span>Explore</span></button><button data-mode="progress"><span>Progress</span></button></nav><button class="menu-button" aria-label="Menu">${icons.menu}</button></header>
    <main class="workspace ${state.mobileView}">
      <button class="import-bar" id="open-import">${icons.upload}<span><strong>${state.busy ? "Resolving game" : "Import a game"}</strong><small>Chess.com link or PGN</small></span>${icons.chevron}</button>
      ${importLoadingMarkup()}
      <div class="summary-region">${gameSummaryMarkup()}</div>
      <section class="board-region"><div class="board-stage">${boardMarkup()}</div>${navigationMarkup()}${mobileTabsMarkup()}</section>
      <aside id="moves-panel" role="tabpanel" class="analysis-region panel-slot">${moveListMarkup()}${evaluationMarkup()}</aside>
      <div id="review-panel" role="tabpanel" class="review-region panel-slot">${reviewMarkup()}</div>
      <div id="engine-panel" role="tabpanel" class="engine-region panel-slot">${engineWorkMarkup()}</div>
    </main>
    <footer class="local-status"><span class="privacy-note">Private by design · games never leave this computer</span><span class="engine-state ${serviceOffline || state.job?.status === "failed" ? "offline" : ""}"><i></i>${serviceOffline ? "Start local API service" : state.job?.status === "failed" ? "Engine unavailable" : "Stockfish ready"}</span></footer>
  </div>
  ${importDialogMarkup()}`;
  bindEvents();
}

function exploreShellMarkup(): string {
  return `<div class="learning-shell explore"><header class="app-header"><a href="/" class="brand"><span class="brand-mark">♞</span><span>Personal Chess Tutor<small>Local analysis studio</small></span></a><nav><button data-mode="game"><span>Review</span></button><button data-mode="explore" class="active"><span>Explore</span></button><button data-mode="progress"><span>Progress</span></button></nav></header><main class="explore-main"><p class="overline">Phase 3 preview</p><h1>A quiet library for positions worth remembering.</h1><p>Openings, middlegames, and endgames will connect concepts directly to moments from your own games. The old daily-training dashboard is intentionally out of the primary workflow.</p><div class="explore-paths"><article><span>01</span><h2>Openings</h2><p>Plans, move trees, and departures seen in your games.</p></article><article><span>02</span><h2>Middlegames</h2><p>Structures, tactical motifs, and recurring decisions.</p></article><article><span>03</span><h2>Endgames</h2><p>Essential techniques linked to exact positions.</p></article></div><button data-mode="game" class="primary-action">Return to review</button></main></div>`;
}

function trainingBoardMarkup(drill: Drill | undefined): string {
  if (!drill) return `<div class="training-empty">Analyze a game to create your first exact-position drill.</div>`;
  const solution = drill.solutions[0] ?? "";
  const highlighted = state.shownHint >= 1 && solution.length >= 2 ? [solution.slice(0, 2)] : null;
  const fen = state.showPunishment && drill.fen_after_punishment ? drill.fen_after_punishment : drill.fen;
  return `<div class="training-board board" role="grid" aria-label="Drill position">${squaresFromFen(fen).map((square, index) => {
    const light = (Math.floor(index / 8) + index) % 2 === 0;
    const selected = highlighted?.includes(square.name) ?? false;
    return `<div class="square ${light ? "light" : "dark"} ${selected ? "selected from-square" : ""}" role="gridcell">${pieceMarkup(square.piece, square.name)}</div>`;
  }).join("")}</div>`;
}

function metric(value: string, label: string, detail: string): string {
  return `<article class="metric"><strong>${value}</strong><span>${label}</span><small>${detail}</small></article>`;
}

function rateMetric(value: Profile["endgame_conversion"], label: string): string {
  const rate = value.rate === null ? "More data needed" : `${Math.round(value.rate * 100)}%`;
  return `<div><strong>${escapeHtml(label)}: ${rate}</strong><small>${value.numerator} of ${value.denominator}; rates appear after 5 eligible games</small></div>`;
}

function trendMarkup(profile: Profile | null): string {
  const points = profile?.activity_trend ?? [];
  const maximum = Math.max(1, ...points.flatMap((point) => [point.games_analyzed, point.mistakes, point.drill_attempts]));
  const bars = points.map((point, index) => {
    const date = new Date(point.day_start_ms);
    const label = date.toLocaleDateString(undefined, { month: "short", day: "numeric" });
    const height = (value: number) => value === 0 ? 0 : Math.max(8, Math.round(value / maximum * 100));
    const showLabel = index === 0 || index === points.length - 1 || index === 6;
    return `<div class="trend-day" title="${escapeHtml(`${label}: ${point.games_analyzed} games, ${point.mistakes} mistakes, ${point.drill_attempts} attempts`)}"><div class="trend-bars"><i class="games" style="height:${height(point.games_analyzed)}%"></i><i class="mistakes" style="height:${height(point.mistakes)}%"></i><i class="attempts" style="height:${height(point.drill_attempts)}%"></i></div><small>${showLabel ? escapeHtml(label) : ""}</small></div>`;
  }).join("");
  return `<section class="trend-panel"><div class="panel-title"><div><p class="overline">Last 14 calendar days</p><h2>Practice activity</h2></div><div class="trend-legend"><span class="games">Games</span><span class="mistakes">Mistakes</span><span class="attempts">Drills</span></div></div><div class="trend-chart" role="img" aria-label="Daily analyzed games, detected mistakes, and drill attempts">${bars}</div></section>`;
}

function trainingShellMarkup(): string {
  const drill = state.drills.find((candidate) => candidate.id === state.activeDrill) ?? state.drills[0];
  const profile = state.profile;
  const due = state.drills.filter((candidate) => candidate.schedule.state === "due" || candidate.schedule.state === "new").length;
  const hint = !drill ? "" : state.shownHint === 0 ? "No hint yet. First identify what changed and calculate forcing moves." : state.shownHint === 1 ? "The relevant piece's starting square is highlighted." : state.shownHint === 2 ? `Candidate moves: ${drill.solutions.join(", ")}` : `Solution: ${drill.solutions[0]}. ${drill.explanation}`;
  const hintAvailable = Boolean(drill && state.shownHint < drill.available_hint_level);
  return `<div class="learning-shell ${state.mode}">
    <header class="app-header"><a href="/" class="brand"><span class="brand-mark">♞</span><span>Personal Chess Tutor<small>Local analysis studio</small></span></a><nav><button data-mode="game"><span>Review</span></button><button data-mode="explore"><span>Explore</span></button><button data-mode="progress" class="${state.mode === "progress" ? "active" : ""}"><span>Progress</span></button></nav></header>
    <main class="learning-main">
      <header class="learning-heading"><div><p class="overline">Personalized training · ${due} ready today</p><h1>${state.mode === "training" ? "Turn mistakes into stronger habits." : "Progress you can trace back to games."}</h1></div><p>Every count comes from your local event log. No composite score.</p></header>
      <section class="metrics">${metric(String(profile?.games_analyzed ?? 0), "games deep analyzed", `${profile?.games_shallow_analyzed ?? 0} shallow ready · ${profile?.games_imported ?? 0} imported`)}${metric(`${Math.round((profile?.drill_accuracy ?? 0) * 100)}%`, "drill accuracy", `${profile?.drill_correct ?? 0} of ${profile?.drill_attempts ?? 0} · ${Math.round((profile?.retention_rate ?? 0) * 100)}% retained (${profile?.retained_reviews ?? 0}/${profile?.retention_reviews ?? 0})`)}${metric((profile?.average_centipawn_loss ?? 0).toFixed(0), "average CP loss", `${profile?.total_positions ?? 0} positions`)}${metric(String(due), "reviews ready", `${state.drills.length} drills total`)}</section>
      <div class="learning-grid">
        <section class="training-panel"><div class="panel-title"><p class="overline">Daily review</p><h2>${drill ? escapeHtml(drill.category) : "No drills yet"}</h2></div>${trainingBoardMarkup(drill)}
          ${drill ? `<div class="coach-sequence"><ol><li>What did the previous move change?</li><li>Which piece or square is under threat?</li><li>Choose the strongest response.</li></ol>${state.showPunishment ? `<div class="lesson-replay"><strong>Your game: ${escapeHtml(drill.played_move)} → ${escapeHtml(drill.opponent_response)}</strong><p>${escapeHtml(drill.changed_threat)}</p><p>${drill.attacked_pieces.length ? `Attacked pieces: ${escapeHtml(drill.attacked_pieces.join(", "))}.` : "No direct loose piece was detected; calculate checks and forcing threats."}</p><p>The board shows the opponent's strongest reply. Retry from the exact source position.</p><button class="primary-action" data-retry>Retry exact position</button></div>` : `<p class="hint-copy">${escapeHtml(hint)}</p>${hintAvailable ? `<button class="secondary-action" data-hint>${state.shownHint === 0 ? "Reveal earned hint" : "Reveal next earned hint"}</button>` : `<small class="hint-locked">${state.shownHint >= 3 ? "Solution revealed." : "A failed attempt unlocks the next hint."}</small>`}<form class="attempt-form"><label>Your move in UCI <input id="drill-move" required pattern="[a-h][1-8][a-h][1-8][qrbn]?" placeholder="e2e4"></label><button class="primary-action">Try move</button></form>`}<p class="attempt-message" role="status">${escapeHtml(state.attemptMessage)}</p></div>` : ""}
        </section>
        <aside class="queue-panel"><div class="panel-title"><p class="overline">Queue</p><h2>Review schedule</h2></div><div class="drill-list">${state.drills.map((item) => `<button data-drill="${escapeHtml(item.id)}" class="${item.id === drill?.id ? "active" : ""}"><span>${escapeHtml(item.category)}</span><small>${item.schedule.state} · ${Math.round(item.schedule.success_rate * 100)}%${item.source_type === "public_corpus" ? " · validated corpus" : " · your game"}</small></button>`).join("") || "<p>No generated drills.</p>"}</div><button class="secondary-action" data-supplemental>Add validated public puzzles</button><small>Uses recurring motifs only after your own positions; each solution is checked twice locally.</small></aside>
        ${trendMarkup(profile)}
        <section class="weakness-panel"><div class="panel-title"><p class="overline">Evidence · ${profile?.games_analyzed_7_days ?? 0} games this week / ${profile?.games_analyzed_30_days ?? 0} this month</p><h2>Recurring weaknesses</h2></div>${profile?.weaknesses.map((weakness) => `<div class="weakness-row"><div><strong>${escapeHtml(weakness.category)}</strong><small>${weakness.occurrences} total in ${weakness.games} games · ${Math.round(weakness.recurrence_rate * 100)}% recurrence · ${weakness.average_loss_cp.toFixed(0)} average CP loss · phases: ${escapeHtml(Object.entries(weakness.phases).map(([phase, count]) => `${phase} ${count}`).join(", ") || "none")} · ${weakness.repeated_interval_days === null ? "interval needs 2 games" : `${weakness.repeated_interval_days.toFixed(1)} days between repeats`} · ${weakness.occurrences_7_days} last 7 days / ${weakness.occurrences_30_days} last 30</small></div><span>${Math.round(weakness.drill_accuracy * 100)}%</span></div>`).join("") || "<p>Analyze multiple games to reveal recurring patterns.</p>"}<div class="opening-summary"><p class="overline">Personal repertoire · ${escapeHtml(profile?.player_name || "player not inferred")}${profile?.latest_rating ? ` · latest rating ${profile.latest_rating}` : ""}</p>${profile?.openings.map((opening) => `<div><strong>${escapeHtml(opening.eco)} · ${escapeHtml(opening.name)}</strong><small>${opening.games} games · ${opening.mistakes} major mistakes · ${opening.average_centipawn_loss.toFixed(0)} average CP loss</small></div>`).join("") || "<small>No recognized opening yet.</small>"}${profile ? rateMetric(profile.endgame_conversion, "Endgame conversion") + rateMetric(profile.king_safety_violations, "King-safety violation rate") + rateMetric(profile.time_management_failures, "Time-management failure rate") : ""}</div></section>
        <section class="resource-panel"><div class="panel-title"><p class="overline">Recommended next</p><h2>Resources with reasons</h2></div>${state.resources.map((resource) => `<article class="resource"><div><strong>${escapeHtml(resource.title)}</strong><p>${escapeHtml(resource.evidence)}</p><small>${escapeHtml(resource.kind)} · ${escapeHtml(resource.phase)}</small></div><button data-resource="${escapeHtml(resource.id)}" ${resource.completed ? "disabled" : ""}>${resource.completed ? "Completed" : "Mark studied"}</button></article>`).join("") || "<p>Recommendations appear after analyzed mistakes.</p>"}</section>
        <section class="batch-panel"><div class="panel-title"><div><p class="overline">History · ${state.cacheHits} position cache hits</p><h2>Batch import recent games</h2></div><button class="queue-toggle" data-queue-action="${state.queuePaused ? "resume" : "pause"}">${state.queuePaused ? "Resume queue" : "Pause queue"}</button></div>${state.batches.map((batch) => `<div class="batch-progress"><strong>${escapeHtml(batch.id)}</strong><span>${batch.completed} complete · ${batch.remaining} games / ${batch.positions_remaining} positions remaining · ${batch.positions_analyzed} positions analyzed · ${batch.duplicates} duplicates · ${batch.failed + batch.job_failures} failed</span><progress max="${Math.max(1, batch.discovered)}" value="${batch.completed + batch.duplicates + batch.failed + batch.job_failures}"></progress></div>`).join("")}<p>Paste Chess.com game URLs one per line, or complete PGNs separated by a line containing <code>---</code>. Duplicate identities are skipped.</p><textarea id="batch-urls" placeholder="https://www.chess.com/game/live/…&#10;https://www.chess.com/game/live/…"></textarea><textarea id="batch-pgns" placeholder="[Event &quot;Game one&quot;]…&#10;---&#10;[Event &quot;Game two&quot;]…"></textarea><button class="primary-action" data-batch>Import recent games</button><p role="status">${escapeHtml(state.batchMessage)}</p></section>
      </div>
    </main>
    <footer class="local-status"><span>Scheduler: pct-sm2-1 · Profile: profile-1</span><span>Rebuilt from immutable local events</span></footer>
  </div>`;
}

async function refreshTraining(): Promise<void> {
  const [drills, profile, resources, batches] = await Promise.all([loadDrills(), loadProfile(), loadResources(), loadBatches()]);
  state.drills = drills;
  state.profile = profile;
  state.resources = resources;
  state.batches = batches.batches;
  state.queuePaused = batches.paused;
  state.cacheHits = batches.cache_hits;
  if (!state.activeDrill && drills[0]) state.activeDrill = drills[0].id;
  state.shownHint = drills.find((drill) => drill.id === state.activeDrill)?.hint_level ?? 0;
}

async function activateDrillSession(): Promise<void> {
  if (!state.activeDrill) return;
  const updated = await beginDrillSession(state.activeDrill);
  state.drills = state.drills.map((drill) => drill.id === updated.id ? updated : drill);
  state.shownHint = updated.hint_level;
  state.drillStartedAt = Date.now();
  render();
}

function bindTrainingEvents(): void {
  document.querySelectorAll<HTMLButtonElement>("[data-mode]").forEach((button) => button.addEventListener("click", () => { state.mode = button.dataset.mode as AppMode; render(); if (state.mode === "training") void activateDrillSession(); }));
  document.querySelectorAll<HTMLButtonElement>("[data-drill]").forEach((button) => button.addEventListener("click", () => { state.activeDrill = button.dataset.drill ?? ""; state.showPunishment = false; state.attemptMessage = ""; render(); void activateDrillSession(); }));
  document.querySelectorAll<HTMLButtonElement>("[data-resource]").forEach((button) => button.addEventListener("click", async () => { await completeResource(button.dataset.resource ?? ""); await refreshTraining(); render(); }));
  document.querySelector<HTMLButtonElement>("[data-supplemental]")?.addEventListener("click", async () => {
    try {
      const result = await generateSupplementalDrills();
      state.attemptMessage = result.added > 0 ? `${result.added} independently validated puzzles added.` : "No stable corpus puzzle matched a recurring weakness yet.";
      await refreshTraining();
    } catch (error) {
      state.attemptMessage = error instanceof Error ? error.message : "Supplemental drill generation failed.";
    }
    render();
  });
  document.querySelector<HTMLButtonElement>("[data-queue-action]")?.addEventListener("click", async (event) => {
    const paused = (event.currentTarget as HTMLButtonElement).dataset.queueAction === "pause";
    await setQueuePaused(paused);
    await refreshTraining();
    render();
  });
  document.querySelector<HTMLButtonElement>("[data-batch]")?.addEventListener("click", async () => {
    const raw = document.querySelector<HTMLTextAreaElement>("#batch-pgns")?.value ?? "";
    const pgns = raw.split(/^---$/m).map((value) => value.trim()).filter(Boolean);
    const urls = (document.querySelector<HTMLTextAreaElement>("#batch-urls")?.value ?? "").split(/\s+/).map((value) => value.trim()).filter(Boolean);
    try { const result = await importBatch(pgns, urls); state.batchMessage = `${result.imported} imported, ${result.duplicates} duplicates, ${result.queued} queued, ${result.failed} failed.`; await refreshTraining(); }
    catch (error) { state.batchMessage = error instanceof Error ? error.message : "Batch import failed."; }
    render();
  });
}

function importDialogMarkup(): string {
  return `<dialog id="import-dialog"><form method="dialog" class="dialog-shell"><header><div><p class="overline">New review</p><h2>Bring in a game.</h2><p>Paste a public Chess.com link. We’ll identify the players, find the exact archive PGN, and start local analysis.</p></div><button value="cancel" aria-label="Close">×</button></header>
    <label class="link-input"><span>Chess.com game link</span><input id="game-url" type="url" placeholder="https://www.chess.com/game/live/171626462440" autocomplete="url"></label>
    <details class="pgn-fallback"><summary>Use PGN instead</summary><label><span class="sr-only">PGN</span><textarea id="game-pgn" placeholder="[Event &quot;…&quot;]&#10;&#10;1. e4 e5 …"></textarea></label></details>
    <p class="dialog-error" role="alert">${escapeHtml(state.error)}</p>
    <footer><span>Public data only · no Chess.com password</span><button value="default" id="submit-import" class="primary-action">Open review ${icons.chevron}</button></footer>
  </form></dialog>`;
}

function bindEvents(): void {
  document.querySelectorAll<HTMLButtonElement>("[data-mode]").forEach((button) => button.addEventListener("click", () => {
    state.mode = button.dataset.mode as AppMode;
    render();
    if (state.mode === "training") void activateDrillSession();
  }));
  const importBar = document.querySelector<HTMLButtonElement>("#open-import");
  importBar?.addEventListener("click", () => {
    document.querySelector<HTMLDialogElement>("#import-dialog")?.showModal();
  });
  importBar?.addEventListener("dragover", (event) => {
    event.preventDefault();
    importBar.classList.add("dragging");
  });
  importBar?.addEventListener("dragleave", () => importBar.classList.remove("dragging"));
  importBar?.addEventListener("drop", (event) => {
    event.preventDefault();
    importBar.classList.remove("dragging");
    const file = event.dataTransfer?.files[0];
    if (!file || !file.name.toLowerCase().endsWith(".pgn")) {
      state.error = "Drop a .pgn file to import it.";
      render();
      return;
    }
    void file.text().then((pgn) => runImport({ pgn }));
  });
  document.querySelector<HTMLButtonElement>("#submit-import")?.addEventListener("click", (event) => {
    event.preventDefault();
    void submitImport();
  });
  document.querySelectorAll<HTMLButtonElement>("[data-ply]").forEach((button) => {
    button.addEventListener("click", () => selectPly(Number(button.dataset.ply)));
  });
  document.querySelectorAll<HTMLButtonElement>("[data-nav]").forEach((button) => {
    button.addEventListener("click", () => navigate(button.dataset.nav ?? ""));
  });
  document.querySelector<HTMLButtonElement>("[data-flip]")?.addEventListener("click", () => {
    state.boardOrientation = state.boardOrientation === "white" ? "black" : "white";
    render();
  });
  document.querySelectorAll<HTMLButtonElement>("[data-mistake]").forEach((button) => {
    button.addEventListener("click", () => {
      const index = Number(button.dataset.mistake);
      state.expandedMistake = state.expandedMistake === index ? -1 : index;
      state.highlightedUci = "";
      const mistake = state.game?.analysis?.mistakes[index];
      if (mistake) state.selectedPly = mistake.ply;
      render();
    });
  });
  document.querySelectorAll<HTMLButtonElement>("[data-better]").forEach((button) => {
    button.addEventListener("click", () => {
      state.highlightedUci = button.dataset.better ?? "";
      render();
    });
  });
  document.querySelectorAll<HTMLButtonElement>("[data-view]").forEach((button) => {
    button.addEventListener("click", () => {
      state.mobileView = (button.dataset.view as MobileView) ?? "game";
      render();
    });
  });
  document.querySelector<HTMLButtonElement>("[data-engine-toggle]")?.addEventListener("click", () => {
    state.engineExpanded = !state.engineExpanded;
    if (state.engineExpanded) void refreshEngineFacts();
    render();
  });
  document.querySelector<HTMLButtonElement>("[data-analysis-retry]")?.addEventListener("click", async () => {
    if (!state.game) return;
    try {
      state.jobStartedAt = Date.now();
      state.job = await startAnalysis(state.game.game.id);
      state.error = "";
    } catch (error) {
      state.error = error instanceof Error ? error.message : "Could not restart analysis.";
    }
    render();
  });
}

function selectPly(ply: number): void {
  state.selectedPly = ply;
  state.highlightedUci = "";
  render();
}

function navigate(action: string): void {
  const last = Math.max(0, (state.game?.game.plies.length ?? 1) - 1);
  if (action === "first") state.selectedPly = 0;
  if (action === "previous") state.selectedPly = Math.max(0, state.selectedPly - 1);
  if (action === "next") state.selectedPly = Math.min(last, state.selectedPly + 1);
  if (action === "last") state.selectedPly = last;
  state.highlightedUci = "";
  render();
}

async function submitImport(): Promise<void> {
  const url = document.querySelector<HTMLInputElement>("#game-url")?.value.trim() ?? "";
  const pgn = document.querySelector<HTMLTextAreaElement>("#game-pgn")?.value.trim() ?? "";
  if (!url && !pgn) {
    state.error = "Paste a Chess.com game URL or PGN.";
    render();
    document.querySelector<HTMLDialogElement>("#import-dialog")?.showModal();
    return;
  }
  await runImport(url ? { url } : { pgn });
}

async function runImport(input: { url: string } | { pgn: string }): Promise<void> {
  const startedAt = Date.now();
  state.busy = true;
  state.error = "";
  state.importStage = "link";
  render();
  try {
    const result = await importGameObservable(input);
    let gameId: string;
    if (result.status === "resolving") {
      state.importStage = "resolving";
      render();
      let resolution = result.resolution;
      while (resolution.status === "queued" || resolution.status === "running") {
        await delay(250);
        resolution = await loadImportResolution(result.resolution_id);
      }
      if (resolution.status !== "resolved" || !resolution.imported_game_id) {
        throw new Error(resolution.error || "Chess.com import could not be resolved.");
      }
      gameId = resolution.imported_game_id;
      state.importStage = "reconstructing";
      render();
      state.game = await loadGame(gameId);
      state.importStage = "analyzing";
      state.jobStartedAt = Date.now();
      state.job = await startAnalysis(gameId);
    } else {
      gameId = result.game_id;
      state.importStage = "reconstructing";
      render();
      state.job = result.job;
      state.jobStartedAt = Date.now();
      state.game = await loadGame(gameId);
      state.importStage = "analyzing";
    }
    state.selectedPly = Math.max(0, state.game.game.plies.length - 1);
    state.expandedMistake = 0;
    await refreshEngineFacts();
  } catch (error) {
    state.error = error instanceof Error ? error.message : "Import failed.";
  } finally {
    const remainingDwell = 320 - (Date.now() - startedAt);
    if (remainingDwell > 0) await delay(remainingDwell);
    state.busy = false;
    state.importStage = "idle";
    render();
    if (state.error) document.querySelector<HTMLDialogElement>("#import-dialog")?.showModal();
  }
}

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

async function refreshEngineFacts(): Promise<void> {
  try {
    [state.diagnostics, state.runtimeSettings] = await Promise.all([loadDiagnostics(), loadRuntimeSettings()]);
    if (state.mode === "game") render();
  } catch {
    // Job progress remains useful if optional diagnostics are unavailable.
  }
}

async function refreshGame(): Promise<void> {
  if (!state.game) return;
  try {
    state.game = await loadGame(state.game.game.id);
    state.error = "";
    render();
  } catch (error) {
    state.error = error instanceof Error ? error.message : "Could not refresh the game.";
    render();
  }
}

function connectProgress(): void {
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  const socket = new WebSocket(`${protocol}//${window.location.host}/ws`);
  socket.addEventListener("message", (event) => {
    const message = JSON.parse(String(event.data)) as ProgressSocketMessage;
    if (message.type !== "job_update" && message.type !== "jobs_snapshot") return;
    if (!state.game) return;
    const job = message.type === "job_update"
      ? message.job
      : message.jobs.find((candidate) => !state.game || candidate.game_id === state.game.game.id);
    if (!job || (state.game && job.game_id !== state.game.game.id)) return;
    state.job = job;
    if ((job.status === "queued" || job.status === "running") && !state.jobStartedAt) state.jobStartedAt = Date.now();
    if (job.status === "running") void refreshEngineFacts();
    if (job.status === "complete") {
      void refreshGame();
      void refreshTraining();
    }
    else render();
  });
  socket.addEventListener("close", () => window.setTimeout(connectProgress, 1500));
}

async function start(): Promise<void> {
  render();
  connectProgress();
  window.setInterval(() => {
    const elapsed = document.querySelector<HTMLElement>("[data-engine-elapsed]");
    if (elapsed && state.jobStartedAt && (state.job?.status === "queued" || state.job?.status === "running")) {
      elapsed.textContent = `${Math.max(0, Math.round((Date.now() - state.jobStartedAt) / 1000))}s`;
    }
  }, 1000);
  try {
    const games = await listGames();
    if (games[0]) {
      state.game = await loadGame(games[0].game.id);
      state.selectedPly = Math.max(0, state.game.game.plies.length - 1);
    }
  } catch (error) {
    state.error = error instanceof Error ? error.message : "Local service is unavailable.";
  }
  try {
    await refreshTraining();
  } catch {
    // Review remains available if optional learning projections are unavailable.
  }
  render();
}

void start();
