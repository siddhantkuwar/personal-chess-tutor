import { autoplayDelay, clampPly, classificationCounts, completePlaybackDwell, firstReviewPly, startPlayback } from "../src/review";
import type { MoveAssessment } from "../src/types";

function assert(actual: unknown, expected: unknown, label: string): void {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(`${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
  }
}

const move = (overrides: Partial<MoveAssessment>): MoveAssessment => ({
  ply: 0,
  move_number: 1,
  side: "white",
  played_uci: "e2e4",
  played_san: "e4",
  san: "e4",
  fen_before: "",
  fen_after: "",
  best_uci: "e2e4",
  best_san: "e4",
  evaluation_before: 0,
  evaluation_after: 10,
  evaluation_after_best: 10,
  loss: 0,
  expected_points_before: 0.5,
  expected_points_after: 0.51,
  expected_points_loss: 0,
  quality: "Best",
  classification: "Best",
  classification_state: "final",
  classification_reasons: [],
  tactical_tags: [],
  principal_variation: [],
  acceptable_alternatives: [],
  phase: "opening",
  best_response: "",
  book_source: "",
  book_version: "",
  depth: 16,
  nodes: 1,
  time_ms: 1,
  multipv: 1,
  engine_version: "test",
  classification_model_version: "tutor-classification-1",
  ...overrides,
});

assert(firstReviewPly(18), 0, "new game starts after White move one");
assert(firstReviewPly(0), -1, "empty game has no selected ply");
assert(clampPly(99, 18), 17, "cursor clamps at end");
assert(autoplayDelay(move({ classification: "Book" })), 380, "routine move speed");
assert(autoplayDelay(move({ classification: "Great" })), 1500, "positive pause");
assert(autoplayDelay(move({ classification: "Inaccuracy" })), 850, "inaccuracy pause");
assert(autoplayDelay(move({ classification: "Blunder" })), null, "blunder blocks autoplay");
const adjacent = [
  move({ ply: 0, classification: "Blunder" }),
  move({ ply: 1, side: "black", classification: "Blunder" }),
  move({ ply: 2, classification: "Good" }),
];
assert(startPlayback("paused_key_move", 0, adjacent), { mode: "transitioning_from_key_move", selectedPly: 1, delayMs: 700 }, "continue visibly advances from a paused blunder");
assert(completePlaybackDwell("transitioning_from_key_move", 1, adjacent), { mode: "paused_key_move", selectedPly: 1, delayMs: null }, "adjacent blunder pauses after readable dwell");
assert(startPlayback("paused_key_move", 1, adjacent), { mode: "transitioning_from_key_move", selectedPly: 2, delayMs: 700 }, "second continue never needs manual next");
const criticalRun = [
  move({ ply: 0, classification: "Mistake" }),
  move({ ply: 1, side: "black", classification: "Miss" }),
  move({ ply: 2, classification: "Blunder" }),
];
assert(startPlayback("paused_key_move", 0, criticalRun).selectedPly, 1, "mistake continue selects adjacent miss");
assert(completePlaybackDwell("transitioning_from_key_move", 1, criticalRun).mode, "paused_key_move", "miss receives its own pause");
assert(startPlayback("paused_key_move", 1, criticalRun).selectedPly, 2, "miss continue selects adjacent blunder");
assert(completePlaybackDwell("transitioning_from_key_move", 2, criticalRun).mode, "paused_key_move", "blunder receives its own pause");
assert(classificationCounts([
  move({ side: "white", classification: "Best" }),
  move({ side: "black", classification: "Mistake" }),
  move({ side: "white", classification: "Best" }),
], "white"), { Best: 2 }, "classification totals are side-specific");

console.log("review state tests passed");
