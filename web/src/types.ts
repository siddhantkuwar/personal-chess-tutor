export interface Ply {
  ply: number;
  san: string;
  uci: string;
  fen_before: string;
  fen_after: string;
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
  category: string;
  explanation: string;
  punishment: string;
  better_moves: string[];
  engine: {
    best_move: string;
    ponder_move: string;
    lines: EngineLine[];
  };
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
  analysis_status: "pending" | "complete";
  analysis: {
    game_id: string;
    moves: MoveAssessment[];
    mistakes: Mistake[];
  } | null;
  pgn?: string;
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
