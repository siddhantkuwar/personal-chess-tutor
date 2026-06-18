#pragma once

#include "pct/chess/board.hpp"

#include <string>
#include <string_view>

namespace pct::chess {

[[nodiscard]] std::string to_san(Board& board, const Move& move);
[[nodiscard]] Move parse_san(Board& board, std::string_view san);

} // namespace pct::chess
