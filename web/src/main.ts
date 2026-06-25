import "./styles.css";
import { advanceDrillHint, beginDrillSession, completeResource, generateSupplementalDrills, importBatch, importGame, listGames, loadBatches, loadDrills, loadGame, loadProfile, loadResources, setQueuePaused, submitDrillAttempt } from "./api";
import { squaresFromFen, uciSquares } from "./chess";
import { icons } from "./icons";
import type { BatchProgress, Drill, Job, Mistake, Profile, ResourceRecommendation, StoredGame } from "./types";

type MobileView = "game" | "moves" | "review";
type AppMode = "game" | "training" | "progress";

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
}

const initialFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const state: State = {
  game: null,
  selectedPly: 0,
  expandedMistake: 0,
  highlightedUci: "",
  job: null,
  error: "",
  busy: false,
  mobileView: "game",
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
  const squares = squaresFromFen(currentFen());
  return `<div class="board-wrap">
    <div class="board" role="grid" aria-label="Chess position">
      ${squares.map((square, index) => {
        const light = (Math.floor(index / 8) + index) % 2 === 0;
        const selected = highlighted?.includes(square.name) ?? false;
        return `<div class="square ${light ? "light" : "dark"} ${selected ? "selected" : ""}" role="gridcell" data-square="${square.name}">
          ${square.file === "a" ? `<span class="rank-coordinate">${square.rank}</span>` : ""}
          ${square.rank === "1" ? `<span class="file-coordinate">${square.file}</span>` : ""}
          <span class="piece" aria-label="${square.piece ? `piece on ${square.name}` : `empty ${square.name}`}">${square.piece}</span>
        </div>`;
      }).join("")}
      ${arrowMarkup(highlighted)}
    </div>
  </div>`;
}

function arrowMarkup(highlighted: [string, string] | null): string {
  if (!state.highlightedUci || !highlighted) return "";
  const center = (name: string): [number, number] => [
    (name.charCodeAt(0) - 97 + 0.5) * 12.5,
    (8 - Number(name[1]) + 0.5) * 12.5,
  ];
  const [x1, y1] = center(highlighted[0]);
  const [x2, y2] = center(highlighted[1]);
  return `<svg class="move-arrow" viewBox="0 0 100 100" aria-hidden="true">
    <defs><marker id="arrowhead" markerWidth="5" markerHeight="5" refX="3" refY="2.5" orient="auto"><path d="M0 0 5 2.5 0 5Z"/></marker></defs>
    <line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" marker-end="url(#arrowhead)"/>
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
  return `<section class="game-summary">
    <div class="players"><h2>${escapeHtml(tags.White ?? "No game imported")}<span> vs. </span>${escapeHtml(tags.Black ?? "")}</h2><strong>${escapeHtml(tags.Result ?? "")}</strong></div>
    ${statusMarkup()}
    ${state.error ? `<p class="inline-error" role="alert">${escapeHtml(state.error)}</p>` : ""}
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
  return `<nav class="mobile-tabs" aria-label="Game sections">
    ${(["game", "moves", "review"] as const).map((view) => `<button data-view="${view}" class="${state.mobileView === view ? "active" : ""}">${view === "game" ? "▦" : view === "moves" ? "☷" : "☆"}<span>${view[0]?.toUpperCase()}${view.slice(1)}</span></button>`).join("")}
  </nav>`;
}

function render(): void {
  if (state.mode !== "game") {
    app.innerHTML = trainingShellMarkup();
    bindTrainingEvents();
    return;
  }
  app.innerHTML = `<div class="app-shell">
    <header class="app-header"><a href="/" class="brand">Personal Chess Tutor</a><nav><button data-mode="game" class="active">${icons.book}<span>Study</span></button><button data-mode="training">${icons.chevron}<span>Train</span></button><button data-mode="progress">${icons.chart}<span>Progress</span></button></nav><button class="menu-button" aria-label="Menu">${icons.menu}</button></header>
    <button class="import-bar" id="open-import">${icons.upload}<strong>${state.busy ? "Importing…" : "Import game"}</strong><span>Drag &amp; drop a .pgn file or paste PGN</span>${icons.chevron}</button>
    <main class="workspace ${state.mobileView}">
      <div class="summary-region">${gameSummaryMarkup()}</div>
      <section class="board-region">${boardMarkup()}${navigationMarkup()}${mobileTabsMarkup()}</section>
      <aside class="analysis-region">${moveListMarkup()}${evaluationMarkup()}</aside>
      <div class="review-region">${reviewMarkup()}</div>
    </main>
    <footer class="local-status"><span>All analysis is performed locally on your device.</span><span>Your games and analysis stay on this computer.</span><span class="engine-state"><i></i>${state.job?.status === "failed" ? "Engine unavailable" : "Local service ready"}</span></footer>
  </div>
  ${importDialogMarkup()}`;
  bindEvents();
}

function trainingBoardMarkup(drill: Drill | undefined): string {
  if (!drill) return `<div class="training-empty">Analyze a game to create your first exact-position drill.</div>`;
  const solution = drill.solutions[0] ?? "";
  const highlighted = state.shownHint >= 1 && solution.length >= 2 ? [solution.slice(0, 2)] : null;
  const fen = state.showPunishment && drill.fen_after_punishment ? drill.fen_after_punishment : drill.fen;
  return `<div class="training-board board" role="grid" aria-label="Drill position">${squaresFromFen(fen).map((square, index) => {
    const light = (Math.floor(index / 8) + index) % 2 === 0;
    const selected = highlighted?.includes(square.name) ?? false;
    return `<div class="square ${light ? "light" : "dark"} ${selected ? "selected" : ""}" role="gridcell"><span class="piece">${square.piece}</span></div>`;
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
  return `<div class="learning-shell">
    <header class="app-header"><a href="/" class="brand">Personal Chess Tutor</a><nav><button data-mode="game">${icons.book}<span>Study</span></button><button data-mode="training" class="${state.mode === "training" ? "active" : ""}">${icons.chevron}<span>Train</span></button><button data-mode="progress" class="${state.mode === "progress" ? "active" : ""}">${icons.chart}<span>Progress</span></button></nav></header>
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
  return `<dialog id="import-dialog"><form method="dialog" class="dialog-shell"><header><h2>Import a game</h2><button value="cancel" aria-label="Close">×</button></header>
    <label>Chess.com game URL<input id="game-url" type="url" placeholder="https://www.chess.com/game/live/…" autocomplete="url"></label>
    <div class="or"><span>or paste PGN</span></div>
    <label><span class="sr-only">PGN</span><textarea id="game-pgn" placeholder="[Event &quot;…&quot;]&#10;&#10;1. e4 e5 …"></textarea></label>
    <p class="dialog-error" role="alert">${escapeHtml(state.error)}</p>
    <footer><button value="cancel" class="secondary-action">Cancel</button><button value="default" id="submit-import" class="primary-action">Import and analyze</button></footer>
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
  state.busy = true;
  state.error = "";
  render();
  try {
    const result = await importGame(input);
    state.job = result.job;
    state.game = await loadGame(result.game_id);
    state.selectedPly = Math.max(0, state.game.game.plies.length - 1);
    state.expandedMistake = 0;
  } catch (error) {
    state.error = error instanceof Error ? error.message : "Import failed.";
  } finally {
    state.busy = false;
    render();
    if (state.error) document.querySelector<HTMLDialogElement>("#import-dialog")?.showModal();
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
    const message = JSON.parse(String(event.data)) as
      | { type: "job_update"; job: Job }
      | { type: "jobs_snapshot"; jobs: Job[] };
    const job = message.type === "job_update"
      ? message.job
      : message.jobs.find((candidate) => !state.game || candidate.game_id === state.game.game.id);
    if (!job || (state.game && job.game_id !== state.game.game.id)) return;
    state.job = job;
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
  try {
    const [games] = await Promise.all([listGames(), refreshTraining()]);
    if (games[0]) {
      state.game = await loadGame(games[0].game.id);
      state.selectedPly = Math.max(0, state.game.game.plies.length - 1);
    }
  } catch (error) {
    state.error = error instanceof Error ? error.message : "Local service is unavailable.";
  }
  render();
}

void start();
