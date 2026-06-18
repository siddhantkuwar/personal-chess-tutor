import type { Job, StoredGame } from "./types";

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
