#include "pct/analysis/analyzer.hpp"
#include "pct/chess/board.hpp"
#include "pct/chess/pgn.hpp"
#include "pct/engine/stockfish.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace {

std::string read_file(const char* path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error(std::string("cannot open ") + path);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

void usage() {
    std::cerr << "usage:\n"
              << "  pct-cli fen '<fen>'\n"
              << "  pct-cli perft '<fen>' <depth>\n"
              << "  pct-cli pgn <file>\n"
              << "  pct-cli analyze <file> [stockfish-path]\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc >= 3 && std::string_view(argv[1]) == "fen") {
            std::cout << pct::chess::Board::from_fen(argv[2]).to_fen() << '\n';
            return 0;
        }
        if (argc >= 4 && std::string_view(argv[1]) == "perft") {
            auto board = pct::chess::Board::from_fen(argv[2]);
            std::cout << board.perft(static_cast<unsigned>(std::stoul(argv[3]))) << '\n';
            return 0;
        }
        if (argc >= 3 && std::string_view(argv[1]) == "pgn") {
            const auto game = pct::chess::parse_pgn(read_file(argv[2]));
            std::cout << game.tag("White", "?") << " vs. " << game.tag("Black", "?") << ", "
                      << game.plies.size() << " plies, id=" << game.identity << '\n';
            return 0;
        }
        if (argc >= 3 && std::string_view(argv[1]) == "analyze") {
            const auto game = pct::chess::parse_pgn(read_file(argv[2]));
            pct::engine::Stockfish engine(
                pct::engine::StockfishOptions{argc >= 4 ? argv[3] : "stockfish", 128, 1});
            pct::analysis::AnalysisCache cache;
            pct::analysis::Analyzer analyzer(engine, cache);
            const auto result = analyzer.analyze(game, [](const pct::analysis::Progress& progress) {
                std::cerr << pct::analysis::name(progress.stage) << ' ' << progress.complete << '/'
                          << progress.total << '\n';
            });
            for (const auto& mistake : result.mistakes) {
                std::cout << mistake.rank << ". " << mistake.san << " - " << mistake.category
                          << " (" << mistake.loss << " cp)\n"
                          << "   " << mistake.explanation << '\n';
            }
            return 0;
        }
        usage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
