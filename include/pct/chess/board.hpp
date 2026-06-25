#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pct::chess {

using Square = std::uint8_t;
constexpr Square no_square = 64;

enum class Color : std::uint8_t { White, Black };
enum class PieceType : std::uint8_t { None, Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    Color color{Color::White};
    PieceType type{PieceType::None};

    [[nodiscard]] constexpr bool empty() const {
        return type == PieceType::None;
    }
    friend constexpr bool operator==(Piece, Piece) = default;
};

enum MoveFlag : std::uint8_t {
    Quiet = 0,
    Capture = 1 << 0,
    DoublePawnPush = 1 << 1,
    KingCastle = 1 << 2,
    QueenCastle = 1 << 3,
    EnPassant = 1 << 4,
    Promotion = 1 << 5,
};

struct Move {
    Square from{no_square};
    Square to{no_square};
    PieceType promotion{PieceType::None};
    std::uint8_t flags{Quiet};

    [[nodiscard]] constexpr bool has(MoveFlag flag) const {
        return (flags & static_cast<std::uint8_t>(flag)) != 0;
    }
    friend constexpr bool operator==(Move, Move) = default;
};

struct Undo {
    Piece captured{};
    Square captured_square{no_square};
    Square en_passant{no_square};
    std::uint8_t castling_rights{0};
    std::uint16_t halfmove_clock{0};
    std::uint16_t fullmove_number{1};
    std::uint64_t hash{0};
};

struct PerftStats {
    std::uint64_t nodes{0};
    std::uint64_t captures{0};
    std::uint64_t en_passant{0};
    std::uint64_t castles{0};
    std::uint64_t promotions{0};
    std::uint64_t checks{0};
    std::uint64_t checkmates{0};
};

class Board {
  public:
    Board();

    [[nodiscard]] static Board initial();
    [[nodiscard]] static Board from_fen(std::string_view fen);
    [[nodiscard]] std::string to_fen() const;

    [[nodiscard]] Piece at(Square square) const;
    [[nodiscard]] Color side_to_move() const {
        return side_to_move_;
    }
    [[nodiscard]] Square en_passant_square() const {
        return en_passant_;
    }
    [[nodiscard]] std::uint8_t castling_rights() const {
        return castling_rights_;
    }
    [[nodiscard]] std::uint16_t halfmove_clock() const {
        return halfmove_clock_;
    }
    [[nodiscard]] std::uint16_t fullmove_number() const {
        return fullmove_number_;
    }
    [[nodiscard]] std::uint64_t hash() const {
        return hash_;
    }
    [[nodiscard]] bool hash_is_consistent() const { return hash_ == compute_hash(); }
    [[nodiscard]] std::size_t repetition_count() const;
    [[nodiscard]] bool is_threefold_repetition() const {
        return repetition_count() >= 3;
    }

    [[nodiscard]] bool is_square_attacked(Square square, Color by) const;
    [[nodiscard]] bool in_check(Color color) const;
    [[nodiscard]] std::vector<Move> pseudo_legal_moves() const;
    [[nodiscard]] std::vector<Move> legal_moves();
    [[nodiscard]] std::optional<Move> find_legal_move(Square from, Square to,
                                                      PieceType promotion = PieceType::Queen);

    Undo make_move(const Move& move);
    void unmake_move(const Move& move, const Undo& undo);

    [[nodiscard]] std::uint64_t perft(unsigned depth);
    [[nodiscard]] PerftStats perft_stats(unsigned depth);
    [[nodiscard]] int material(Color color) const;

    friend bool operator==(const Board&, const Board&) = default;

  private:
    std::array<Piece, 64> squares_{};
    Color side_to_move_{Color::White};
    std::uint8_t castling_rights_{0};
    Square en_passant_{no_square};
    std::uint16_t halfmove_clock_{0};
    std::uint16_t fullmove_number_{1};
    std::uint64_t hash_{0};
    std::vector<std::uint64_t> repetition_history_;

    void add_pawn_moves(std::vector<Move>& moves, Square from, Piece piece) const;
    void add_knight_moves(std::vector<Move>& moves, Square from, Piece piece) const;
    void add_sliding_moves(std::vector<Move>& moves, Square from, Piece piece,
                           const int* directions, std::size_t count) const;
    void add_king_moves(std::vector<Move>& moves, Square from, Piece piece) const;
    [[nodiscard]] Square king_square(Color color) const;
    [[nodiscard]] std::uint64_t compute_hash() const;
};

[[nodiscard]] constexpr Color opposite(Color color) {
    return color == Color::White ? Color::Black : Color::White;
}

[[nodiscard]] std::string square_name(Square square);
[[nodiscard]] Square parse_square(std::string_view name);
[[nodiscard]] char piece_symbol(Piece piece);
[[nodiscard]] Piece piece_from_symbol(char symbol);
[[nodiscard]] std::string uci(const Move& move);

} // namespace pct::chess
