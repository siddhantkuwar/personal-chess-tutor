#pragma once

#include "pct/chess/pgn.hpp"
#include "pct/engine/stockfish.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace pct::analysis {

inline constexpr std::string_view opening_book_version = "2026.1";

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
    std::vector<std::string> evidence;
    std::string confidence{"proven"};
    std::string classifier_version{"taxonomy-2"};
};

struct GameAnalysis {
    std::string game_id;
    std::vector<MoveAssessment> moves;
    std::vector<Mistake> mistakes;
    std::string eco{"A00"};
    std::string opening{"Uncommon Opening"};
    std::size_t book_ply{0};
    std::optional<std::size_t> departure_ply;
    std::string opening_book_version{std::string(::pct::analysis::opening_book_version)};
};

struct OpeningMatch {
    std::string eco;
    std::string name;
    std::size_t book_ply{0};
    std::optional<std::size_t> departure_ply;
    std::string book_version{std::string(opening_book_version)};
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
    [[nodiscard]] std::size_t hit_count() const;

  private:
    [[nodiscard]] static std::string key(const engine::AnalysisRequest& request);
    mutable std::mutex mutex_;
    std::map<std::string, engine::AnalysisResult> values_;
    mutable std::size_t hits_{0};
};

class Analyzer {
  public:
    Analyzer(engine::AnalysisEngine& engine, AnalysisCache& cache, AnalyzerOptions options = {});

    [[nodiscard]] GameAnalysis analyze(const chess::Game& game, ProgressCallback progress = {},
                                       CancellationToken stop_token = {});
    [[nodiscard]] GameAnalysis analyze_shallow(const chess::Game& game,
                                               ProgressCallback progress = {},
                                               CancellationToken stop_token = {});
    [[nodiscard]] GameAnalysis analyze_deep(const chess::Game& game,
                                            GameAnalysis shallow_analysis,
                                            ProgressCallback progress = {},
                                            CancellationToken stop_token = {});
    [[nodiscard]] static GamePhase classify_phase(const chess::Board& board, std::size_t ply);
    [[nodiscard]] std::size_t cache_hits() const { return cache_.hit_count(); }

  private:
    engine::AnalysisEngine& engine_;
    AnalysisCache& cache_;
    AnalyzerOptions options_;

    [[nodiscard]] engine::AnalysisResult analyze_cached(const engine::AnalysisRequest& request,
                                                        CancellationToken stop_token);
};

[[nodiscard]] std::string_view name(AnalysisStage stage);
[[nodiscard]] std::string_view name(GamePhase phase);
[[nodiscard]] std::string_view name(MoveQuality quality);
[[nodiscard]] OpeningMatch recognize_opening(const chess::Game& game);
[[nodiscard]] std::string classify_tactical_motif(chess::Board board,
                                                  const chess::Move& best_move);
[[nodiscard]] std::string
classify_mistake_category(const chess::Game& game, std::size_t ply,
                          const engine::AnalysisResult& best_before,
                          const engine::AnalysisResult& best_after);

} // namespace pct::analysis
