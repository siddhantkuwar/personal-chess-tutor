#include "pct/chess/bitboard.hpp"

#include "pct/common/error.hpp"

#include <bit>

namespace pct::chess {

BitboardBoard::BitboardBoard() {
    rebuild();
}

BitboardBoard::BitboardBoard(Board board) : board_(std::move(board)) {
    rebuild();
}

BitboardBoard BitboardBoard::initial() {
    return BitboardBoard(Board::initial());
}

BitboardBoard BitboardBoard::from_fen(std::string_view fen) {
    return BitboardBoard(Board::from_fen(fen));
}

std::size_t BitboardBoard::index(Color color, PieceType type) {
    if (type == PieceType::None)
        throw Error(ErrorCode::InvalidArgument, "empty pieces do not have a bitboard");
    const std::size_t color_offset = color == Color::White ? 0 : 6;
    return color_offset + static_cast<std::size_t>(type) - 1;
}

void BitboardBoard::rebuild() {
    pieces_.fill(0);
    for (Square square = 0; square < 64; ++square) {
        const Piece piece = board_.at(square);
        if (!piece.empty())
            pieces_[index(piece.color, piece.type)] |= std::uint64_t{1} << square;
    }
}

Piece BitboardBoard::at(Square square) const {
    if (square >= 64)
        throw Error(ErrorCode::InvalidArgument, "square is outside the board");
    const std::uint64_t mask = std::uint64_t{1} << square;
    for (const Color color : {Color::White, Color::Black}) {
        for (const PieceType type : {PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
                                     PieceType::Rook, PieceType::Queen, PieceType::King}) {
            if ((pieces_[index(color, type)] & mask) != 0)
                return Piece{color, type};
        }
    }
    return {};
}

Undo BitboardBoard::make_move(const Move& move) {
    Undo undo = board_.make_move(move);
    rebuild();
    return undo;
}

void BitboardBoard::unmake_move(const Move& move, const Undo& undo) {
    board_.unmake_move(move, undo);
    rebuild();
}

std::uint64_t BitboardBoard::pieces(Color color, PieceType type) const {
    return pieces_[index(color, type)];
}

std::uint64_t BitboardBoard::occupancy(Color color) const {
    std::uint64_t result = 0;
    for (const PieceType type : {PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
                                 PieceType::Rook, PieceType::Queen, PieceType::King})
        result |= pieces_[index(color, type)];
    return result;
}

int BitboardBoard::material(Color color) const {
    int result = 0;
    for (const auto [type, value] : {
             std::pair{PieceType::Pawn, 100}, std::pair{PieceType::Knight, 320},
             std::pair{PieceType::Bishop, 330}, std::pair{PieceType::Rook, 500},
             std::pair{PieceType::Queen, 900}}) {
        result += static_cast<int>(std::popcount(pieces_[index(color, type)])) * value;
    }
    return result;
}

} // namespace pct::chess
