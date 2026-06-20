#include "test.hpp"

#include "pct/chess/pgn.hpp"

using namespace pct::chess;

TEST_CASE("PGN parses tags comments variations and positions") {
    constexpr std::string_view pgn = R"pgn(
[Event "Example"]
[Site "https://www.chess.com/game/live/123"]
[Date "2026.06.17"]
[Round "-"]
[White "Alex"]
[Black "Morgan"]
[Result "0-1"]

1. e4 {King pawn} e5 2. Nf3 Nc6 (2... Nf6) 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 0-1
)pgn";
    const Game game = parse_pgn(pgn);
    CHECK_EQ(game.tag("White"), "Alex");
    CHECK_EQ(game.tag("Black"), "Morgan");
    CHECK_EQ(game.plies.size(), 10ULL);
    CHECK_EQ(game.plies.front().san, "e4");
    CHECK_EQ(game.plies.back().san, "Be7");
    CHECK(!game.identity.empty());
}

TEST_CASE("PGN identity is deterministic") {
    constexpr std::string_view pgn = R"pgn(
[White "A"]
[Black "B"]
[Result "1-0"]
1. e4 e5 2. Nf3 Nc6 1-0
)pgn";
    CHECK_EQ(parse_pgn(pgn).identity, parse_pgn(pgn).identity);
}

TEST_CASE("PGN rejects illegal move sequences") {
    CHECK_THROWS(parse_pgn("[Result \"*\"]\n\n1. e4 e5 2. Ke3 *"));
}

TEST_CASE("PGN stops at the game result") {
    const Game game = parse_pgn(
        "[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1. e4 e5 1-0 trailing page text");
    CHECK_EQ(game.plies.size(), 2ULL);
}

TEST_CASE("PGN extracts trustworthy clock and elapsed move annotations") {
    const auto game = parse_pgn(R"pgn(
[White "A"]
[Black "B"]
[Result "*"]
[TimeControl "600+5"]

1. e4 {[%clk 0:09:58.5]} e5 {[%clk 0:09:57]} 2. Nf3 {[%clk 0:09:55]} *
)pgn");
    CHECK_EQ(*game.plies[0].clock_ms, 598500LL);
    CHECK_EQ(*game.plies[0].elapsed_ms, 6500LL);
    CHECK_EQ(*game.plies[1].clock_ms, 597000LL);
    CHECK_EQ(*game.plies[1].elapsed_ms, 8000LL);
    CHECK_EQ(*game.plies[2].elapsed_ms, 8500LL);
}
