import { buildExploreEntries, ratingDelta, ratingHistory, reviewArc } from "../src/insights";
import type { MoveAssessment, StoredGame } from "../src/types";

function assert(actual: unknown, expected: unknown, label: string): void {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) throw new Error(`${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

const move = (ply: number, phase: string, tags: string[], loss = 0): MoveAssessment => ({
  ply, move_number: Math.floor(ply / 2) + 1, side: ply % 2 ? "black" : "white", played_uci: "e2e4", played_san: "e4", san: "e4",
  fen_before: "8/8/8/8/8/8/4P3/4K2k w - - 0 1", fen_after: "8/8/8/8/4P3/8/8/4K2k b - - 0 1", best_uci: "e2e4", best_san: "e4",
  evaluation_before: 0, evaluation_after: 0, evaluation_after_best: 0, loss: 0, expected_points_before: .5, expected_points_after: .5,
  expected_points_loss: loss, quality: "Good", classification: loss > .2 ? "Blunder" : "Good", classification_state: "final",
  classification_reasons: [], tactical_tags: tags, principal_variation: [], acceptable_alternatives: [], phase, best_response: "", book_source: "", book_version: "",
  depth: 18, nodes: 1, time_ms: 1, multipv: 1, engine_version: "test", classification_model_version: "test",
});

const stored = (date: string, rating: string, moves: MoveAssessment[]): StoredGame => ({
  game: { id: date, tags: { White: "Alex", Black: "Rival", WhiteElo: rating, BlackElo: "1400", UTCDate: date, Result: "1-0" }, plies: [] },
  source_url: "", import_method: "manual", analysis_status: "complete",
  analysis: { game_id: date, moves, mistakes: [], eco: "C20", opening: "King's Pawn Game", book_ply: 2, departure_ply: 2, opening_book_version: "2026.1", accuracy: 92, white_accuracy: 94, black_accuracy: 90, accuracy_sample_size: moves.length, accuracy_version: "tutor-expected-points-squared-v1" },
});

const games = [stored("2026.07.01", "1400", [move(0, "opening", []), move(1, "middlegame", ["fork"]), move(2, "endgame", [], .3)]), stored("2026.07.15", "1432", [move(0, "opening", [])])];
assert(ratingHistory(games, "Alex").map((point) => point.rating), [1400, 1432], "rating evidence order");
assert(ratingDelta(ratingHistory(games, "Alex")), 32, "thirty-day delta");
assert(buildExploreEntries(games).map((entry) => entry.section), ["Openings", "Middlegames", "Endgames"], "all Explore sections have evidence");
assert(reviewArc(games)[1]?.largestSwing, .3, "review arc uses largest expected-points swing");

console.log("insights tests passed");
