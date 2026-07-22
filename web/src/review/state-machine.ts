import type { MoveAssessment } from "../types";

export type ReviewMode =
  | "manual"
  | "playing_routine"
  | "paused_key_move"
  | "transitioning_from_key_move"
  | "try_move"
  | "revealed_move"
  | "variation";

export interface PlaybackTransition {
  mode: ReviewMode;
  selectedPly: number;
  delayMs: number | null;
}

export const routineClassifications = new Set(["Book", "Best", "Excellent", "Good"]);
export const positiveClassifications = new Set(["Brilliant", "Great"]);
export const blockingClassifications = new Set(["Mistake", "Miss", "Blunder"]);

export function autoplayDelay(move: MoveAssessment | undefined): number | null {
  if (!move) return null;
  if (blockingClassifications.has(move.classification)) return null;
  if (positiveClassifications.has(move.classification)) return 1500;
  if (move.classification === "Inaccuracy") return 850;
  return 380;
}

export function startPlayback(mode: ReviewMode, selectedPly: number, moves: MoveAssessment[]): PlaybackTransition {
  if (!moves.length || selectedPly >= moves.length - 1) return { mode: "manual", selectedPly, delayMs: null };
  const selected = moves[selectedPly];
  if (mode === "paused_key_move" || blockingClassifications.has(selected?.classification ?? "")) {
    return {
      mode: "transitioning_from_key_move",
      selectedPly: selectedPly + 1,
      delayMs: 700,
    };
  }
  return { mode: "playing_routine", selectedPly, delayMs: autoplayDelay(selected) };
}

export function completePlaybackDwell(mode: ReviewMode, selectedPly: number, moves: MoveAssessment[]): PlaybackTransition {
  const selected = moves[selectedPly];
  if (mode === "transitioning_from_key_move" && blockingClassifications.has(selected?.classification ?? "")) {
    return { mode: "paused_key_move", selectedPly, delayMs: null };
  }
  if (selectedPly >= moves.length - 1) return { mode: "manual", selectedPly, delayMs: null };
  const nextPly = selectedPly + 1;
  if (blockingClassifications.has(moves[nextPly]?.classification ?? "")) {
    return { mode: "paused_key_move", selectedPly: nextPly, delayMs: null };
  }
  return {
    mode: "playing_routine",
    selectedPly: nextPly,
    delayMs: autoplayDelay(moves[nextPly]),
  };
}

export function pauseForSelectedMove(move: MoveAssessment | undefined): ReviewMode {
  return move && blockingClassifications.has(move.classification) ? "paused_key_move" : "manual";
}

export function isPlaying(mode: ReviewMode): boolean {
  return mode === "playing_routine" || mode === "transitioning_from_key_move";
}
