#include "test.hpp"

#include "pct/chess/bitboard.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace pct::chess;

namespace {

std::vector<std::string> legal_uci(Board& board) {
    std::vector<std::string> result;
    for (const auto& move : board.legal_moves())
        result.push_back(uci(move));
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> legal_uci(BitboardBoard& board) {
    std::vector<std::string> result;
    for (const auto& move : board.legal_moves())
        result.push_back(uci(move));
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace

TEST_CASE("bitboard representation mirrors array pieces and metadata") {
    const std::string fen =
        "r3k2r/p1ppqpb1/bn2pnp1/2pP4/1p2P3/2N2N2/PPQBBPPP/R3K2R w KQkq - 0 1";
    const Board array = Board::from_fen(fen);
    const BitboardBoard bits = BitboardBoard::from_fen(fen);
    CHECK_EQ(bits.to_fen(), array.to_fen());
    CHECK_EQ(bits.hash(), array.hash());
    for (Square square = 0; square < 64; ++square)
        CHECK(bits.at(square) == array.at(square));
    CHECK_EQ(bits.material(Color::White), array.material(Color::White));
    CHECK_EQ(bits.material(Color::Black), array.material(Color::Black));
    int white_pieces = 0;
    int black_pieces = 0;
    for (Square square = 0; square < 64; ++square) {
        const Piece piece = array.at(square);
        if (!piece.empty())
            piece.color == Color::White ? ++white_pieces : ++black_pieces;
    }
    CHECK_EQ(std::popcount(bits.occupancy(Color::White)), white_pieces);
    CHECK_EQ(std::popcount(bits.occupancy(Color::Black)), black_pieces);
}

TEST_CASE("bitboard and array boards pass identical perft fixtures") {
    for (const auto& [fen, depth] : std::vector<std::pair<std::string, unsigned>>{
             {Board::initial().to_fen(), 3},
             {"r3k2r/p1ppqpb1/bn2pnp1/2pP4/1p2P3/2N2N2/PPQBBPPP/R3K2R w KQkq - 0 1", 3},
         }) {
        Board array = Board::from_fen(fen);
        BitboardBoard bits = BitboardBoard::from_fen(fen);
        CHECK_EQ(bits.perft(depth), array.perft(depth));
    }
}

TEST_CASE("bitboard and array boards stay equivalent through make and unmake") {
    Board array = Board::initial();
    BitboardBoard bits = BitboardBoard::initial();
    const Board initial_array = array;
    const BitboardBoard initial_bits = bits;
    std::vector<std::pair<Move, Undo>> array_history;
    std::vector<std::pair<Move, Undo>> bit_history;
    for (int ply = 0; ply < 40; ++ply) {
        CHECK(legal_uci(array) == legal_uci(bits));
        const auto moves = array.legal_moves();
        if (moves.empty())
            break;
        const Move move = moves[static_cast<std::size_t>((ply * 29 + 7) % moves.size())];
        const auto bit_move = bits.find_legal_move(move.from, move.to, move.promotion);
        CHECK(bit_move.has_value());
        array_history.emplace_back(move, array.make_move(move));
        bit_history.emplace_back(*bit_move, bits.make_move(*bit_move));
        CHECK_EQ(bits.to_fen(), array.to_fen());
        CHECK_EQ(bits.hash(), array.hash());
    }
    while (!array_history.empty()) {
        array.unmake_move(array_history.back().first, array_history.back().second);
        bits.unmake_move(bit_history.back().first, bit_history.back().second);
        array_history.pop_back();
        bit_history.pop_back();
        CHECK_EQ(bits.to_fen(), array.to_fen());
        CHECK_EQ(bits.hash(), array.hash());
    }
    CHECK(array == initial_array);
    CHECK(bits == initial_bits);
}
