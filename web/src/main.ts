import "./styles.css";
import { importGame, listGames, loadGame } from "./api";
import { squaresFromFen, uciSquares } from "./chess";
import { icons } from "./icons";
import type { Job, Mistake, StoredGame } from "./types";

type MobileView = "game" | "moves" | "review";

interface State {
  game: StoredGame | null;
  selectedPly: number;
  expandedMistake: number;
  highlightedUci: string;
  job: Job | null;
  error: string;
  busy: boolean;
  mobileView: MobileView;
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
};

const root = document.querySelector<HTMLDivElement>("#app");
if (!root) throw new Error("Application root is missing");
const app: HTMLDivElement = root;

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
  app.innerHTML = `<div class="app-shell">
    <header class="app-header"><a href="/" class="brand">Personal Chess Tutor</a><nav><button>${icons.book}<span>Study</span></button><button>${icons.settings}<span>Settings</span></button><button>${icons.help}<span>Help</span></button></nav><button class="menu-button" aria-label="Menu">${icons.menu}</button></header>
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
    if (job.status === "complete") void refreshGame();
    else render();
  });
  socket.addEventListener("close", () => window.setTimeout(connectProgress, 1500));
}

async function start(): Promise<void> {
  render();
  connectProgress();
  try {
    const games = await listGames();
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
