#pragma once

#include "pct/chess/board.hpp"
#include "pct/chess/board_model.hpp"

#include <array>
#include <cstdint>

namespace pct::chess {

class BitboardBoard {
  public:
    BitboardBoard();

    [[nodiscard]] static BitboardBoard initial();
    [[nodiscard]] static BitboardBoard from_fen(std::string_view fen);
    [[nodiscard]] std::string to_fen() const { return board_.to_fen(); }
    [[nodiscard]] Piece at(Square square) const;
    [[nodiscard]] Color side_to_move() const { return board_.side_to_move(); }
    [[nodiscard]] Square en_passant_square() const { return board_.en_passant_square(); }
    [[nodiscard]] std::uint8_t castling_rights() const { return board_.castling_rights(); }
    [[nodiscard]] std::uint16_t halfmove_clock() const { return board_.halfmove_clock(); }
    [[nodiscard]] std::uint16_t fullmove_number() const { return board_.fullmove_number(); }
    [[nodiscard]] std::uint64_t hash() const { return board_.hash(); }
    [[nodiscard]] std::size_t repetition_count() const { return board_.repetition_count(); }
    [[nodiscard]] bool is_threefold_repetition() const {
        return board_.is_threefold_repetition();
    }
    [[nodiscard]] bool is_square_attacked(Square square, Color by) const {
        return board_.is_square_attacked(square, by);
    }
    [[nodiscard]] bool in_check(Color color) const { return board_.in_check(color); }
    [[nodiscard]] std::vector<Move> pseudo_legal_moves() const {
        return board_.pseudo_legal_moves();
    }
    [[nodiscard]] std::vector<Move> legal_moves() { return board_.legal_moves(); }
    [[nodiscard]] std::optional<Move> find_legal_move(
        Square from, Square to, PieceType promotion = PieceType::Queen) {
        return board_.find_legal_move(from, to, promotion);
    }

    Undo make_move(const Move& move);
    void unmake_move(const Move& move, const Undo& undo);
    [[nodiscard]] std::uint64_t perft(unsigned depth) { return board_.perft(depth); }
    [[nodiscard]] PerftStats perft_stats(unsigned depth) { return board_.perft_stats(depth); }
    [[nodiscard]] int material(Color color) const;
    [[nodiscard]] std::uint64_t occupancy(Color color) const;
    [[nodiscard]] std::uint64_t pieces(Color color, PieceType type) const;

    friend bool operator==(const BitboardBoard&, const BitboardBoard&) = default;

  private:
    Board board_;
    std::array<std::uint64_t, 12> pieces_{};

    explicit BitboardBoard(Board board);
    void rebuild();
    [[nodiscard]] static std::size_t index(Color color, PieceType type);
};

static_assert(BoardModel<BitboardBoard>);

} // namespace pct::chess
