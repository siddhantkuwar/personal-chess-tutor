#include "pct/chess/pgn.hpp"

#include "pct/chess/san.hpp"
#include "pct/common/error.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace pct::chess {
namespace {

std::string strip_non_mainline(std::string_view input) {
    std::string output;
    int variation_depth = 0;
    bool brace_comment = false;
    bool line_comment = false;
    for (std::size_t index = 0; index < input.size(); ++index) {
        const char character = input[index];
        if (line_comment) {
            if (character == '\n') {
                line_comment = false;
                output.push_back(' ');
            }
            continue;
        }
        if (brace_comment) {
            if (character == '}')
                brace_comment = false;
            continue;
        }
        if (character == ';' && variation_depth == 0) {
            line_comment = true;
        } else if (character == '{' && variation_depth == 0) {
            brace_comment = true;
        } else if (character == '(') {
            ++variation_depth;
        } else if (character == ')') {
            if (variation_depth == 0) {
                throw Error(ErrorCode::ParseError, "unbalanced PGN variation");
            }
            --variation_depth;
        } else if (variation_depth == 0) {
            output.push_back(character);
        }
    }
    if (brace_comment || variation_depth != 0) {
        throw Error(ErrorCode::ParseError, "unterminated PGN comment or variation");
    }
    return output;
}

bool result_token(std::string_view token) {
    return token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*";
}

bool move_number_token(std::string_view token) {
    if (token.empty())
        return false;
    std::size_t index = 0;
    while (index < token.size() && std::isdigit(static_cast<unsigned char>(token[index])) != 0) {
        ++index;
    }
    if (index == 0 || index == token.size())
        return false;
    while (index < token.size() && token[index] == '.')
        ++index;
    return index == token.size();
}

std::uint64_t fnv1a(std::string_view value) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

std::string Game::tag(std::string_view key, std::string_view fallback) const {
    const auto found = tags.find(std::string(key));
    return found == tags.end() ? std::string(fallback) : found->second;
}

Game parse_pgn(std::string_view pgn) {
    Game game;
    game.source_pgn = std::string(pgn);
    std::size_t offset = 0;

    while (offset < pgn.size()) {
        while (offset < pgn.size() && std::isspace(static_cast<unsigned char>(pgn[offset])) != 0) {
            ++offset;
        }
        if (offset >= pgn.size() || pgn[offset] != '[')
            break;
        const std::size_t end = pgn.find(']', offset);
        if (end == std::string_view::npos) {
            throw Error(ErrorCode::ParseError, "unterminated PGN tag");
        }
        const std::string_view line = pgn.substr(offset + 1, end - offset - 1);
        const std::size_t space = line.find_first_of(" \t");
        const std::size_t quote = line.find('"', space);
        const std::size_t final_quote = line.rfind('"');
        if (space == std::string_view::npos || quote == std::string_view::npos ||
            final_quote == quote) {
            throw Error(ErrorCode::ParseError, "invalid PGN tag");
        }
        game.tags.emplace(std::string(line.substr(0, space)),
                          std::string(line.substr(quote + 1, final_quote - quote - 1)));
        offset = end + 1;
    }

    Board board = game.tag("SetUp") == "1" ? Board::from_fen(game.tag("FEN")) : Board::initial();
    std::istringstream moves(strip_non_mainline(pgn.substr(offset)));
    for (std::string token; moves >> token;) {
        if (token.starts_with('$') || move_number_token(token))
            continue;
        const auto dot = token.rfind('.');
        if (dot != std::string::npos && dot + 1 < token.size() &&
            std::isdigit(static_cast<unsigned char>(token.front())) != 0) {
            token.erase(0, dot + 1);
        }
        if (token.empty())
            continue;
        if (result_token(token))
            break;
        const std::string before = board.to_fen();
        const Move move = parse_san(board, token);
        const std::string canonical = to_san(board, move);
        board.make_move(move);
        game.plies.push_back(Ply{move, canonical, before, board.to_fen()});
    }
    if (game.plies.empty()) {
        throw Error(ErrorCode::ParseError, "PGN contains no moves");
    }
    game.identity = normalized_game_identity(game);
    return game;
}

std::string normalized_game_identity(const Game& game) {
    std::ostringstream normalized;
    normalized << game.tag("Site") << '|' << game.tag("Date") << '|' << game.tag("Round") << '|'
               << game.tag("White") << '|' << game.tag("Black") << '|' << game.tag("Result") << '|';
    for (const Ply& ply : game.plies)
        normalized << uci(ply.move) << ' ';
    std::ostringstream identity;
    identity << std::hex << std::setfill('0') << std::setw(16) << fnv1a(normalized.str());
    return identity.str();
}

} // namespace pct::chess
