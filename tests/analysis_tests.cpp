#include "test.hpp"

#include "pct/analysis/analyzer.hpp"

using namespace pct;

namespace {

class TacticalEngine final : public engine::AnalysisEngine {
  public:
    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   CancellationToken) override {
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

chess::Game position_game(std::string_view fen, std::string_view uci, std::size_t ply = 0) {
    chess::Board board = chess::Board::from_fen(fen);
    const auto move = board.find_legal_move(chess::parse_square(uci.substr(0, 2)),
                                            chess::parse_square(uci.substr(2, 2)));
    CHECK(move.has_value());
    const std::string before = board.to_fen();
    board.make_move(*move);
    chess::Game game;
    game.plies.resize(ply + 1);
    game.plies[ply] = chess::Ply{*move, std::string(uci), before, board.to_fen(), {}, {}};
    return game;
}

engine::AnalysisResult best_result(std::string move, std::optional<int> mate = {}) {
    engine::AnalysisResult result;
    result.best_move = move;
    result.lines.push_back(
        engine::PrincipalVariation{1, 18, 0, mate, 1000, 5, {std::move(move)}});
    return result;
}

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
    CHECK(!result.mistakes.front().evidence.empty());
    CHECK_EQ(result.mistakes.front().classifier_version, "taxonomy-2");
    CHECK(progress.back().stage == analysis::AnalysisStage::Complete);
    CHECK(cache.size() > game.plies.size());
}

TEST_CASE("analysis emits time-management categories only from clock evidence") {
    const chess::Game game = chess::parse_pgn(R"pgn(
[White "Alex"]
[Black "Morgan"]
[Result "0-1"]
[TimeControl "600+0"]
1. e4 {[%clk 0:09:59]} e5 {[%clk 0:09:59]} 2. Qh5 {[%clk 0:09:58]}
Nc6 {[%clk 0:09:58]} 3. Qxe5+ {[%clk 0:09:57.5]} Nxe5 {[%clk 0:09:57]} 0-1
)pgn");
    TacticalEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{4, 6, 80, 5, 3});
    const auto result = analyzer.analyze(game);
    CHECK(!result.mistakes.empty());
    CHECK_EQ(result.mistakes.front().category, "Instant-move blunder");
}

TEST_CASE("opening recognition reports ECO name and departure from local book") {
    const auto italian = chess::parse_pgn(
        "[Result \"*\"]\n\n1. e4 e5 2. Nf3 Nc6 3. Bc4 Bc5 *");
    const auto match = analysis::recognize_opening(italian);
    CHECK_EQ(match.eco, "C50");
    CHECK_EQ(match.name, "Italian Game");
    CHECK_EQ(match.book_ply, 6ULL);
    CHECK_EQ(match.book_version, "2026.1");
    CHECK(!match.departure_ply.has_value());
    const auto departed = chess::parse_pgn(
        "[Result \"*\"]\n\n1. e4 e5 2. Nf3 Nc6 3. Bc4 Nf6 *");
    CHECK_EQ(*analysis::recognize_opening(departed).departure_ply, 5ULL);
}

TEST_CASE("tactical motif classifier proves fork pin skewer discovery and back rank geometry") {
    const auto classify = [](std::string_view fen, std::string_view from, std::string_view to) {
        chess::Board board = chess::Board::from_fen(fen);
        const auto move = board.find_legal_move(chess::parse_square(from), chess::parse_square(to));
        CHECK(move.has_value());
        return analysis::classify_tactical_motif(board, *move);
    };
    CHECK_EQ(classify("1q1r2k1/8/8/4N3/8/8/8/6K1 w - - 0 1", "e5", "c6"), "Fork");
    CHECK_EQ(classify("k7/8/b7/8/8/8/8/R5K1 w - - 0 1", "a1", "a5"), "Pin");
    CHECK_EQ(classify("r6k/8/q7/8/8/8/8/R5K1 w - - 0 1", "a1", "a5"), "Skewer");
    CHECK_EQ(classify("4q2k/8/8/8/8/8/4B3/4R1K1 w - - 0 1", "e2", "d3"),
             "Discovered attack");
    CHECK_EQ(classify("7k/8/8/8/8/8/8/R5K1 w - - 0 1", "a1", "a8"),
             "Back-rank weakness");
    CHECK_EQ(classify("q6k/8/b7/8/8/8/8/R5K1 w - - 0 1", "a1", "a6"),
             "Removal of defender");
    CHECK_EQ(classify("6k1/1q5r/b7/8/8/8/8/R5KR w - - 0 1", "a1", "a6"),
             "Overloaded defender");
    CHECK_EQ(classify("7k/8/8/8/8/8/8/R5K1 w - - 0 1", "a1", "a2"), "");
}

TEST_CASE("mistake classifier fixtures cover time capture mate and strategic rules") {
    const engine::AnalysisResult none;
    const auto classify = [&](const chess::Game& game, std::size_t ply,
                              const engine::AnalysisResult& before = {},
                              const engine::AnalysisResult& after = {}) {
        return analysis::classify_mistake_category(game, ply, before, after);
    };
    const auto neutral = position_game(chess::Board::initial().to_fen(), "e2e4");
    CHECK_EQ(classify(neutral, 0), "One-move tactical loss");
    CHECK_THROWS(analysis::classify_mistake_category(neutral, 1, none, none));

    auto instant = neutral;
    instant.plies[0].elapsed_ms = 2'000;
    CHECK_EQ(classify(instant, 0), "Instant-move blunder");
    instant.plies[0].elapsed_ms = 2'001;
    CHECK(classify(instant, 0) != "Instant-move blunder");

    auto excessive = neutral;
    excessive.plies[0].elapsed_ms = 120'000;
    CHECK_EQ(classify(excessive, 0), "Excessive early time use");
    excessive.plies[0].elapsed_ms = 119'999;
    CHECK(classify(excessive, 0) != "Excessive early time use");

    auto low_time = neutral;
    low_time.plies[0].clock_ms = 30'000;
    CHECK_EQ(classify(low_time, 0), "Time-management failure");
    low_time.plies[0].clock_ms = 30'001;
    CHECK(classify(low_time, 0) != "Time-management failure");

    CHECK_EQ(classify(neutral, 0, none, best_result("", 1)),
             "Failed response to mate threat");
    CHECK(classify(neutral, 0, none, best_result("", -1)) !=
          "Failed response to mate threat");

    const auto hanging_queen =
        position_game("4k3/8/8/8/8/8/r6Q/4K3 w - - 0 1", "e1f1");
    CHECK_EQ(classify(hanging_queen, 0, none, best_result("a2h2")), "Hanging queen");
    CHECK(classify(hanging_queen, 0) != "Hanging queen");
    const auto hanging_piece =
        position_game("4k3/8/8/8/8/8/r6B/4K3 w - - 0 1", "e1f1");
    CHECK_EQ(classify(hanging_piece, 0, none, best_result("a2h2")), "Hanging piece");
    CHECK(classify(hanging_piece, 0) != "Hanging piece");
    const auto ignored_attack =
        position_game("4k3/8/8/8/8/8/r6P/4K3 w - - 0 1", "e1f1");
    CHECK_EQ(classify(ignored_attack, 0, none, best_result("a2h2")), "Ignored attack");
    CHECK(classify(ignored_attack, 0) != "Ignored attack");

    auto failed_recapture =
        position_game("4k3/8/8/8/4p3/3P4/8/4K3 w - - 0 1", "e1f1", 1);
    failed_recapture.plies[0].move = chess::Move{chess::parse_square("e5"),
                                                 chess::parse_square("e4"),
                                                 chess::PieceType::None, chess::Capture};
    CHECK_EQ(classify(failed_recapture, 1), "Failed recapture");
    failed_recapture.plies[0].move.flags = chess::Quiet;
    CHECK(classify(failed_recapture, 1) != "Failed recapture");

    const auto missed_capture =
        position_game("q7/4k3/8/8/8/8/8/R3K3 w - - 0 1", "e1f1");
    CHECK_EQ(classify(missed_capture, 0, best_result("a1a8")), "Missed free capture");
    CHECK(classify(missed_capture, 0) != "Missed free capture");
    const auto missed_mate =
        position_game("8/8/7k/5K2/6Q1/8/8/8 w - - 0 1", "g4f3");
    CHECK_EQ(classify(missed_mate, 0, best_result("g4g6")), "Missed mate");
    CHECK(classify(missed_mate, 0) != "Missed mate");
    const auto missed_check =
        position_game("8/3k4/8/8/8/8/8/4R1K1 w - - 0 1", "g1f1");
    CHECK_EQ(classify(missed_check, 0, best_result("e1e7")), "Missed check");
    CHECK(classify(missed_check, 0) != "Missed check");

    const auto passed_pawn =
        position_game("8/8/8/8/8/4p3/8/4K2k w - - 0 1", "e1f1", 50);
    CHECK_EQ(classify(passed_pawn, 50), "Ignored passed pawn");
    const auto no_passed_pawn =
        position_game("8/8/8/8/8/4p3/3P4/4K2k w - - 0 1", "e1f1", 50);
    CHECK(classify(no_passed_pawn, 50) != "Ignored passed pawn");
    const auto inactive_king =
        position_game("8/8/8/8/4K3/8/8/7k w - - 0 1", "e4e3", 50);
    CHECK_EQ(classify(inactive_king, 50), "Incorrect king activity");
    const auto active_king =
        position_game("8/8/8/8/8/4K3/8/7k w - - 0 1", "e3e4", 50);
    CHECK(classify(active_king, 50) != "Incorrect king activity");

    const auto open_king = position_game(
        "r2qk2r/pppppppp/2n2n2/2b1b3/8/8/5PPP/R2QK1NR w KQkq - 0 7", "g1f3", 12);
    CHECK_EQ(classify(open_king, 12), "Open king position");
    CHECK(classify(neutral, 0) != "Open king position");
    const auto early_queen = position_game(
        "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", "d1h5", 2);
    CHECK_EQ(classify(early_queen, 2), "Premature queen development");
    CHECK(classify(neutral, 0) != "Premature queen development");
    const auto flank_pawn = position_game(chess::Board::initial().to_fen(), "a2a3");
    CHECK_EQ(classify(flank_pawn, 0), "Unnecessary flank-pawn moves");
    CHECK(classify(neutral, 0) != "Unnecessary flank-pawn moves");

    auto repeated = position_game(
        "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 2", "f3h4", 2);
    repeated.plies[0].move = chess::Move{chess::parse_square("g1"), chess::parse_square("f3")};
    CHECK_EQ(classify(repeated, 2), "Repeated piece movement");
    repeated.plies[0].move = {};
    CHECK(classify(repeated, 2) != "Repeated piece movement");

    const auto delayed_development =
        position_game(chess::Board::initial().to_fen(), "e2e4", 14);
    CHECK_EQ(classify(delayed_development, 14), "Delayed development");
    CHECK(classify(neutral, 0) != "Delayed development");
    const auto delayed_castling = position_game(
        "r2qk2r/pppppppp/2n2n2/2b2b2/2B1B3/2N2N2/PPPPPPPP/R2QK2R w KQkq - 0 9", "e2e3", 16);
    CHECK_EQ(classify(delayed_castling, 16), "Delayed castling");
    CHECK(classify(neutral, 0) != "Delayed castling");
}

TEST_CASE("phase classification uses position state") {
    CHECK(analysis::Analyzer::classify_phase(chess::Board::initial(), 0) ==
          analysis::GamePhase::Opening);
    const auto endgame = chess::Board::from_fen("8/8/4k3/8/8/4K3/3P4/8 w - - 0 1");
    CHECK(analysis::Analyzer::classify_phase(endgame, 50) == analysis::GamePhase::Endgame);
    const auto developed_uncastled = chess::Board::from_fen(
        "r2qk2r/pp3ppp/2n1bn2/2ppp3/2PPP3/2N1BN2/PP3PPP/R2QK2R w - - 0 12");
    CHECK(analysis::Analyzer::classify_phase(developed_uncastled, 22) ==
          analysis::GamePhase::Middlegame);
    const auto simplified = chess::Board::from_fen(
        "4k3/pp3ppp/8/8/8/8/PP3PPP/4K3 w - - 0 20");
    CHECK(analysis::Analyzer::classify_phase(simplified, 40) ==
          analysis::GamePhase::Endgame);
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
    CHECK_EQ(cache.hit_count(), 1ULL);
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

TEST_CASE("split shallow and deep analysis matches the combined contract") {
    const auto game = chess::parse_pgn(
        "[White \"A\"]\n[Black \"B\"]\n[Result \"0-1\"]\n\n1. e4 e5 2. Qh5 Nc6 3. Qxe5+ Nxe5 0-1");
    TacticalEngine split_engine;
    analysis::AnalysisCache split_cache;
    analysis::Analyzer split(split_engine, split_cache,
                             analysis::AnalyzerOptions{4, 6, 80, 5, 3});
    const auto shallow = split.analyze_shallow(game);
    CHECK_EQ(shallow.opening_book_version, "2026.1");
    CHECK_EQ(shallow.moves.size(), game.plies.size());
    CHECK(shallow.mistakes.empty());
    const auto staged = split.analyze_deep(game, shallow);

    TacticalEngine combined_engine;
    analysis::AnalysisCache combined_cache;
    analysis::Analyzer combined(combined_engine, combined_cache,
                                analysis::AnalyzerOptions{4, 6, 80, 5, 3});
    const auto direct = combined.analyze(game);
    CHECK_EQ(staged.moves.size(), direct.moves.size());
    CHECK_EQ(staged.mistakes.size(), direct.mistakes.size());
    CHECK_EQ(staged.mistakes.front().category, direct.mistakes.front().category);
    auto mismatched = shallow;
    mismatched.game_id = "wrong";
    CHECK_THROWS(split.analyze_deep(game, std::move(mismatched)));
}
