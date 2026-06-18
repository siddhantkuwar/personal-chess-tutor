#include "test.hpp"

#include "pct/analysis/analyzer.hpp"

using namespace pct;

namespace {

class TacticalEngine final : public engine::AnalysisEngine {
  public:
    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   std::stop_token) override {
        chess::Board board = chess::Board::from_fen(request.fen);
        engine::AnalysisResult result;
        int score = 0;
        std::string best;
        for (const chess::Move& move : board.legal_moves()) {
            if (move.has(chess::Capture) && board.at(move.to).type == chess::PieceType::Queen) {
                score = 800;
                best = chess::uci(move);
                break;
            }
            if (best.empty())
                best = chess::uci(move);
        }
        result.best_move = best;
        for (int multipv = 1; multipv <= request.multipv; ++multipv) {
            result.lines.push_back(engine::PrincipalVariation{
                multipv, request.depth, score - (multipv - 1) * 10, std::nullopt, 1000, 5, {best}});
        }
        return result;
    }
};

} // namespace

TEST_CASE("analysis identifies a hanging queen with evidence") {
    const chess::Game game = chess::parse_pgn(R"pgn(
[White "Alex"]
[Black "Morgan"]
[Result "0-1"]
1. e4 e5 2. Qh5 Nc6 3. Qxe5+ Nxe5 0-1
)pgn");
    TacticalEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{4, 6, 80, 5, 3});
    std::vector<analysis::Progress> progress;
    const analysis::GameAnalysis result = analyzer.analyze(
        game, [&](const analysis::Progress& update) { progress.push_back(update); });
    CHECK_EQ(result.moves.size(), game.plies.size());
    CHECK(!result.mistakes.empty());
    CHECK_EQ(result.mistakes.front().category, "Hanging queen");
    CHECK(result.mistakes.front().loss >= 700);
    CHECK(!result.mistakes.front().punishment.empty());
    CHECK(progress.back().stage == analysis::AnalysisStage::Complete);
    CHECK(cache.size() > game.plies.size());
}

TEST_CASE("phase classification uses position state") {
    CHECK(analysis::Analyzer::classify_phase(chess::Board::initial(), 0) ==
          analysis::GamePhase::Opening);
    const auto endgame = chess::Board::from_fen("8/8/4k3/8/8/4K3/3P4/8 w - - 0 1");
    CHECK(analysis::Analyzer::classify_phase(endgame, 50) == analysis::GamePhase::Endgame);
}

TEST_CASE("analysis cache keys include engine settings") {
    analysis::AnalysisCache cache;
    engine::AnalysisRequest shallow{chess::Board::initial().to_fen(), 8,
                                    std::chrono::milliseconds(0), 1};
    engine::AnalysisRequest deep = shallow;
    deep.depth = 18;
    cache.put(shallow, engine::AnalysisResult{{}, "e2e4", {}});
    engine::AnalysisResult found;
    CHECK(cache.get(shallow, found));
    CHECK(!cache.get(deep, found));
}

TEST_CASE("analysis cache ignores FEN move clocks") {
    analysis::AnalysisCache cache;
    engine::AnalysisRequest first{chess::Board::initial().to_fen(), 8, std::chrono::milliseconds(0),
                                  1};
    engine::AnalysisRequest same_position = first;
    same_position.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 14 27";
    cache.put(first, engine::AnalysisResult{{}, "e2e4", {}});
    engine::AnalysisResult found;
    CHECK(cache.get(same_position, found));
}
