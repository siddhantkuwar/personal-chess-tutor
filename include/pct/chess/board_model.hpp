#pragma once

#include "pct/chess/board.hpp"

#include <concepts>
#include <string>
#include <string_view>
#include <vector>

namespace pct::chess {

template <typename T>
concept BoardModel = requires(T board, const T constant, Move move, Undo undo, Square square,
                              std::string_view fen) {
    { T::initial() } -> std::same_as<T>;
    { T::from_fen(fen) } -> std::same_as<T>;
    { constant.to_fen() } -> std::same_as<std::string>;
    { constant.at(square) } -> std::same_as<Piece>;
    { board.legal_moves() } -> std::same_as<std::vector<Move>>;
    { board.make_move(move) } -> std::same_as<Undo>;
    { board.unmake_move(move, undo) } -> std::same_as<void>;
    { board.perft(1) } -> std::same_as<std::uint64_t>;
    { constant.hash() } -> std::same_as<std::uint64_t>;
};

static_assert(BoardModel<Board>);

} // namespace pct::chess
