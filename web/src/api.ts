import type { BatchProgress, Drill, DrillAttempt, Job, Profile, ResourceRecommendation, StoredGame } from "./types";

interface ApiErrorBody {
  error?: string;
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    ...init,
    headers: { "Content-Type": "application/json", ...init?.headers },
  });
  const body = (await response.json()) as T & ApiErrorBody;
  if (!response.ok) {
    throw new Error(body.error ?? `Request failed with HTTP ${response.status}`);
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

export function startAnalysis(id: string): Promise<Job> {
  return request(`/api/games/${encodeURIComponent(id)}/analysis`, { method: "POST" });
}

export async function loadDrills(): Promise<Drill[]> {
  return (await request<{ drills: Drill[] }>("/api/drills")).drills;
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
