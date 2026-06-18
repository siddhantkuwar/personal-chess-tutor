#include "pct/chess/san.hpp"

#include "pct/common/error.hpp"

#include <algorithm>
#include <cctype>

namespace pct::chess {
namespace {

char san_piece(PieceType type) {
    switch (type) {
    case PieceType::Knight:
        return 'N';
    case PieceType::Bishop:
        return 'B';
    case PieceType::Rook:
        return 'R';
    case PieceType::Queen:
        return 'Q';
    case PieceType::King:
        return 'K';
    case PieceType::Pawn:
    case PieceType::None:
        return '\0';
    }
    return '\0';
}

std::string normalize(std::string_view input) {
    std::string result;
    for (char character : input) {
        if (std::isspace(static_cast<unsigned char>(character)) == 0) {
            result.push_back(character == '0' ? 'O' : character);
        }
    }
    while (!result.empty() && (result.back() == '!' || result.back() == '?' ||
                               result.back() == '+' || result.back() == '#')) {
        result.pop_back();
    }
    const auto ep = result.find("e.p.");
    if (ep != std::string::npos)
        result.erase(ep);
    return result;
}

} // namespace

std::string to_san(Board& board, const Move& move) {
    const Piece moving = board.at(move.from);
    if (moving.empty()) {
        throw Error(ErrorCode::IllegalMove, "SAN move has no source piece");
    }

    std::string result;
    if (move.has(KingCastle)) {
        result = "O-O";
    } else if (move.has(QueenCastle)) {
        result = "O-O-O";
    } else {
        const char symbol = san_piece(moving.type);
        if (symbol != '\0')
            result.push_back(symbol);

        if (moving.type != PieceType::Pawn) {
            bool conflict = false;
            bool same_file = false;
            bool same_rank = false;
            for (const Move& candidate : board.legal_moves()) {
                if (candidate.from == move.from || candidate.to != move.to)
                    continue;
                const Piece other = board.at(candidate.from);
                if (other.type == moving.type && other.color == moving.color) {
                    conflict = true;
                    same_file = same_file || candidate.from % 8 == move.from % 8;
                    same_rank = same_rank || candidate.from / 8 == move.from / 8;
                }
            }
            if (conflict) {
                if (!same_file) {
                    result.push_back(square_name(move.from)[0]);
                } else if (!same_rank) {
                    result.push_back(square_name(move.from)[1]);
                } else {
                    result += square_name(move.from);
                }
            }
        } else if (move.has(Capture)) {
            result.push_back(square_name(move.from)[0]);
        }

        if (move.has(Capture))
            result.push_back('x');
        result += square_name(move.to);
        if (move.has(Promotion)) {
            result.push_back('=');
            result.push_back(san_piece(move.promotion));
        }
    }

    const Undo undo = board.make_move(move);
    if (board.in_check(board.side_to_move())) {
        result.push_back(board.legal_moves().empty() ? '#' : '+');
    }
    board.unmake_move(move, undo);
    return result;
}

Move parse_san(Board& board, std::string_view san) {
    const std::string expected = normalize(san);
    for (const Move& move : board.legal_moves()) {
        if (normalize(to_san(board, move)) == expected) {
            return move;
        }
    }
    throw Error(ErrorCode::IllegalMove,
                "illegal or unsupported SAN move '" + std::string(san) + "' in " + board.to_fen());
}

} // namespace pct::chess
