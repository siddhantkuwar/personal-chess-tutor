#include "test.hpp"

#include "pct/chess/board.hpp"
#include "pct/engine/stockfish.hpp"

using namespace pct::engine;

TEST_CASE("UCI info parser reads score and principal variation") {
    const auto line = Stockfish::parse_info_line(
        "info depth 18 multipv 2 score cp -210 nodes 1200000 time 812 pv c6b4 f3e5");
    CHECK(line.has_value());
    CHECK_EQ(line->depth, 18);
    CHECK_EQ(line->multipv, 2);
    CHECK_EQ(*line->centipawns, -210);
    CHECK_EQ(line->nodes, 1200000ULL);
    CHECK_EQ(line->moves.size(), 2ULL);
}

TEST_CASE("persistent UCI process completes MultiPV analysis") {
    Stockfish engine(StockfishOptions{PCT_FAKE_STOCKFISH_PATH, 16, 1});
    AnalysisRequest request;
    request.fen = pct::chess::Board::initial().to_fen();
    request.depth = 8;
    request.multipv = 2;
    request.timeout = std::chrono::seconds(2);
    const AnalysisResult result = engine.analyze(request);
    CHECK_EQ(result.best_move, "e2e4");
    CHECK_EQ(result.ponder_move, "e7e5");
    CHECK_EQ(result.lines.size(), 2ULL);
    CHECK(engine.running());
}

TEST_CASE("Stockfish executable resolver accepts explicit paths and reports missing engines") {
    CHECK_EQ(Stockfish::resolve_executable(PCT_FAKE_STOCKFISH_PATH), PCT_FAKE_STOCKFISH_PATH);
    CHECK_THROWS(Stockfish::resolve_executable("/definitely/missing/stockfish"));
}

TEST_CASE("UCI timeout sends stop and leaves the engine reusable") {
    Stockfish engine(StockfishOptions{PCT_SLOW_STOCKFISH_PATH, 16, 1});
    AnalysisRequest request;
    request.fen = pct::chess::Board::initial().to_fen();
    request.depth = 8;
    request.timeout = std::chrono::milliseconds(40);
    CHECK_THROWS(engine.analyze(request));
    CHECK(engine.running());
}

TEST_CASE("UCI crash restarts an isolated engine process") {
    Stockfish engine(StockfishOptions{PCT_CRASH_STOCKFISH_PATH, 16, 1});
    AnalysisRequest request;
    request.fen = pct::chess::Board::initial().to_fen();
    request.depth = 8;
    request.timeout = std::chrono::seconds(1);
    CHECK_THROWS(engine.analyze(request));
    CHECK(engine.running());
}
