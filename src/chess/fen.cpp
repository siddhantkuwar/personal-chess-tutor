#include "pct/chess/board.hpp"

#include "pct/common/error.hpp"

#include <cctype>
#include <charconv>
#include <sstream>
#include <string>
#include <vector>

namespace pct::chess {
namespace {

std::vector<std::string> split(std::string_view input) {
    std::istringstream stream{std::string(input)};
    std::vector<std::string> parts;
    for (std::string part; stream >> part;) {
        parts.push_back(std::move(part));
    }
    return parts;
}

std::uint16_t parse_uint(std::string_view value, const char* field) {
    unsigned parsed = 0;
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (ec != std::errc{} || end != value.data() + value.size() || parsed > 65535) {
        throw Error(ErrorCode::ParseError, std::string("invalid FEN ") + field);
    }
    return static_cast<std::uint16_t>(parsed);
}

} // namespace

Board Board::initial() {
    return from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

Board Board::from_fen(std::string_view fen) {
    const auto fields = split(fen);
    if (fields.size() != 6) {
        throw Error(ErrorCode::ParseError, "FEN must contain six fields");
    }

    Board board;
    board.squares_.fill(Piece{});
    int rank = 7;
    int file = 0;
    for (const char symbol : fields[0]) {
        if (symbol == '/') {
            if (file != 8 || rank == 0) {
                throw Error(ErrorCode::ParseError, "invalid FEN board rows");
            }
            --rank;
            file = 0;
        } else if (std::isdigit(static_cast<unsigned char>(symbol)) != 0) {
            const int empty = symbol - '0';
            if (empty < 1 || empty > 8 || file + empty > 8) {
                throw Error(ErrorCode::ParseError, "invalid FEN empty-square count");
            }
            file += empty;
        } else {
            const Piece piece = piece_from_symbol(symbol);
            if (piece.empty() || file >= 8) {
                throw Error(ErrorCode::ParseError, "invalid FEN piece placement");
            }
            board.squares_[static_cast<std::size_t>(rank * 8 + file)] = piece;
            ++file;
        }
    }
    if (rank != 0 || file != 8) {
        throw Error(ErrorCode::ParseError, "FEN board is incomplete");
    }

    if (fields[1] == "w") {
        board.side_to_move_ = Color::White;
    } else if (fields[1] == "b") {
        board.side_to_move_ = Color::Black;
    } else {
        throw Error(ErrorCode::ParseError, "invalid FEN side to move");
    }

    board.castling_rights_ = 0;
    if (fields[2] != "-") {
        for (const char right : fields[2]) {
            switch (right) {
            case 'K':
                board.castling_rights_ |= 1;
                break;
            case 'Q':
                board.castling_rights_ |= 2;
                break;
            case 'k':
                board.castling_rights_ |= 4;
                break;
            case 'q':
                board.castling_rights_ |= 8;
                break;
            default:
                throw Error(ErrorCode::ParseError, "invalid FEN castling rights");
            }
        }
    }

    board.en_passant_ = fields[3] == "-" ? no_square : parse_square(fields[3]);
    board.halfmove_clock_ = parse_uint(fields[4], "halfmove clock");
    board.fullmove_number_ = parse_uint(fields[5], "fullmove number");
    if (board.fullmove_number_ == 0) {
        throw Error(ErrorCode::ParseError, "FEN fullmove number must be positive");
    }

    if (board.king_square(Color::White) == no_square ||
        board.king_square(Color::Black) == no_square) {
        throw Error(ErrorCode::ParseError, "FEN must contain both kings");
    }
    board.hash_ = board.compute_hash();
    board.repetition_history_.push_back(board.hash_);
    return board;
}

std::string Board::to_fen() const {
    std::ostringstream output;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = squares_[static_cast<std::size_t>(rank * 8 + file)];
            if (piece.empty()) {
                ++empty;
                continue;
            }
            if (empty != 0) {
                output << empty;
                empty = 0;
            }
            output << piece_symbol(piece);
        }
        if (empty != 0) {
            output << empty;
        }
        if (rank != 0) {
            output << '/';
        }
    }
    output << (side_to_move_ == Color::White ? " w " : " b ");
    if (castling_rights_ == 0) {
        output << '-';
    } else {
        if ((castling_rights_ & 1) != 0)
            output << 'K';
        if ((castling_rights_ & 2) != 0)
            output << 'Q';
        if ((castling_rights_ & 4) != 0)
            output << 'k';
        if ((castling_rights_ & 8) != 0)
            output << 'q';
    }
    output << ' ' << (en_passant_ == no_square ? "-" : square_name(en_passant_)) << ' '
           << halfmove_clock_ << ' ' << fullmove_number_;
    return output.str();
}

} // namespace pct::chess
