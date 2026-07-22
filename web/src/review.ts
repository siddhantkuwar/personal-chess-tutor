import type { MoveAssessment } from "./types";
import { blockingClassifications, positiveClassifications } from "./review/state-machine";
export * from "./review/state-machine";

export function firstReviewPly(moveCount: number): number {
  return moveCount > 0 ? 0 : -1;
}

export function clampPly(ply: number, moveCount: number): number {
  if (moveCount <= 0) return -1;
  return Math.max(0, Math.min(moveCount - 1, Math.trunc(ply)));
}

export function classificationCounts(moves: MoveAssessment[], side: "white" | "black"): Record<string, number> {
  const counts: Record<string, number> = {};
  for (const move of moves) {
    if (move.side !== side) continue;
    counts[move.classification] = (counts[move.classification] ?? 0) + 1;
  }
  return counts;
}

export function isKeyMove(move: MoveAssessment | undefined): boolean {
  return Boolean(move && (blockingClassifications.has(move.classification) || positiveClassifications.has(move.classification)));
}
