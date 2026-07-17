import type {
  ApiFailure,
  BatchProgress,
  ChessComProfileResponse,
  Diagnostics,
  Drill,
  DrillAttempt,
  ImportGameResponse,
  ImportResolution,
  IngestSync,
  Job,
  Profile,
  ResourceRecommendation,
  RuntimeSettings,
  StoredGame,
} from "./types";

export class ApiError extends Error {
  readonly status: number;
  readonly code?: string;
  readonly actions: NonNullable<ApiFailure["actions"]>;

  constructor(status: number, body: ApiFailure) {
    super(body.error || `Request failed with HTTP ${status}`);
    this.name = "ApiError";
    this.status = status;
    this.code = body.code;
    this.actions = body.actions ?? [];
  }
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    ...init,
    headers: { "Content-Type": "application/json", ...init?.headers },
  });
  const contentType = response.headers.get("content-type") ?? "";
  if (!contentType.includes("application/json")) {
    throw new Error("Local API service is not running.");
  }
  const body = (await response.json()) as T & Partial<ApiFailure>;
  if (!response.ok) {
    throw new ApiError(response.status, {
      error: body.error ?? `Request failed with HTTP ${response.status}`,
      code: body.code,
      actions: body.actions,
    });
  }
  return body;
}

export async function listGames(): Promise<StoredGame[]> {
  const response = await request<{ games: StoredGame[] }>("/api/games");
  return response.games;
}

export function loadGame(id: string): Promise<StoredGame> {
  return request<StoredGame>(`/api/games/${encodeURIComponent(id)}`);
}

export function importGame(input: { url: string } | { pgn: string }): Promise<{
  duplicate: boolean;
  game_id: string;
  job: Job;
}> {
  return request("/api/import", { method: "POST", body: JSON.stringify(input) });
}

/**
 * Correctly models both API outcomes. Retained separately so existing callers
 * of importGame keep their Phase 1 compile-time contract while Phase 2 can
 * render asynchronous URL-resolution progress.
 */
export function importGameObservable(
  input: { url: string; username?: string } | { pgn: string },
): Promise<ImportGameResponse> {
  return request("/api/import", { method: "POST", body: JSON.stringify(input) });
}

export function startAnalysis(id: string): Promise<Job> {
  return request(`/api/games/${encodeURIComponent(id)}/analysis`, { method: "POST" });
}

export function loadJob(id: number): Promise<Job> {
  return request(`/api/jobs/${id}`);
}

export async function loadJobs(): Promise<{ jobs: Job[]; paused: boolean }> {
  return request("/api/jobs");
}

export function cancelJob(id: number): Promise<Job> {
  return request(`/api/jobs/${id}`, { method: "DELETE" });
}

/** Re-submits analysis through the public job route; it does not expose engine internals. */
export function retryAnalysis(job: Pick<Job, "game_id">): Promise<Job> {
  return startAnalysis(job.game_id);
}

export function loadDiagnostics(): Promise<Diagnostics> {
  return request("/api/diagnostics");
}

export function loadRuntimeSettings(): Promise<RuntimeSettings> {
  return request("/api/settings");
}

export function loadChessComProfile(): Promise<ChessComProfileResponse> {
  return request("/api/chesscom/profile");
}

export function configureChessComProfile(input: {
  username: string;
  time_controls?: string[];
}): Promise<ChessComProfileResponse> {
  return request("/api/chesscom/profile", { method: "PUT", body: JSON.stringify(input) });
}

export function resolveImport(input: { url: string; username?: string }): Promise<ImportResolution> {
  return request("/api/import/resolve", { method: "POST", body: JSON.stringify(input) });
}

export function loadImportResolution(id: string): Promise<ImportResolution> {
  return request(`/api/import/resolutions/${encodeURIComponent(id)}`);
}

export function cancelImportResolution(id: string): Promise<ImportResolution> {
  return request(`/api/import/resolutions/${encodeURIComponent(id)}`, { method: "DELETE" });
}

export function retryImportResolution(
  resolution: Pick<ImportResolution, "url" | "username">,
): Promise<ImportResolution> {
  return resolveImport({ url: resolution.url, ...(resolution.username ? { username: resolution.username } : {}) });
}

export function startChessComSync(input: {
  days: 7 | 30 | 90;
  username?: string;
}): Promise<IngestSync> {
  return request("/api/chesscom/sync", { method: "POST", body: JSON.stringify(input) });
}

export function loadChessComSync(id = "current"): Promise<IngestSync> {
  return request(`/api/chesscom/sync/${encodeURIComponent(id)}`);
}

export function cancelChessComSync(id = "current"): Promise<IngestSync> {
  return request(`/api/chesscom/sync/${encodeURIComponent(id)}`, { method: "DELETE" });
}

export async function loadDrills(): Promise<Drill[]> {
  return (await request<{ drills: Drill[] }>("/api/drills")).drills;
}

export function generateSupplementalDrills(): Promise<{ added: number; drills: Drill[] }> {
  return request("/api/drills/supplemental", { method: "POST", body: "{}" });
}

export function loadProfile(): Promise<Profile> {
  return request<Profile>("/api/profile");
}

export async function loadResources(): Promise<ResourceRecommendation[]> {
  return (await request<{ resources: ResourceRecommendation[] }>("/api/resources")).resources;
}

export function submitDrillAttempt(id: string, move: string, responseTimeMs: number, hintLevel: number): Promise<{ attempt: DrillAttempt; drill: Drill }> {
  return request(`/api/drills/${encodeURIComponent(id)}/attempt`, {
    method: "POST",
    body: JSON.stringify({ move, response_time_ms: responseTimeMs, hint_level: hintLevel }),
  });
}

export function beginDrillSession(id: string): Promise<Drill> {
  return request(`/api/drills/${encodeURIComponent(id)}/session`, {
    method: "POST",
    body: "{}",
  });
}

export function advanceDrillHint(id: string): Promise<Drill> {
  return request(`/api/drills/${encodeURIComponent(id)}/hint`, {
    method: "POST",
    body: "{}",
  });
}

export function completeResource(id: string): Promise<{ completed: boolean }> {
  return request(`/api/resources/${encodeURIComponent(id)}/complete`, { method: "POST", body: "{}" });
}

export function importBatch(pgns: string[], urls: string[]): Promise<{ discovered: number; imported: number; duplicates: number; queued: number; failed: number }> {
  return request("/api/import/batch", { method: "POST", body: JSON.stringify({ pgns, urls }) });
}

export function loadBatches(): Promise<{ batches: BatchProgress[]; paused: boolean; cache_hits: number }> {
  return request("/api/batches");
}

export function setQueuePaused(paused: boolean): Promise<{ paused: boolean }> {
  return request(`/api/jobs/${paused ? "pause" : "resume"}`, { method: "POST", body: "{}" });
}
