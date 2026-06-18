#pragma once

#include "pct/chess/pgn.hpp"
#include "pct/engine/stockfish.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

namespace pct::analysis {

enum class AnalysisStage { Parsing, ShallowScan, DeepAnalysis, Complete };
enum class GamePhase { Opening, Middlegame, Endgame };
enum class MoveQuality {
    Developing,
    Capture,
    Check,
    Recapture,
    Threat,
    Neutral,
    Inaccuracy,
    Mistake,
    Blunder,
};

struct Progress {
    AnalysisStage stage{AnalysisStage::Parsing};
    std::size_t complete{0};
    std::size_t total{0};
    std::string message;
};

struct MoveAssessment {
    std::size_t ply{0};
    std::string san;
    std::string fen_before;
    std::string fen_after;
    int evaluation_before{0};
    int evaluation_after{0};
    int loss{0};
    int material_delta{0};
    MoveQuality quality{MoveQuality::Neutral};
    GamePhase phase{GamePhase::Middlegame};
    std::string best_response;
};

struct Mistake {
    std::size_t rank{0};
    std::size_t ply{0};
    std::string san;
    std::string fen;
    int evaluation_before{0};
    int evaluation_after{0};
    int loss{0};
    GamePhase phase{GamePhase::Middlegame};
    std::string category;
    std::string explanation;
    std::string punishment;
    std::vector<std::string> better_moves;
    engine::AnalysisResult engine_details;
};

struct GameAnalysis {
    std::string game_id;
    std::vector<MoveAssessment> moves;
    std::vector<Mistake> mistakes;
};

struct AnalyzerOptions {
    int shallow_depth{10};
    int deep_depth{18};
    int candidate_threshold_cp{80};
    std::size_t max_deep_candidates{5};
    std::size_t top_mistakes{3};
};

using ProgressCallback = std::function<void(const Progress&)>;

class AnalysisCache {
  public:
    [[nodiscard]] bool get(const engine::AnalysisRequest& request,
                           engine::AnalysisResult& result) const;
    void put(const engine::AnalysisRequest& request, engine::AnalysisResult result);
    [[nodiscard]] std::size_t size() const;

  private:
    [[nodiscard]] static std::string key(const engine::AnalysisRequest& request);
    mutable std::mutex mutex_;
    std::map<std::string, engine::AnalysisResult> values_;
};

class Analyzer {
  public:
    Analyzer(engine::AnalysisEngine& engine, AnalysisCache& cache, AnalyzerOptions options = {});

    [[nodiscard]] GameAnalysis analyze(const chess::Game& game, ProgressCallback progress = {},
                                       std::stop_token stop_token = {});
    [[nodiscard]] static GamePhase classify_phase(const chess::Board& board, std::size_t ply);

  private:
    engine::AnalysisEngine& engine_;
    AnalysisCache& cache_;
    AnalyzerOptions options_;

    [[nodiscard]] engine::AnalysisResult analyze_cached(const engine::AnalysisRequest& request,
                                                        std::stop_token stop_token);
};

[[nodiscard]] std::string_view name(AnalysisStage stage);
[[nodiscard]] std::string_view name(GamePhase phase);
[[nodiscard]] std::string_view name(MoveQuality quality);

} // namespace pct::analysis
