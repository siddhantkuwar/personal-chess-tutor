#include "pct/analysis/analyzer.hpp"

#include "pct/common/json.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace pct::analysis {
namespace {

struct BookLine {
    std::string eco;
    std::string name;
    std::vector<std::string> moves;
};

struct OpeningBook {
    std::string version;
    std::vector<BookLine> lines;
};

OpeningBook fallback_book() {
    return {std::string(opening_book_version),
            {{"C50", "Italian Game", {"e4", "e5", "Nf3", "Nc6", "Bc4", "Bc5"}},
             {"C60", "Ruy Lopez", {"e4", "e5", "Nf3", "Nc6", "Bb5", "a6"}},
             {"B50", "Sicilian Defense", {"e4", "c5", "Nf3", "d6"}},
             {"C00", "French Defense", {"e4", "e6", "d4", "d5"}},
             {"B12", "Caro-Kann Defense", {"e4", "c6", "d4", "d5"}},
             {"D35", "Queen's Gambit Declined", {"d4", "d5", "c4", "e6", "Nc3", "Nf6"}},
             {"E60", "King's Indian Defense", {"d4", "Nf6", "c4", "g6"}},
             {"A28", "English Opening", {"c4", "e5", "Nc3", "Nf6"}}}};
}

OpeningBook load_book() {
    std::filesystem::path path = PCT_OPENING_BOOK_PATH;
    if (const char* override_path = std::getenv("PCT_OPENING_BOOK");
        override_path != nullptr && *override_path != '\0')
        path = override_path;
    else if (std::filesystem::exists("resources/openings.json"))
        path = "resources/openings.json";
    std::ifstream input(path);
    if (!input)
        return fallback_book();
    try {
        std::stringstream contents;
        contents << input.rdbuf();
        const json::Value document = json::parse(contents.str());
        OpeningBook book;
        book.version = document.at("version").as_string();
        for (const auto& value : document.at("lines").as_array()) {
            BookLine line;
            line.eco = value.at("eco").as_string();
            line.name = value.at("name").as_string();
            for (const auto& move : value.at("moves").as_array())
                line.moves.push_back(move.as_string());
            if (!line.eco.empty() && !line.name.empty() && !line.moves.empty())
                book.lines.push_back(std::move(line));
        }
        return book.version.empty() || book.lines.empty() ? fallback_book() : book;
    } catch (const std::exception&) {
        return fallback_book();
    }
}

} // namespace

OpeningMatch recognize_opening(const chess::Game& game) {
    const OpeningBook book = load_book();
    const BookLine* best = nullptr;
    std::size_t best_match = 0;
    for (const auto& line : book.lines) {
        std::size_t matched = 0;
        while (matched < game.plies.size() && matched < line.moves.size() &&
               game.plies[matched].san == line.moves[matched])
            ++matched;
        if (matched > best_match) {
            best = &line;
            best_match = matched;
        }
    }
    if (!best || best_match < 2)
        return OpeningMatch{"", "", 0, std::nullopt, book.version};
    OpeningMatch match{best->eco, best->name, best_match, std::nullopt, book.version};
    if (best_match < game.plies.size() && best_match < best->moves.size())
        match.departure_ply = best_match;
    return match;
}

} // namespace pct::analysis
