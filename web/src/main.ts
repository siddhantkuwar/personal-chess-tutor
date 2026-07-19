import "./styles.css";
import "./design/tokens.css";
import { advanceDrillHint, beginDrillSession, completeResource, generateSupplementalDrills, importBatch, importGameObservable, listGames, loadBatches, loadDiagnostics, loadDrills, loadGame, loadImportResolution, loadProfile, loadResources, loadRuntimeSettings, setQueuePaused, startAnalysis, submitDrillAttempt } from "./api";
import { applyUciLineToFen, moveOverlayGeometry, squaresFromFen, uciSquares } from "./chess";
import type { BoardOrientation } from "./chess";
import { icons } from "./icons";
import { acceptedSquareMove, autoplayDelay, blockingClassifications, classificationCounts, completePlaybackDwell, firstReviewPly, isAcceptedTry, isPlaying, pauseForSelectedMove, startPlayback, type ReviewMode } from "./review";
import { buildExploreEntries, inferPlayerName, ratingDelta, ratingHistory, reviewArc, type ExploreSection } from "./insights";
import type { BatchProgress, Diagnostics, Drill, Job, MoveAssessment, Profile, ProgressSocketMessage, ResourceRecommendation, RuntimeSettings, StoredGame } from "./types";

type MobileView = "review" | "moves" | "engine";
type AppMode = "game" | "training" | "explore" | "progress";
type InspectorTab = "line" | "graph" | "summary" | "method";

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
  reviewMode: ReviewMode;
  tryMessage: string;
  variationCursor: number;
  games: StoredGame[];
  inspectorTab: InspectorTab;
  exploreSection: ExploreSection;
  selectedExploreId: string;
  trySourceSquare: string;
  ledgerScrollTop: number;
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
  reviewMode: "manual",
  tryMessage: "",
  variationCursor: 0,
  games: [],
  inspectorTab: "line",
  exploreSection: "Openings",
  selectedExploreId: "",
  trySourceSquare: "",
  ledgerScrollTop: 0,
};
let autoplayTimer = 0;

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

document.addEventListener("visibilitychange", () => {
  if (document.hidden && isPlaying(state.reviewMode)) {
    state.reviewMode = "manual";
    window.clearTimeout(autoplayTimer);
  }
});

document.addEventListener("keydown", (event) => {
  if (event.target instanceof HTMLInputElement || event.target instanceof HTMLTextAreaElement) return;
  if (state.mode !== "game" || !state.game) return;
  if (event.key === "ArrowLeft") { event.preventDefault(); navigate("previous"); }
  if (event.key === "ArrowRight") { event.preventDefault(); navigate("next"); }
  if (event.key === "Home") { event.preventDefault(); navigate("first"); }
  if (event.key === "End") { event.preventDefault(); navigate("last"); }
  if (event.key === " ") { event.preventDefault(); reviewAction(isPlaying(state.reviewMode) ? "pause" : "play"); }
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
  if (state.reviewMode === "try_move" || state.reviewMode === "revealed_move") {
    return state.game.analysis?.moves[state.selectedPly]?.fen_before ?? state.game.game.plies[state.selectedPly]?.fen_before ?? initialFen;
  }
  if (state.reviewMode === "variation") {
    const move = state.game.analysis?.moves[state.selectedPly];
    if (move) return applyUciLineToFen(move.fen_before, move.principal_variation, state.variationCursor);
  }
  return state.game.game.plies[state.selectedPly]?.fen_after ?? initialFen;
}

function activeUci(): string {
  if (state.reviewMode === "variation") {
    const pv = state.game?.analysis?.moves[state.selectedPly]?.principal_variation ?? [];
    return pv[Math.max(0, state.variationCursor - 1)] ?? "";
  }
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
        const selected = (highlighted?.includes(square.name) ?? false) || state.trySourceSquare === square.name;
        const moveClass = highlighted?.[0] === square.name ? "from-square" : highlighted?.[1] === square.name ? "to-square" : "";
        const tryClass = state.trySourceSquare === square.name ? "try-source" : "";
        return `<div class="square ${light ? "light" : "dark"} ${selected ? "selected" : ""} ${moveClass} ${tryClass}" role="gridcell" data-square="${square.name}" ${state.reviewMode === "try_move" ? `tabindex="0" aria-label="Choose ${square.name}"` : ""}>
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
  const side = pieceTone === "black-piece" ? "black" : "white";
  return `<span class="piece ${pieceTone}" aria-label="${title}"><img src="/pieces/lasker/${side}_${kind}.svg" alt="" draggable="false"></span>`;
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
  if (state.reviewMode !== "revealed_move" || !state.highlightedUci || !highlighted) return "";
  const geometry = moveOverlayGeometry(state.highlightedUci, state.boardOrientation);
  if (!geometry) return "";
  const { x: x1, y: y1 } = geometry.source;
  const { x: x2, y: y2 } = geometry.destination;
  const distance = Math.hypot(x2 - x1, y2 - y1) || 1;
  const unitX = (x2 - x1) / distance;
  const unitY = (y2 - y1) / distance;
  const startX = x1 + unitX * 2.4;
  const startY = y1 + unitY * 2.4;
  const endX = x2 - unitX * 3.4;
  const endY = y2 - unitY * 3.4;
  return `<svg class="move-arrow" viewBox="0 0 100 100" aria-hidden="true">
    <defs><marker id="arrowhead" markerUnits="userSpaceOnUse" markerWidth="4" markerHeight="4" viewBox="0 0 4 4" refX="3.5" refY="2" orient="auto"><path d="M0 0 4 2 0 4Z"/></marker></defs>
    <line x1="${startX}" y1="${startY}" x2="${endX}" y2="${endY}" marker-end="url(#arrowhead)"/>
  </svg>`;
}

function statusMarkup(): string {
  const progress = state.job?.progress;
  const complete = state.game?.analysis_status === "complete" || state.job?.status === "complete";
  const parsedDone = Boolean(state.game);
  const shallowDone = complete || progress?.stage === "deep_analysis" || progress?.stage === "complete";
  const activeStage = !parsedDone ? 0 : !shallowDone ? 1 : 2;
  const headline = complete
    ? "Review ready"
    : progress?.stage === "deep_analysis"
      ? "Reading the critical positions"
      : progress?.stage === "shallow_scan"
        ? "Mapping the game"
        : "Reconstructing the position";
  const detail = complete
    ? `${state.game?.analysis?.moves.length ?? 0} plies classified · local Stockfish`
    : progress?.total
      ? `${progress.complete} of ${progress.total} positions · ${Math.round(progressPercent())}%`
      : "Preparing local engine evidence";
  const steps = [
    { label: "Parsed", done: parsedDone },
    { label: "Shallow scan", done: shallowDone },
    { label: "Deep analysis", done: complete },
  ];
  return `<section class="analysis-status ${complete ? "is-complete" : "is-running"}" aria-live="polite">
    <div class="analysis-kinetic" aria-hidden="true"><span></span><span></span><span></span><i>${complete ? "✓" : ""}</i></div>
    <div class="analysis-copy"><small>${complete ? "Analysis complete" : "Stockfish is working locally"}</small><strong>${headline}</strong><span>${detail}</span></div>
    <div class="analysis-sequence" aria-label="Analysis stages">${steps.map((step, index) => `<div class="${step.done ? "done" : index === activeStage ? "active" : ""}"><i>${step.done ? "✓" : index + 1}</i><span>${step.label}</span></div>`).join("")}</div>
    <div class="analysis-beam" aria-hidden="true"><svg viewBox="0 0 100 3" preserveAspectRatio="none"><rect class="beam-track" width="100" height="3"/><rect class="beam-fill" width="${progressPercent()}" height="3"/></svg></div>
  </section>`;
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

function moveClassification(index: number): MoveAssessment | undefined {
  return state.game?.analysis?.moves[index];
}

function classificationClass(value: string): string {
  return value.toLowerCase().replace(/[^a-z]+/g, "-");
}

function moveListMarkup(): string {
  const plies = state.game?.game.plies ?? [];
  const rows: string[] = [];
  for (let index = 0; index < plies.length; index += 2) {
    const white = plies[index];
    const black = plies[index + 1];
    rows.push(`<div class="move-row">
      <span class="move-number">${Math.floor(index / 2) + 1}</span>
      ${white ? scoreMoveMarkup(index, white.san) : "<span></span>"}
      ${black ? scoreMoveMarkup(index + 1, black.san) : "<span></span>"}
    </div>`);
  }
  return `<div class="move-table" aria-label="Chronological game scoresheet">
    <div class="move-row move-heading"><span>#</span><span>White</span><span>Black</span></div>
    <div class="move-scroll">${rows.join("") || `<p class="empty-copy">Import a game to see its moves.</p>`}</div>
  </div>`;
}

function scoreMoveMarkup(index: number, san: string): string {
  const assessment = moveClassification(index);
  const label = assessment?.classification ?? "Pending";
  return `<button data-ply="${index}" class="move classification-${classificationClass(label)} ${state.selectedPly === index ? "current" : ""}">
    <span class="move-san">${escapeHtml(san)}</span><span class="move-label">${escapeHtml(label)}</span>
  </button>`;
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
    <button data-review-action="${isPlaying(state.reviewMode) ? "pause" : "play"}" class="play-control" aria-label="${isPlaying(state.reviewMode) ? "Pause review" : state.reviewMode === "paused_key_move" ? "Continue review" : "Play guided review"}" ${!plies.length ? "disabled" : ""}>${isPlaying(state.reviewMode) ? "Ⅱ" : "▶"}</button>
    <button data-nav="next" aria-label="Next move" ${state.selectedPly >= plies.length - 1 ? "disabled" : ""}>›</button>
    <button data-nav="last" aria-label="Last move" ${!plies.length ? "disabled" : ""}>›|</button>
    <button data-flip aria-label="Flip board" title="Flip board">↻</button>
  </div>`;
}

function reviewMarkup(): string {
  const moves = state.game?.analysis?.moves ?? [];
  const move = moves[state.selectedPly];
  if (!move) return `<section class="review-pane"><header><h2>Move review</h2><span>Waiting for analysis</span></header><div class="review-empty">${icons.book}<p>Import a game to begin at White’s first move.</p></div></section>`;
  const changed = move.best_uci && move.best_uci !== move.played_uci;
  const blocking = blockingClassifications.has(move.classification);
  return `<section class="review-pane classification-${classificationClass(move.classification)}">
    <header><div><span class="classification-mark" aria-hidden="true"></span><h2>${escapeHtml(move.classification)}</h2></div><span>${escapeHtml(titleCase(move.classification_state))}</span></header>
    <div class="move-commentary">
      <p class="move-kicker">${move.move_number}${move.side === "white" ? "." : "…"} ${escapeHtml(move.played_san || move.san)}</p>
      <h3>${escapeHtml(classificationHeadline(move))}</h3>
      <p>${escapeHtml(moveExplanation(move))}</p>
      ${move.tactical_tags.length ? `<div class="tactical-tags">${move.tactical_tags.map((tag) => `<span>${escapeHtml(titleCase(tag.replaceAll("_", " ")))}</span>`).join("")}</div>` : ""}
      <dl class="move-facts"><div><dt>Evaluation</dt><dd>${formatEval(move.evaluation_before)} → ${formatEval(move.evaluation_after)}</dd></div><div><dt>Expected points lost</dt><dd>${(move.expected_points_loss * 100).toFixed(1)}%</dd></div><div><dt>Evidence</dt><dd>Depth ${move.depth || "–"} · ${move.classification_model_version || "Tutor model"}</dd></div></dl>
      ${changed ? betterMoveMarkup(move, blocking) : ""}
    </div>
  </section>`;
}

function classificationHeadline(move: MoveAssessment): string {
  if (move.classification === "Brilliant") return "A sound idea with a real concession.";
  if (move.classification === "Great") return "A critical move that held the position together.";
  if (move.classification === "Book") return "Still following known opening play.";
  if (move.classification === "Best") return "The engine’s first choice.";
  if (move.classification === "Excellent") return "Nearly as strong as the best move.";
  if (move.classification === "Good") return "A healthy move that keeps the game intact.";
  if (move.classification === "Inaccuracy") return "A small opportunity slipped away.";
  if (move.classification === "Miss") return "There was a chance to punish the previous move.";
  if (move.classification === "Mistake") return "The position became materially harder.";
  return "A decisive swing needs a closer look.";
}

function moveExplanation(move: MoveAssessment): string {
  const loss = `${(move.expected_points_loss * 100).toFixed(1)}%`;
  if (move.classification === "Book") return "This move stays inside the versioned local opening reference without giving up a meaningful share of the position.";
  if (move.classification === "Brilliant") return "Deep verification found a sound material concession that preserved the position and was not already trivially winning.";
  if (move.classification === "Great") return "Multi-line verification found one critical move with a clear separation from the alternatives.";
  if (move.classification === "Best") return "This matched the engine’s leading candidate, within the model’s numerical tolerance.";
  if (move.classification === "Excellent") return `This stayed very close to the engine’s first choice, giving up only ${loss} expected points.`;
  if (move.classification === "Good") return `The position remains playable; the local model measures ${loss} expected-points loss.`;
  if (move.classification === "Inaccuracy") return `A more precise move was available. The local model measures ${loss} expected-points loss.`;
  if (move.classification === "Miss") return "Deep verification found a tactical or forced opportunity that the played move did not use.";
  if (move.classification === "Mistake") return `The move surrendered ${loss} expected points and needs a concrete alternative.`;
  return `The move surrendered ${loss} expected points; deep verification confirmed a decisive, explainable swing.`;
}

function titleCase(value: string): string {
  return value.replace(/\b\w/g, (letter) => letter.toUpperCase());
}

function betterMoveMarkup(move: MoveAssessment, blocking: boolean): string {
  if (state.reviewMode === "try_move") return `<section class="better-move try-panel"><h4>Find a better move</h4><p>The board is restored to the position before ${escapeHtml(move.played_san || move.san)}. Choose a source and destination on the board, or enter UCI below. Equivalent engine candidates are accepted.</p><form id="try-move-form"><input id="try-move" pattern="[a-h][1-8][a-h][1-8][qrbn]?" placeholder="e2e4" aria-label="Move in UCI" required><button>Check move</button></form><p role="status">${escapeHtml(state.tryMessage)}</p><button class="text-action" data-review-action="cancel">Return to game</button></section>`;
  if (state.reviewMode === "revealed_move" || state.reviewMode === "variation") return `<section class="better-move reveal-panel"><h4>${state.reviewMode === "variation" ? "Principal variation" : "Engine recommendation"}</h4><strong>${escapeHtml(move.best_san || move.best_uci)}</strong><p>${formatEval(move.evaluation_after_best)} instead of ${formatEval(move.evaluation_after)}.</p>${move.principal_variation.length ? `<ol>${move.principal_variation.slice(0, 8).map((pv, index) => `<li class="${index < state.variationCursor ? "played" : ""}">${escapeHtml(pv)}</li>`).join("")}</ol>` : ""}<div>${state.reviewMode === "variation" ? `<button data-review-action="variation-previous" ${state.variationCursor <= 0 ? "disabled" : ""}>Previous</button><button data-review-action="variation-next" ${state.variationCursor >= move.principal_variation.length ? "disabled" : ""}>Next</button>` : `<button data-review-action="variation" ${!move.principal_variation.length ? "disabled" : ""}>Play continuation</button>`}<button class="text-action" data-review-action="cancel">Return to game</button></div></section>`;
  return `<div class="better-actions"><button data-review-action="try">Study this move</button><button data-review-action="reveal">Reveal best move</button>${blocking ? `<button class="continue-review" data-review-action="play">Continue review</button>` : ""}</div>`;
}

function classificationSummaryMarkup(): string {
  const moves = state.game?.analysis?.moves ?? [];
  const order = ["Brilliant", "Great", "Best", "Excellent", "Good", "Book", "Inaccuracy", "Mistake", "Miss", "Blunder"];
  const white = classificationCounts(moves, "white");
  const black = classificationCounts(moves, "black");
  const tags = state.game?.game.tags ?? {};
  const distribution = (counts: Record<string, number>) => `<div class="summary-distribution">${order.flatMap((label) => Array.from({ length: counts[label] ?? 0 }, (_, index) => `<i class="classification-${classificationClass(label)}"><span class="sr-only">${label} ${index + 1} of ${counts[label]}</span></i>`)).join("")}</div>`;
  return `<section class="classification-summary open"><header><div><small>Move quality</small><h3>Game classification</h3></div><span>${moves.length} plies analyzed</span></header>
    <div class="summary-sides"><div><strong>${escapeHtml(tags.White ?? "White")}</strong>${distribution(white)}</div><div><strong>${escapeHtml(tags.Black ?? "Black")}</strong>${distribution(black)}</div></div>
    <div class="summary-table"><span class="summary-heading">Classification</span><strong>White</strong><strong>Black</strong>${order.map((label) => `<span class="summary-label classification-${classificationClass(label)}"><i></i>${label}</span><b class="${white[label] ? "has-value" : ""}">${white[label] ?? 0}</b><b class="${black[label] ? "has-value" : ""}">${black[label] ?? 0}</b>`).join("")}</div>
  </section>`;
}

function evaluationRailMarkup(): string {
  const move = state.game?.analysis?.moves[state.selectedPly];
  const value = Math.max(-600, Math.min(600, move?.evaluation_after ?? 0));
  const whiteShare = Math.max(4, Math.min(96, 50 + value / 12));
  return `<aside class="evaluation-rail" aria-label="Current engine evaluation">
    <span class="eval-score">${formatEval(move?.evaluation_after)}</span>
    <svg class="eval-meter" viewBox="0 0 14 100" preserveAspectRatio="none" aria-hidden="true"><rect class="eval-black" width="14" height="100"/><rect class="eval-white" y="${100 - whiteShare}" width="14" height="${whiteShare}"/><line x1="0" x2="14" y1="50" y2="50"/></svg>
    <span class="eval-side">White</span>
  </aside>`;
}

function evaluationGraphMarkup(): string {
  const moves = state.game?.analysis?.moves ?? [];
  if (!moves.length) return `<div class="inspector-empty">The evaluation timeline appears after analysis.</div>`;
  const width = 920;
  const height = 270;
  const plot = { left: 52, right: 24, top: 28, bottom: 54 };
  const values = moves.map((move) => move.evaluation_after);
  const maxAbs = Math.max(...values.map(Math.abs), 200);
  const range = Math.min(1000, Math.max(300, Math.ceil(maxAbs / 100) * 100));
  const x = (index: number) => plot.left + (index / Math.max(1, moves.length - 1)) * (width - plot.left - plot.right);
  const y = (value: number) => plot.top + ((range - Math.max(-range, Math.min(range, value))) / (range * 2)) * (height - plot.top - plot.bottom);
  const zeroY = y(0);
  const line = values.map((value, index) => `${index === 0 ? "M" : "L"}${x(index).toFixed(1)} ${y(value).toFixed(1)}`).join(" ");
  const area = `${line} L${x(values.length - 1).toFixed(1)} ${zeroY.toFixed(1)} L${x(0).toFixed(1)} ${zeroY.toFixed(1)} Z`;
  const current = moves[state.selectedPly] ?? moves[0];
  const selectedX = x(state.selectedPly);
  const selectedY = y(current?.evaluation_after ?? 0);
  const keyLabels = new Set(["Brilliant", "Great", "Inaccuracy", "Mistake", "Miss", "Blunder"]);
  const labelEvery = Math.max(1, Math.ceil(moves.length / 6));
  return `<div class="inspector-graph">
    <header class="graph-reading"><div><small>${current?.move_number}${current?.side === "black" ? "…" : "."} ${escapeHtml(current?.played_san || current?.san || "Position")}</small><strong>${formatEval(current?.evaluation_after)}</strong></div><span class="classification-${classificationClass(current?.classification ?? "pending")}"><i></i>${escapeHtml(current?.classification ?? "Pending")}</span><p>${escapeHtml(classificationHeadline(current))}</p></header>
    <svg viewBox="0 0 ${width} ${height}" role="img" aria-label="Engine evaluation by move with classification markers">
      <defs><linearGradient id="evaluation-area" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#8bd3b0" stop-opacity=".18"/><stop offset="1" stop-color="#8bd3b0" stop-opacity="0"/></linearGradient></defs>
      ${[-range, 0, range].map((value) => `<line class="graph-grid ${value === 0 ? "zero" : ""}" x1="${plot.left}" x2="${width - plot.right}" y1="${y(value)}" y2="${y(value)}"/><text class="graph-y-label" x="${plot.left - 10}" y="${y(value) + 4}" text-anchor="end">${value === 0 ? "EVEN" : formatEval(value)}</text>`).join("")}
      <path class="graph-area" d="${area}"/>
      <path class="graph-line" d="${line}"/>
      ${moves.map((move, index) => `<rect class="graph-classification classification-${classificationClass(move.classification)}" x="${x(index) - 3.5}" y="${height - 24}" width="7" height="7" rx="2"><title>${move.move_number}${move.side === "black" ? "…" : "."} ${escapeHtml(move.played_san || move.san)} · ${escapeHtml(move.classification)}</title></rect>`).join("")}
      ${moves.map((move, index) => index % labelEvery === 0 || index === moves.length - 1 ? `<text class="graph-x-label" x="${x(index)}" y="${height - 35}" text-anchor="middle">${move.move_number}</text>` : "").join("")}
      <line class="graph-selected-guide" x1="${selectedX}" x2="${selectedX}" y1="${plot.top}" y2="${height - plot.bottom}"/>
      ${moves.map((move, index) => `<circle class="graph-point ${keyLabels.has(move.classification) ? "key" : ""} ${index === state.selectedPly ? "selected" : ""} classification-${classificationClass(move.classification)}" data-eval-ply="${index}" tabindex="0" role="button" aria-label="Move ${move.move_number}${move.side === "black" ? " black" : " white"} ${escapeHtml(move.played_san || move.san)}, ${escapeHtml(move.classification)}, evaluation ${formatEval(move.evaluation_after)}" cx="${x(index)}" cy="${y(move.evaluation_after)}" r="${index === state.selectedPly ? 7 : keyLabels.has(move.classification) ? 4 : 7}"><title>${escapeHtml(move.played_san || move.san)} · ${escapeHtml(move.classification)} · ${formatEval(move.evaluation_after)}</title></circle>`).join("")}
      <circle class="graph-selected-core" cx="${selectedX}" cy="${selectedY}" r="3"/>
    </svg>
    <footer class="graph-legend"><span><i class="classification-best"></i>Strong</span><span><i class="classification-inaccuracy"></i>Inaccuracy</span><span><i class="classification-mistake"></i>Mistake</span><span><i class="classification-blunder"></i>Blunder</span><small>Move number →</small></footer>
  </div>`;
}

function inspectorMarkup(): string {
  const move = state.game?.analysis?.moves[state.selectedPly];
  const tabs: Array<[InspectorTab, string]> = [["line", "Line"], ["graph", "Graph"], ["summary", "Summary"], ["method", "Method"]];
  let content = "";
  if (state.inspectorTab === "line") {
    content = `<div class="inspector-line">${engineWorkMarkup()}<div class="pv-line"><small>Selected position</small><strong>${escapeHtml(move?.best_san || move?.best_uci || "Waiting for analysis")}</strong><code>${escapeHtml(move?.principal_variation.join(" ") || "No principal variation available yet")}</code></div></div>`;
  }
  if (state.inspectorTab === "graph") content = evaluationGraphMarkup();
  if (state.inspectorTab === "summary") {
    const analysis = state.game?.analysis;
    const totals = classificationSummaryMarkup();
    content = `${totals}<div class="opening-facts"><span>Opening</span><strong>${escapeHtml([analysis?.eco, analysis?.opening].filter(Boolean).join(" · ") || "Unclassified")}</strong><span>Known book</span><strong>${analysis?.book_ply ?? 0} plies</strong><span>Departure</span><strong>${analysis?.departure_ply === null || analysis?.departure_ply === undefined ? "Stayed in reference" : `Ply ${analysis.departure_ply + 1}`}</strong></div>`;
  }
  if (state.inspectorTab === "method") content = `<div class="method-note"><h3>What these labels mean</h3><p>Positions are reconstructed from the imported PGN, scanned locally, then deeper candidates are verified with Stockfish. The interface exposes telemetry and evidence, not private reasoning.</p><dl><div><dt>Engine</dt><dd>${escapeHtml(move?.engine_version || "Stockfish local")}</dd></div><div><dt>Classification</dt><dd>${escapeHtml(move?.classification_model_version || "Tutor Classification Model 1")}</dd></div><div><dt>Opening reference</dt><dd>${escapeHtml(move?.book_version || state.game?.analysis?.opening_book_version || "Not used for this move")}</dd></div><div><dt>Depth</dt><dd>${move?.depth || "—"}</dd></div></dl></div>`;
  return `<section class="analysis-inspector"><nav role="tablist" aria-label="Analysis inspector">${tabs.map(([key, label]) => `<button role="tab" aria-selected="${state.inspectorTab === key}" class="${state.inspectorTab === key ? "active" : ""}" data-inspector-tab="${key}">${label}</button>`).join("")}</nav><div class="inspector-content" role="tabpanel">${content}</div></section>`;
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
  window.clearTimeout(autoplayTimer);
  if (state.mode === "explore") {
    app.innerHTML = exploreShellMarkup();
    bindTrainingEvents();
    return;
  }
  if (state.mode === "progress") {
    app.innerHTML = progressShellMarkup();
    bindTrainingEvents();
    return;
  }
  if (state.mode === "training") {
    app.innerHTML = trainingShellMarkup();
    bindTrainingEvents();
    return;
  }
  const serviceOffline = state.error === "Local API service is not running.";
  app.innerHTML = `<div class="app-shell">
    <header class="app-header"><a href="/" class="brand"><span class="brand-mark" aria-hidden="true">${pieceSvg("knight")}</span><span>Personal Chess Tutor<small>Local analysis studio</small></span></a><nav><button data-mode="game" class="active" aria-current="page"><span>Review</span></button><button data-mode="explore"><span>Explore</span></button><button data-mode="progress"><span>Progress</span></button></nav><button class="menu-button" aria-label="Menu">${icons.menu}</button></header>
    <main class="workspace studio-workspace ${state.mobileView}">
      <button class="import-bar" id="open-import">${icons.upload}<span><strong>${state.busy ? "Resolving game" : "Import a game"}</strong><small>Chess.com link or PGN</small></span>${icons.chevron}</button>
      ${importLoadingMarkup()}
      <div class="summary-region">${gameSummaryMarkup()}</div>
      <section class="review-stage"><div class="board-column"><div class="board-with-eval">${evaluationRailMarkup()}<div class="board-region"><div class="board-stage">${boardMarkup()}</div>${navigationMarkup()}</div></div><div id="review-panel" class="review-region panel-slot">${reviewMarkup()}</div></div></section>
      <aside id="moves-panel" class="analysis-region scoresheet-region panel-slot">${moveListMarkup()}</aside>
      <div id="engine-panel" class="engine-region">${inspectorMarkup()}</div>
      ${mobileTabsMarkup()}
    </main>
    <footer class="local-status"><span class="privacy-note">Private by design · games never leave this computer</span><span class="engine-state ${serviceOffline || state.job?.status === "failed" ? "offline" : ""}"><i></i>${serviceOffline ? "Start local API service" : state.job?.status === "failed" ? "Engine unavailable" : "Stockfish ready"}</span></footer>
  </div>
  ${importDialogMarkup()}`;
  bindEvents();
  const ledger = document.querySelector<HTMLElement>(".move-scroll");
  if (ledger) ledger.scrollTop = state.ledgerScrollTop;
  scheduleAutoplay();
}

function exploreShellMarkup(): string {
  const entries = buildExploreEntries(state.games);
  const visible = entries.filter((entry) => entry.section === state.exploreSection);
  const selected = visible.find((entry) => entry.id === state.selectedExploreId) ?? visible[0];
  return `<div class="learning-shell studio-shell"><header class="app-header"><a href="/" class="brand"><span class="brand-mark">${pieceSvg("knight")}</span><span>Personal Chess Tutor<small>Local analysis studio</small></span></a><nav><button data-mode="game"><span>Review</span></button><button data-mode="explore" class="active"><span>Explore</span></button><button data-mode="progress"><span>Progress</span></button></nav></header><main class="explore-studio"><header class="page-intro"><p class="overline">Explore your own positions</p><h1>A working library, built from games you played.</h1><p>Every concept links back to the analyzed position that produced it.</p></header><nav class="section-tabs" aria-label="Position collection">${(["Openings", "Middlegames", "Endgames"] as ExploreSection[]).map((section) => `<button class="${section === state.exploreSection ? "active" : ""}" data-explore-section="${section}">${section}<span>${entries.filter((entry) => entry.section === section).length}</span></button>`).join("")}</nav>${selected ? `<div class="explore-layout"><aside class="concept-ledger">${visible.map((entry) => `<button class="${entry.id === selected.id ? "active" : ""}" data-explore-entry="${escapeHtml(entry.id)}"><strong>${escapeHtml(entry.title)}</strong><span>${escapeHtml(entry.purpose)}</span></button>`).join("")}</aside><section class="concept-study"><div class="mini-board board">${squaresFromFen(selected.fen).map((square, index) => `<div class="square ${(Math.floor(index / 8) + index) % 2 === 0 ? "light" : "dark"}">${pieceMarkup(square.piece, square.name)}</div>`).join("")}</div><div class="concept-copy"><p class="overline">${escapeHtml(selected.section.slice(0, -1))} concept</p><h2>${escapeHtml(selected.title)}</h2><p>${escapeHtml(selected.purpose)}</p><div class="tactical-tags">${selected.tags.map((tag) => `<span>${escapeHtml(titleCase(tag.replaceAll("_", " ")))}</span>`).join("")}</div><small>Source: ${escapeHtml(selected.source)}</small><button class="primary-action" data-open-game="${escapeHtml(selected.gameId)}" data-open-ply="${selected.ply}">Open this position in Review</button></div></section></div>` : `<section class="honest-empty"><h2>No ${state.exploreSection.toLowerCase()} yet.</h2><p>Analyze games that reach this phase and the library will assemble itself from those positions.</p><button data-mode="game" class="primary-action">Return to Review</button></section>`}</main></div>`;
}

function progressShellMarkup(): string {
  const player = inferPlayerName(state.profile, state.games);
  const ratings = ratingHistory(state.games, player);
  const delta = ratingDelta(ratings);
  const latest = ratings[ratings.length - 1];
  const range = ratings.length ? Math.max(...ratings.map((point) => point.rating)) - Math.min(...ratings.map((point) => point.rating)) || 1 : 1;
  const minimum = ratings.length ? Math.min(...ratings.map((point) => point.rating)) : 0;
  const polyline = ratings.map((point, index) => `${ratings.length <= 1 ? 50 : (index / (ratings.length - 1)) * 100},${44 - ((point.rating - minimum) / range) * 34}`).join(" ");
  const arc = reviewArc(state.games);
  const analyzed = state.games.filter((game) => game.analysis?.moves.length);
  const allMoves = analyzed.flatMap((game) => game.analysis?.moves ?? []);
  const critical = allMoves.filter((move) => blockingClassifications.has(move.classification));
  const best = allMoves.filter((move) => ["Best", "Excellent", "Great", "Brilliant"].includes(move.classification));
  return `<div class="learning-shell studio-shell"><header class="app-header"><a href="/" class="brand"><span class="brand-mark">${pieceSvg("knight")}</span><span>Personal Chess Tutor<small>Local analysis studio</small></span></a><nav><button data-mode="game"><span>Review</span></button><button data-mode="explore"><span>Explore</span></button><button data-mode="progress" class="active"><span>Progress</span></button></nav></header><main class="progress-studio"><header class="page-intro"><p class="overline">Evidence, not a composite score</p><h1>Your chess, over time.</h1><p>Built from locally imported PGN tags and completed analyses.</p></header><section class="rating-story"><div><small>${escapeHtml(player || "Player not inferred")}</small><strong>${latest?.rating ?? "—"}</strong><span class="${delta === null ? "neutral" : delta >= 0 ? "positive" : "negative"}">${delta === null ? "30-day change needs two dated ratings" : `${delta >= 0 ? "↑" : "↓"} ${Math.abs(delta)} in the latest 30-day window`}</span></div>${ratings.length ? `<svg viewBox="0 0 100 50" preserveAspectRatio="none" aria-label="Rating history from imported games"><line x1="0" y1="44" x2="100" y2="44"/><polyline points="${polyline}"/>${ratings.map((point, index) => `<circle cx="${ratings.length <= 1 ? 50 : (index / (ratings.length - 1)) * 100}" cy="${44 - ((point.rating - minimum) / range) * 34}" r="1.5"><title>${point.rating} vs ${escapeHtml(point.opponent)}</title></circle>`).join("")}</svg>` : `<div class="rating-empty">Import dated PGNs with rating tags to see rating movement.</div>`}</section><div class="progress-grid"><section class="quality-panel"><p class="overline">Review quality</p><h2>${analyzed.length} analyzed game${analyzed.length === 1 ? "" : "s"}</h2><div class="quality-stats"><div><strong>${best.length}</strong><span>strong decisions</span></div><div><strong>${critical.length}</strong><span>critical errors</span></div><div><strong>${allMoves.length}</strong><span>classified plies</span></div></div><p>Counts are direct classifications, not a synthetic performance rating.</p></section><section class="learning-arc"><p class="overline">Recent learning arc</p><h2>Positions worth revisiting</h2>${arc.slice(0, 8).map((entry) => `<button data-open-game="${escapeHtml(entry.gameId)}" data-open-ply="${entry.largestSwingPly}"><span><strong>${escapeHtml(entry.title)}</strong><small>${escapeHtml(entry.opening)} · ${escapeHtml(entry.result)}</small></span><em>${(entry.largestSwing * 100).toFixed(1)}% swing</em></button>`).join("") || `<p>No completed analyses yet. Review a game to create the first learning marker.</p>`}</section></div></main></div>`;
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
  document.querySelectorAll<HTMLButtonElement>("[data-explore-section]").forEach((button) => button.addEventListener("click", () => {
    state.exploreSection = button.dataset.exploreSection as ExploreSection;
    state.selectedExploreId = "";
    render();
  }));
  document.querySelectorAll<HTMLButtonElement>("[data-explore-entry]").forEach((button) => button.addEventListener("click", () => {
    state.selectedExploreId = button.dataset.exploreEntry ?? "";
    render();
  }));
  document.querySelectorAll<HTMLButtonElement>("[data-open-game]").forEach((button) => button.addEventListener("click", () => {
    void openStoredGame(button.dataset.openGame ?? "", Number(button.dataset.openPly ?? 0));
  }));
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
  document.querySelector<HTMLElement>(".move-scroll")?.addEventListener("scroll", (event) => {
    state.ledgerScrollTop = (event.currentTarget as HTMLElement).scrollTop;
  });
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
  document.querySelectorAll<SVGCircleElement>("[data-eval-ply]").forEach((point) => {
    point.addEventListener("click", () => selectPly(Number(point.dataset.evalPly)));
    point.addEventListener("keydown", (event) => { if (event.key === "Enter" || event.key === " ") selectPly(Number(point.dataset.evalPly)); });
  });
  document.querySelectorAll<HTMLButtonElement>("[data-inspector-tab]").forEach((button) => button.addEventListener("click", () => {
    state.inspectorTab = button.dataset.inspectorTab as InspectorTab;
    render();
  }));
  document.querySelectorAll<HTMLElement>(".square[data-square]").forEach((square) => {
    const choose = () => chooseTrySquare(square.dataset.square ?? "");
    square.addEventListener("click", choose);
    square.addEventListener("keydown", (event) => { if (event.key === "Enter" || event.key === " ") { event.preventDefault(); choose(); } });
  });
  document.querySelectorAll<HTMLButtonElement>("[data-nav]").forEach((button) => {
    button.addEventListener("click", () => navigate(button.dataset.nav ?? ""));
  });
  document.querySelector<HTMLButtonElement>("[data-flip]")?.addEventListener("click", () => {
    state.boardOrientation = state.boardOrientation === "white" ? "black" : "white";
    render();
  });
  document.querySelectorAll<HTMLButtonElement>("[data-review-action]").forEach((button) => {
    button.addEventListener("click", () => reviewAction(button.dataset.reviewAction ?? ""));
  });
  document.querySelector<HTMLFormElement>("#try-move-form")?.addEventListener("submit", (event) => {
    event.preventDefault();
    const move = state.game?.analysis?.moves[state.selectedPly];
    const attempt = document.querySelector<HTMLInputElement>("#try-move")?.value ?? "";
    state.tryMessage = isAcceptedTry(move, attempt)
      ? "That works. It matches an accepted engine candidate."
      : "Not this time. Look for a move that changes the forcing sequence.";
    state.trySourceSquare = "";
    render();
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

function chooseTrySquare(square: string): void {
  if (state.reviewMode !== "try_move" || !square) return;
  if (!state.trySourceSquare) {
    state.trySourceSquare = square;
    state.tryMessage = `Selected ${square}. Choose a destination.`;
    render();
    return;
  }
  const move = state.game?.analysis?.moves[state.selectedPly];
  const accepted = acceptedSquareMove(move, state.trySourceSquare, square);
  state.tryMessage = accepted
    ? "That works. It matches an accepted engine candidate."
    : "Not this time. The position is unchanged—choose another source square.";
  state.trySourceSquare = "";
  render();
}

async function openStoredGame(gameId: string, ply: number): Promise<void> {
  const cached = state.games.find((game) => game.game.id === gameId);
  state.game = cached ?? await loadGame(gameId);
  state.selectedPly = Math.max(0, Math.min(ply, state.game.game.plies.length - 1));
  state.reviewMode = pauseForSelectedMove(state.game.analysis?.moves[state.selectedPly]);
  state.mode = "game";
  state.highlightedUci = "";
  state.trySourceSquare = "";
  state.trySourceSquare = "";
  render();
}

function selectPly(ply: number): void {
  state.selectedPly = ply;
  state.reviewMode = pauseForSelectedMove(state.game?.analysis?.moves[ply]);
  state.highlightedUci = "";
  state.tryMessage = "";
  render();
}

function navigate(action: string): void {
  const last = Math.max(0, (state.game?.game.plies.length ?? 1) - 1);
  if (action === "first") state.selectedPly = 0;
  if (action === "previous") state.selectedPly = Math.max(0, state.selectedPly - 1);
  if (action === "next") state.selectedPly = Math.min(last, state.selectedPly + 1);
  if (action === "last") state.selectedPly = last;
  state.highlightedUci = "";
  state.reviewMode = pauseForSelectedMove(state.game?.analysis?.moves[state.selectedPly]);
  state.tryMessage = "";
  state.trySourceSquare = "";
  render();
}

function reviewAction(action: string): void {
  const move = state.game?.analysis?.moves[state.selectedPly];
  if (action === "play") {
    const transition = startPlayback(state.reviewMode, state.selectedPly, state.game?.analysis?.moves ?? []);
    state.reviewMode = transition.mode;
    state.selectedPly = transition.selectedPly;
  }
  if (action === "pause") state.reviewMode = "manual";
  if (action === "try") {
    state.reviewMode = "try_move";
    state.highlightedUci = "";
    state.tryMessage = "";
    state.trySourceSquare = "";
  }
  if (action === "reveal") {
    state.reviewMode = "revealed_move";
    state.highlightedUci = move?.best_uci ?? "";
  }
  if (action === "variation") state.reviewMode = "variation";
  if (action === "variation") state.variationCursor = Math.min(1, move?.principal_variation.length ?? 0);
  if (action === "variation-previous") state.variationCursor = Math.max(0, state.variationCursor - 1);
  if (action === "variation-next") state.variationCursor = Math.min(move?.principal_variation.length ?? 0, state.variationCursor + 1);
  if (action === "cancel") {
    state.reviewMode = pauseForSelectedMove(move);
    state.highlightedUci = "";
    state.tryMessage = "";
    state.variationCursor = 0;
    state.trySourceSquare = "";
  }
  render();
}

function scheduleAutoplay(): void {
  if (!isPlaying(state.reviewMode)) return;
  const moves = state.game?.analysis?.moves ?? [];
  const delayMs = state.reviewMode === "transitioning_from_key_move" ? 700 : autoplayDelay(moves[state.selectedPly]);
  if (delayMs === null || document.hidden) {
    state.reviewMode = "manual";
    autoplayTimer = window.setTimeout(render, 0);
    return;
  }
  autoplayTimer = window.setTimeout(() => {
    const transition = completePlaybackDwell(state.reviewMode, state.selectedPly, moves);
    state.selectedPly = transition.selectedPly;
    state.reviewMode = transition.mode;
    state.highlightedUci = "";
    render();
  }, delayMs);
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
    state.selectedPly = Math.max(0, firstReviewPly(state.game.game.plies.length));
    state.reviewMode = "manual";
    state.expandedMistake = 0;
    state.games = [state.game, ...state.games.filter((game) => game.game.id !== state.game?.game.id)];
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
    state.games = [state.game, ...state.games.filter((game) => game.game.id !== state.game?.game.id)];
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
    state.games = await Promise.all(games.map((game) => loadGame(game.game.id)));
    if (state.games[0]) {
      state.game = state.games[0];
      state.selectedPly = Math.max(0, firstReviewPly(state.game.game.plies.length));
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
