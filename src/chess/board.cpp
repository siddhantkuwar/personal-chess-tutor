#include "pct/chess/board.hpp"

#include "pct/common/error.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>

namespace pct::chess {
namespace {

constexpr std::uint8_t white_king_side = 1;
constexpr std::uint8_t white_queen_side = 2;
constexpr std::uint8_t black_king_side = 4;
constexpr std::uint8_t black_queen_side = 8;

constexpr int file_of(Square square) {
    return static_cast<int>(square % 8);
}
constexpr int rank_of(Square square) {
    return static_cast<int>(square / 8);
}
constexpr bool on_board(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}
constexpr Square make_square(int file, int rank) {
    return static_cast<Square>(rank * 8 + file);
}

void add_move(std::vector<Move>& moves, Square from, Square to, Piece target,
              std::uint8_t extra = Quiet) {
    moves.push_back(Move{from, to, PieceType::None,
                         static_cast<std::uint8_t>(extra | (target.empty() ? Quiet : Capture))});
}

std::uint64_t mix(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

} // namespace

Board::Board() {
    squares_.fill(Piece{});
}

Piece Board::at(Square square) const {
    if (square >= 64) {
        throw Error(ErrorCode::InvalidArgument, "square is outside the board");
    }
    return squares_[square];
}

std::string square_name(Square square) {
    if (square >= 64) {
        throw Error(ErrorCode::InvalidArgument, "square is outside the board");
    }
    return {static_cast<char>('a' + file_of(square)), static_cast<char>('1' + rank_of(square))};
}

Square parse_square(std::string_view name) {
    if (name.size() != 2 || name[0] < 'a' || name[0] > 'h' || name[1] < '1' || name[1] > '8') {
        throw Error(ErrorCode::ParseError, "invalid square: " + std::string(name));
    }
    return make_square(name[0] - 'a', name[1] - '1');
}

char piece_symbol(Piece piece) {
    char symbol = ' ';
    switch (piece.type) {
    case PieceType::Pawn:
        symbol = 'p';
        break;
    case PieceType::Knight:
        symbol = 'n';
        break;
    case PieceType::Bishop:
        symbol = 'b';
        break;
    case PieceType::Rook:
        symbol = 'r';
        break;
    case PieceType::Queen:
        symbol = 'q';
        break;
    case PieceType::King:
        symbol = 'k';
        break;
    case PieceType::None:
        return ' ';
    }
    return piece.color == Color::White ? static_cast<char>(std::toupper(symbol)) : symbol;
}

Piece piece_from_symbol(char symbol) {
    Piece piece{std::isupper(static_cast<unsigned char>(symbol)) != 0 ? Color::White : Color::Black,
                PieceType::None};
    switch (static_cast<char>(std::tolower(static_cast<unsigned char>(symbol)))) {
    case 'p':
        piece.type = PieceType::Pawn;
        break;
    case 'n':
        piece.type = PieceType::Knight;
        break;
    case 'b':
        piece.type = PieceType::Bishop;
        break;
    case 'r':
        piece.type = PieceType::Rook;
        break;
    case 'q':
        piece.type = PieceType::Queen;
        break;
    case 'k':
        piece.type = PieceType::King;
        break;
    default:
        break;
    }
    return piece;
}

std::string uci(const Move& move) {
    std::string result = square_name(move.from) + square_name(move.to);
    if (move.has(Promotion)) {
        result.push_back(
            static_cast<char>(std::tolower(piece_symbol(Piece{Color::White, move.promotion}))));
    }
    return result;
}

Square Board::king_square(Color color) const {
    for (Square square = 0; square < 64; ++square) {
        if (squares_[square] == Piece{color, PieceType::King}) {
            return square;
        }
    }
    return no_square;
}

bool Board::is_square_attacked(Square square, Color by) const {
    const int file = file_of(square);
    const int rank = rank_of(square);

    const int pawn_source_rank = rank + (by == Color::White ? -1 : 1);
    for (const int delta_file : {-1, 1}) {
        const int source_file = file + delta_file;
        if (on_board(source_file, pawn_source_rank) &&
            squares_[make_square(source_file, pawn_source_rank)] == Piece{by, PieceType::Pawn}) {
            return true;
        }
    }

    constexpr std::array<std::array<int, 2>, 8> knight_offsets{{
        {{1, 2}},
        {{2, 1}},
        {{2, -1}},
        {{1, -2}},
        {{-1, -2}},
        {{-2, -1}},
        {{-2, 1}},
        {{-1, 2}},
    }};
    for (const auto& offset : knight_offsets) {
        const int source_file = file + offset[0];
        const int source_rank = rank + offset[1];
        if (on_board(source_file, source_rank) &&
            squares_[make_square(source_file, source_rank)] == Piece{by, PieceType::Knight}) {
            return true;
        }
    }

    constexpr std::array<std::array<int, 2>, 8> directions{{
        {{1, 0}},
        {{-1, 0}},
        {{0, 1}},
        {{0, -1}},
        {{1, 1}},
        {{1, -1}},
        {{-1, 1}},
        {{-1, -1}},
    }};
    for (std::size_t index = 0; index < directions.size(); ++index) {
        int source_file = file + directions[index][0];
        int source_rank = rank + directions[index][1];
        int distance = 1;
        while (on_board(source_file, source_rank)) {
            const Piece piece = squares_[make_square(source_file, source_rank)];
            if (!piece.empty()) {
                if (piece.color == by) {
                    const bool diagonal = index >= 4;
                    if (piece.type == PieceType::Queen ||
                        (diagonal && piece.type == PieceType::Bishop) ||
                        (!diagonal && piece.type == PieceType::Rook) ||
                        (distance == 1 && piece.type == PieceType::King)) {
                        return true;
                    }
                }
                break;
            }
            source_file += directions[index][0];
            source_rank += directions[index][1];
            ++distance;
        }
    }
    return false;
}

bool Board::in_check(Color color) const {
    const Square king = king_square(color);
    return king != no_square && is_square_attacked(king, opposite(color));
}

void Board::add_pawn_moves(std::vector<Move>& moves, Square from, Piece piece) const {
    const int file = file_of(from);
    const int rank = rank_of(from);
    const int step = piece.color == Color::White ? 1 : -1;
    const int start_rank = piece.color == Color::White ? 1 : 6;
    const int promotion_rank = piece.color == Color::White ? 7 : 0;
    const int next_rank = rank + step;
    if (!on_board(file, next_rank)) {
        return;
    }

    const auto add_pawn_move = [&](Square to, std::uint8_t flags) {
        if (rank_of(to) == promotion_rank) {
            for (const PieceType promotion :
                 {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
                moves.push_back(
                    Move{from, to, promotion, static_cast<std::uint8_t>(flags | Promotion)});
            }
        } else {
            moves.push_back(Move{from, to, PieceType::None, flags});
        }
    };

    const Square one = make_square(file, next_rank);
    if (squares_[one].empty()) {
        add_pawn_move(one, Quiet);
        const int double_rank = rank + 2 * step;
        if (rank == start_rank && squares_[make_square(file, double_rank)].empty()) {
            moves.push_back(
                Move{from, make_square(file, double_rank), PieceType::None, DoublePawnPush});
        }
    }

    for (const int delta_file : {-1, 1}) {
        const int target_file = file + delta_file;
        if (!on_board(target_file, next_rank)) {
            continue;
        }
        const Square target = make_square(target_file, next_rank);
        if (!squares_[target].empty() && squares_[target].color != piece.color) {
            add_pawn_move(target, Capture);
        } else if (target == en_passant_) {
            add_pawn_move(target, static_cast<std::uint8_t>(Capture | EnPassant));
        }
    }
}

void Board::add_knight_moves(std::vector<Move>& moves, Square from, Piece piece) const {
    constexpr std::array<std::array<int, 2>, 8> offsets{{
        {{1, 2}},
        {{2, 1}},
        {{2, -1}},
        {{1, -2}},
        {{-1, -2}},
        {{-2, -1}},
        {{-2, 1}},
        {{-1, 2}},
    }};
    for (const auto& offset : offsets) {
        const int target_file = file_of(from) + offset[0];
        const int target_rank = rank_of(from) + offset[1];
        if (!on_board(target_file, target_rank)) {
            continue;
        }
        const Square target = make_square(target_file, target_rank);
        if (squares_[target].empty() || squares_[target].color != piece.color) {
            add_move(moves, from, target, squares_[target]);
        }
    }
}

void Board::add_sliding_moves(std::vector<Move>& moves, Square from, Piece piece,
                              const int* directions, std::size_t count) const {
    for (std::size_t index = 0; index < count; ++index) {
        const int delta_file = directions[index * 2];
        const int delta_rank = directions[index * 2 + 1];
        int target_file = file_of(from) + delta_file;
        int target_rank = rank_of(from) + delta_rank;
        while (on_board(target_file, target_rank)) {
            const Square target = make_square(target_file, target_rank);
            if (squares_[target].empty()) {
                add_move(moves, from, target, Piece{});
            } else {
                if (squares_[target].color != piece.color) {
                    add_move(moves, from, target, squares_[target]);
                }
                break;
            }
            target_file += delta_file;
            target_rank += delta_rank;
        }
    }
}

void Board::add_king_moves(std::vector<Move>& moves, Square from, Piece piece) const {
    constexpr std::array<std::array<int, 2>, 8> offsets{{
        {{1, 0}},
        {{-1, 0}},
        {{0, 1}},
        {{0, -1}},
        {{1, 1}},
        {{1, -1}},
        {{-1, 1}},
        {{-1, -1}},
    }};
    for (const auto& offset : offsets) {
        const int target_file = file_of(from) + offset[0];
        const int target_rank = rank_of(from) + offset[1];
        if (!on_board(target_file, target_rank)) {
            continue;
        }
        const Square target = make_square(target_file, target_rank);
        if (squares_[target].empty() || squares_[target].color != piece.color) {
            add_move(moves, from, target, squares_[target]);
        }
    }

    const Color enemy = opposite(piece.color);
    if (piece.color == Color::White && from == parse_square("e1") && !in_check(Color::White)) {
        if ((castling_rights_ & white_king_side) != 0 && squares_[parse_square("f1")].empty() &&
            squares_[parse_square("g1")].empty() &&
            squares_[parse_square("h1")] == Piece{Color::White, PieceType::Rook} &&
            !is_square_attacked(parse_square("f1"), enemy) &&
            !is_square_attacked(parse_square("g1"), enemy)) {
            moves.push_back(Move{from, parse_square("g1"), PieceType::None, KingCastle});
        }
        if ((castling_rights_ & white_queen_side) != 0 && squares_[parse_square("d1")].empty() &&
            squares_[parse_square("c1")].empty() && squares_[parse_square("b1")].empty() &&
            squares_[parse_square("a1")] == Piece{Color::White, PieceType::Rook} &&
            !is_square_attacked(parse_square("d1"), enemy) &&
            !is_square_attacked(parse_square("c1"), enemy)) {
            moves.push_back(Move{from, parse_square("c1"), PieceType::None, QueenCastle});
        }
    }
    if (piece.color == Color::Black && from == parse_square("e8") && !in_check(Color::Black)) {
        if ((castling_rights_ & black_king_side) != 0 && squares_[parse_square("f8")].empty() &&
            squares_[parse_square("g8")].empty() &&
            squares_[parse_square("h8")] == Piece{Color::Black, PieceType::Rook} &&
            !is_square_attacked(parse_square("f8"), enemy) &&
            !is_square_attacked(parse_square("g8"), enemy)) {
            moves.push_back(Move{from, parse_square("g8"), PieceType::None, KingCastle});
        }
        if ((castling_rights_ & black_queen_side) != 0 && squares_[parse_square("d8")].empty() &&
            squares_[parse_square("c8")].empty() && squares_[parse_square("b8")].empty() &&
            squares_[parse_square("a8")] == Piece{Color::Black, PieceType::Rook} &&
            !is_square_attacked(parse_square("d8"), enemy) &&
            !is_square_attacked(parse_square("c8"), enemy)) {
            moves.push_back(Move{from, parse_square("c8"), PieceType::None, QueenCastle});
        }
    }
}

std::vector<Move> Board::pseudo_legal_moves() const {
    std::vector<Move> moves;
    moves.reserve(64);
    constexpr int bishop_directions[] = {1, 1, 1, -1, -1, 1, -1, -1};
    constexpr int rook_directions[] = {1, 0, -1, 0, 0, 1, 0, -1};
    for (Square from = 0; from < 64; ++from) {
        const Piece piece = squares_[from];
        if (piece.empty() || piece.color != side_to_move_) {
            continue;
        }
        switch (piece.type) {
        case PieceType::Pawn:
            add_pawn_moves(moves, from, piece);
            break;
        case PieceType::Knight:
            add_knight_moves(moves, from, piece);
            break;
        case PieceType::Bishop:
            add_sliding_moves(moves, from, piece, bishop_directions, 4);
            break;
        case PieceType::Rook:
            add_sliding_moves(moves, from, piece, rook_directions, 4);
            break;
        case PieceType::Queen:
            add_sliding_moves(moves, from, piece, bishop_directions, 4);
            add_sliding_moves(moves, from, piece, rook_directions, 4);
            break;
        case PieceType::King:
            add_king_moves(moves, from, piece);
            break;
        case PieceType::None:
            break;
        }
    }
    return moves;
}

std::vector<Move> Board::legal_moves() {
    std::vector<Move> legal;
    for (const Move& move : pseudo_legal_moves()) {
        const Undo undo = make_move(move);
        const Color moved = opposite(side_to_move_);
        if (!in_check(moved)) {
            legal.push_back(move);
        }
        unmake_move(move, undo);
    }
    return legal;
}

std::optional<Move> Board::find_legal_move(Square from, Square to, PieceType promotion) {
    for (const Move& move : legal_moves()) {
        if (move.from == from && move.to == to &&
            (!move.has(Promotion) || move.promotion == promotion)) {
            return move;
        }
    }
    return std::nullopt;
}

Undo Board::make_move(const Move& move) {
    if (move.from >= 64 || move.to >= 64 || squares_[move.from].empty() ||
        squares_[move.from].color != side_to_move_) {
        throw Error(ErrorCode::IllegalMove, "cannot make invalid move");
    }
    Piece moving = squares_[move.from];
    Undo undo{squares_[move.to], move.to,          en_passant_, castling_rights_,
              halfmove_clock_,   fullmove_number_, hash_};

    if (move.has(EnPassant)) {
        undo.captured_square = static_cast<Square>(static_cast<int>(move.to) +
                                                   (moving.color == Color::White ? -8 : 8));
        undo.captured = squares_[undo.captured_square];
        squares_[undo.captured_square] = Piece{};
    }

    squares_[move.from] = Piece{};
    squares_[move.to] = moving;
    if (move.has(Promotion)) {
        squares_[move.to].type = move.promotion;
    }

    if (move.has(KingCastle)) {
        const Square rook_from =
            moving.color == Color::White ? parse_square("h1") : parse_square("h8");
        const Square rook_to =
            moving.color == Color::White ? parse_square("f1") : parse_square("f8");
        squares_[rook_to] = squares_[rook_from];
        squares_[rook_from] = Piece{};
    } else if (move.has(QueenCastle)) {
        const Square rook_from =
            moving.color == Color::White ? parse_square("a1") : parse_square("a8");
        const Square rook_to =
            moving.color == Color::White ? parse_square("d1") : parse_square("d8");
        squares_[rook_to] = squares_[rook_from];
        squares_[rook_from] = Piece{};
    }

    if (moving.type == PieceType::King) {
        castling_rights_ &= moving.color == Color::White
                                ? static_cast<std::uint8_t>(~(white_king_side | white_queen_side))
                                : static_cast<std::uint8_t>(~(black_king_side | black_queen_side));
    }
    if (move.from == parse_square("a1") || move.to == parse_square("a1"))
        castling_rights_ &= ~white_queen_side;
    if (move.from == parse_square("h1") || move.to == parse_square("h1"))
        castling_rights_ &= ~white_king_side;
    if (move.from == parse_square("a8") || move.to == parse_square("a8"))
        castling_rights_ &= ~black_queen_side;
    if (move.from == parse_square("h8") || move.to == parse_square("h8"))
        castling_rights_ &= ~black_king_side;

    en_passant_ =
        move.has(DoublePawnPush)
            ? static_cast<Square>((static_cast<int>(move.from) + static_cast<int>(move.to)) / 2)
            : no_square;
    halfmove_clock_ = (moving.type == PieceType::Pawn || !undo.captured.empty())
                          ? 0
                          : static_cast<std::uint16_t>(halfmove_clock_ + 1);
    if (side_to_move_ == Color::Black) {
        ++fullmove_number_;
    }
    side_to_move_ = opposite(side_to_move_);
    hash_ = compute_hash();
    repetition_history_.push_back(hash_);
    return undo;
}

void Board::unmake_move(const Move& move, const Undo& undo) {
    if (repetition_history_.size() > 1)
        repetition_history_.pop_back();
    side_to_move_ = opposite(side_to_move_);
    Piece moving = squares_[move.to];
    if (move.has(Promotion)) {
        moving.type = PieceType::Pawn;
    }
    squares_[move.from] = moving;
    squares_[move.to] = Piece{};
    if (!undo.captured.empty()) {
        squares_[undo.captured_square] = undo.captured;
    }
    if (move.has(KingCastle)) {
        const Square rook_from =
            moving.color == Color::White ? parse_square("h1") : parse_square("h8");
        const Square rook_to =
            moving.color == Color::White ? parse_square("f1") : parse_square("f8");
        squares_[rook_from] = squares_[rook_to];
        squares_[rook_to] = Piece{};
    } else if (move.has(QueenCastle)) {
        const Square rook_from =
            moving.color == Color::White ? parse_square("a1") : parse_square("a8");
        const Square rook_to =
            moving.color == Color::White ? parse_square("d1") : parse_square("d8");
        squares_[rook_from] = squares_[rook_to];
        squares_[rook_to] = Piece{};
    }
    en_passant_ = undo.en_passant;
    castling_rights_ = undo.castling_rights;
    halfmove_clock_ = undo.halfmove_clock;
    fullmove_number_ = undo.fullmove_number;
    hash_ = undo.hash;
}

std::uint64_t Board::perft(unsigned depth) {
    if (depth == 0) {
        return 1;
    }
    std::uint64_t nodes = 0;
    for (const Move& move : legal_moves()) {
        const Undo undo = make_move(move);
        nodes += perft(depth - 1);
        unmake_move(move, undo);
    }
    return nodes;
}

PerftStats Board::perft_stats(unsigned depth) {
    if (depth == 0) {
        return PerftStats{1};
    }
    PerftStats result;
    for (const Move& move : legal_moves()) {
        const Undo undo = make_move(move);
        if (depth == 1) {
            ++result.nodes;
            if (move.has(Capture))
                ++result.captures;
            if (move.has(EnPassant))
                ++result.en_passant;
            if (move.has(KingCastle) || move.has(QueenCastle))
                ++result.castles;
            if (move.has(Promotion))
                ++result.promotions;
            if (in_check(side_to_move_)) {
                ++result.checks;
                if (legal_moves().empty())
                    ++result.checkmates;
            }
        } else {
            const PerftStats child = perft_stats(depth - 1);
            result.nodes += child.nodes;
            result.captures += child.captures;
            result.en_passant += child.en_passant;
            result.castles += child.castles;
            result.promotions += child.promotions;
            result.checks += child.checks;
            result.checkmates += child.checkmates;
        }
        unmake_move(move, undo);
    }
    return result;
}

int Board::material(Color color) const {
    int total = 0;
    for (const Piece piece : squares_) {
        if (piece.empty() || piece.color != color)
            continue;
        switch (piece.type) {
        case PieceType::Pawn:
            total += 100;
            break;
        case PieceType::Knight:
            total += 320;
            break;
        case PieceType::Bishop:
            total += 330;
            break;
        case PieceType::Rook:
            total += 500;
            break;
        case PieceType::Queen:
            total += 900;
            break;
        case PieceType::King:
        case PieceType::None:
            break;
        }
    }
    return total;
}

std::uint64_t Board::compute_hash() const {
    std::uint64_t result = 0;
    for (Square square = 0; square < 64; ++square) {
        const Piece piece = squares_[square];
        if (!piece.empty()) {
            const auto code = static_cast<std::uint64_t>(piece.type) +
                              (piece.color == Color::Black ? 8ULL : 0ULL);
            result ^= mix(code * 64ULL + square);
        }
    }
    result ^= mix(1024ULL + static_cast<std::uint64_t>(castling_rights_));
    if (en_passant_ != no_square) {
        const int source_rank = rank_of(en_passant_) + (side_to_move_ == Color::White ? -1 : 1);
        bool capturable = false;
        for (const int delta_file : {-1, 1}) {
            const int source_file = file_of(en_passant_) + delta_file;
            if (on_board(source_file, source_rank) &&
                squares_[make_square(source_file, source_rank)] ==
                    Piece{side_to_move_, PieceType::Pawn}) {
                capturable = true;
            }
        }
        if (capturable) {
            result ^= mix(2048ULL + static_cast<std::uint64_t>(file_of(en_passant_)));
        }
    }
    if (side_to_move_ == Color::Black)
        result ^= mix(4096ULL);
    return result;
}

std::size_t Board::repetition_count() const {
    return static_cast<std::size_t>(
        std::count(repetition_history_.begin(), repetition_history_.end(), hash_));
}

} // namespace pct::chess
