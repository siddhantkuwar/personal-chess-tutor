export interface Ply {
  ply: number;
  san: string;
  uci: string;
  fen_before: string;
  fen_after: string;
  clock_ms: number | null;
  elapsed_ms: number | null;
}

export interface GameData {
  id: string;
  tags: Record<string, string>;
  plies: Ply[];
}

export interface EngineLine {
  multipv: number;
  depth: number;
  centipawns: number | null;
  mate: number | null;
  nodes: number;
  time_ms: number;
  moves: string[];
}

export interface Mistake {
  rank: number;
  ply: number;
  san: string;
  fen: string;
  evaluation_before: number;
  evaluation_after: number;
  loss: number;
  phase: string;
  opening: string;
  category: string;
  explanation: string;
  punishment: string;
  better_moves: string[];
  engine: {
    best_move: string;
    ponder_move: string;
    lines: EngineLine[];
  };
  evidence: string[];
  confidence: "proven" | "suggestive";
  classifier_version: string;
}

export interface MoveAssessment {
  ply: number;
  san: string;
  fen_before: string;
  fen_after: string;
  evaluation_before: number;
  evaluation_after: number;
  loss: number;
  quality: string;
  phase: string;
  best_response: string;
}

export interface StoredGame {
  game: GameData;
  source_url: string;
  import_method: string;
  analysis_status: "pending" | "shallow" | "complete";
  analysis: {
    game_id: string;
    moves: MoveAssessment[];
    mistakes: Mistake[];
    eco: string;
    opening: string;
    book_ply: number;
    departure_ply: number | null;
    opening_book_version: string;
  } | null;
  pgn?: string;
}

export interface DrillAttempt {
  id: number;
  attempted_at_ms: number;
  correct: boolean;
  move: string;
  response_time_ms: number;
  hint_level: number;
  retries: number;
}

export interface Drill {
  id: string;
  source_game_id: string;
  source_ply: number;
  fen: string;
  category: string;
  phase: string;
  explanation: string;
  punishment: string;
  solutions: string[];
  difficulty: number;
  impact_cp: number;
  attempts: DrillAttempt[];
  played_move: string;
  fen_after_mistake: string;
  fen_after_punishment: string;
  session_hint_level: number;
  session_started_at_ms: number;
  hint_level: number;
  available_hint_level: number;
  changed_threat: string;
  attacked_pieces: string[];
  opponent_response: string;
  schedule: {
    state: "new" | "due" | "upcoming" | "mastered";
    next_review_ms: number;
    success_rate: number;
    retention: number;
    priority: number;
  };
}

export interface Weakness {
  category: string;
  occurrences: number;
  games: number;
  attempts: number;
  correct: number;
  occurrences_7_days: number;
  occurrences_30_days: number;
  drill_accuracy: number;
  average_loss_cp: number;
  recurrence_rate: number;
  repeated_interval_days: number | null;
  phases: Record<string, number>;
}

export interface Profile {
  projection_version: string;
  player_name: string;
  latest_rating: number;
  rating_observations: number;
  games_imported: number;
  games_analyzed: number;
  games_shallow_analyzed: number;
  games_analyzed_7_days: number;
  games_analyzed_30_days: number;
  total_mistakes: number;
  total_positions: number;
  drill_attempts: number;
  drill_correct: number;
  retention_reviews: number;
  retained_reviews: number;
  analysis_completion_rate: number;
  drill_accuracy: number;
  retention_rate: number;
  average_centipawn_loss: number;
  weaknesses: Weakness[];
  openings: Array<{ eco: string; name: string; games: number; mistakes: number; average_centipawn_loss: number }>;
  activity_trend: Array<{ day_start_ms: number; games_analyzed: number; mistakes: number; drill_attempts: number; drill_correct: number }>;
  endgame_conversion: RateMetric;
  king_safety_violations: RateMetric;
  time_management_failures: RateMetric;
}

export interface RateMetric {
  numerator: number;
  denominator: number;
  rate: number | null;
  statistically_meaningful: boolean;
}

export interface ResourceRecommendation {
  id: string;
  title: string;
  kind: string;
  locator: string;
  phase: string;
  opening: string;
  evidence: string;
  completed: boolean;
}

export interface BatchProgress {
  id: string;
  discovered: number;
  imported: number;
  duplicates: number;
  failed: number;
  queued: number;
  completed: number;
  job_failures: number;
  remaining: number;
  paused: boolean;
  positions_analyzed: number;
  positions_remaining: number;
}

export interface Job {
  id: number;
  game_id: string;
  status: "queued" | "running" | "complete" | "failed" | "cancelled";
  progress: {
    stage: "parsing" | "shallow_scan" | "deep_analysis" | "complete";
    complete: number;
    total: number;
    message: string;
  };
  error: string;
}
