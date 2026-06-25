#include "pct/training/training.hpp"

#include "pct/chess/board.hpp"
#include "pct/common/error.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>

namespace pct::training {
namespace {

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input)
        throw Error(ErrorCode::IoError, "cannot open tactical corpus: " + path.string());
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

std::optional<chess::Move> legal_uci(chess::Board& board, std::string_view text) {
    if (text.size() < 4)
        return std::nullopt;
    chess::PieceType promotion = chess::PieceType::Queen;
    if (text.size() == 5) {
        if (text[4] == 'n') promotion = chess::PieceType::Knight;
        if (text[4] == 'b') promotion = chess::PieceType::Bishop;
        if (text[4] == 'r') promotion = chess::PieceType::Rook;
    }
    try {
        return board.find_legal_move(chess::parse_square(text.substr(0, 2)),
                                     chess::parse_square(text.substr(2, 2)), promotion);
    } catch (const Error&) {
        return std::nullopt;
    }
}

std::optional<int> score(const engine::PrincipalVariation& line) {
    if (line.centipawns)
        return line.centipawns;
    if (line.mate)
        return *line.mate > 0 ? 100000 - *line.mate : -100000 - *line.mate;
    return std::nullopt;
}

bool stable_solution(const engine::AnalysisResult& result, std::string_view solution) {
    if (result.best_move != solution || result.lines.empty() || result.lines.front().moves.empty() ||
        result.lines.front().moves.front() != solution)
        return false;
    if (result.lines.size() < 2)
        return true;
    const auto best = score(result.lines[0]);
    const auto alternative = score(result.lines[1]);
    return !best || !alternative || *best - *alternative >= 50;
}

} // namespace

TacticalCorpus TacticalCorpus::load(const std::filesystem::path& path) {
    const json::Value document = json::parse(read_text(path));
    TacticalCorpus result;
    const auto& manifest = document.at("manifest");
    result.manifest_ = TacticalCorpusManifest{
        manifest.at("id").as_string(), manifest.at("version").as_string(),
        manifest.at("source_url").as_string(), manifest.at("download_url").as_string(),
        manifest.at("license").as_string()};
    if (result.manifest_.id.empty() || result.manifest_.version.empty() ||
        result.manifest_.source_url.empty() || result.manifest_.license.empty())
        throw Error(ErrorCode::ParseError, "tactical corpus manifest is incomplete");
    std::set<std::string> ids;
    for (const auto& value : document.at("puzzles").as_array()) {
        const std::string id = value.at("id").as_string();
        if (!ids.insert(id).second)
            throw Error(ErrorCode::ParseError, "duplicate tactical puzzle id");
        chess::Board board = chess::Board::from_fen(value.at("fen").as_string());
        const auto& moves = value.at("moves").as_array();
        if (moves.size() < 2)
            throw Error(ErrorCode::ParseError, "tactical puzzle needs setup and solution moves");
        const auto setup = legal_uci(board, moves[0].as_string());
        if (!setup)
            throw Error(ErrorCode::IllegalMove, "tactical puzzle setup move is illegal");
        static_cast<void>(board.make_move(*setup));
        const std::string solution = moves[1].as_string();
        if (!legal_uci(board, solution))
            throw Error(ErrorCode::IllegalMove, "tactical puzzle solution move is illegal");
        TacticalPuzzle puzzle;
        puzzle.id = id;
        puzzle.fen = board.to_fen();
        puzzle.solution = solution;
        puzzle.rating = value.at("rating").as_int();
        for (const auto& motif : value.at("motifs").as_array())
            puzzle.motifs.push_back(motif.as_string());
        result.puzzles_.push_back(std::move(puzzle));
    }
    return result;
}

std::vector<TacticalPuzzle> TacticalCorpus::match(const Profile& profile,
                                                  std::size_t limit) const {
    std::set<std::string> recurring;
    for (const auto& weakness : profile.weaknesses)
        if (weakness.occurrences >= 2)
            recurring.insert(weakness.category);
    std::vector<TacticalPuzzle> result;
    for (const auto& puzzle : puzzles_) {
        const bool motif_match = std::any_of(
            puzzle.motifs.begin(), puzzle.motifs.end(),
            [&](const std::string& motif) { return recurring.contains(motif); });
        const bool rating_match = profile.latest_rating <= 0 ||
                                  std::abs(puzzle.rating - profile.latest_rating) <= 400;
        if (motif_match && rating_match)
            result.push_back(puzzle);
    }
    std::stable_sort(result.begin(), result.end(), [&](const auto& left, const auto& right) {
        return std::abs(left.rating - profile.latest_rating) <
               std::abs(right.rating - profile.latest_rating);
    });
    if (result.size() > limit)
        result.resize(limit);
    return result;
}

AdvancedDrillGenerator::AdvancedDrillGenerator(TacticalCorpus corpus,
                                               ValidationEngineFactory verifier_factory)
    : corpus_(std::move(corpus)), verifier_factory_(std::move(verifier_factory)) {
    if (!verifier_factory_)
        throw Error(ErrorCode::InvalidArgument, "advanced drills require an engine factory");
}

std::vector<Drill> AdvancedDrillGenerator::generate(const Profile& profile,
                                                    const std::vector<Drill>& personal_drills,
                                                    std::size_t limit) const {
    std::map<std::string, std::size_t> personal_by_category;
    for (const auto& drill : personal_drills)
        if (drill.source_type == "personal_game")
            ++personal_by_category[drill.category];
    const auto candidates = corpus_.match(profile, limit * 3);
    if (candidates.empty())
        return {};
    auto first = verifier_factory_();
    auto second = verifier_factory_();
    if (!first || !second)
        throw Error(ErrorCode::EngineError, "advanced drill verifier factory returned no engine");
    std::vector<Drill> result;
    for (const auto& puzzle : candidates) {
        const auto category = std::find_if(puzzle.motifs.begin(), puzzle.motifs.end(),
                                           [&](const auto& motif) {
                                               return std::any_of(profile.weaknesses.begin(),
                                                                  profile.weaknesses.end(),
                                                                  [&](const auto& weakness) {
                                                                      return weakness.category == motif;
                                                                  });
                                           });
        if (category == puzzle.motifs.end() || personal_by_category[*category] >= 3)
            continue;
        engine::AnalysisRequest request;
        request.fen = puzzle.fen;
        request.depth = 18;
        request.multipv = 2;
        request.priority = engine::AnalysisPriority::Historical;
        const engine::AnalysisResult first_result = first->analyze(request);
        const engine::AnalysisResult second_result = second->analyze(request);
        if (!stable_solution(first_result, puzzle.solution) ||
            !stable_solution(second_result, puzzle.solution))
            continue;
        Drill drill;
        drill.id = "corpus:" + corpus_.manifest().id + ":" + puzzle.id;
        drill.source_game_id = "corpus:" + corpus_.manifest().id;
        drill.fen = puzzle.fen;
        drill.category = *category;
        drill.phase = "any";
        drill.explanation = "A validated " + *category +
                            " pattern selected because it recurs in your games.";
        drill.solutions = {puzzle.solution};
        drill.difficulty = std::clamp(1 + puzzle.rating / 500, 1, 5);
        drill.source_type = "public_corpus";
        drill.provenance = corpus_.manifest().source_url + " | " + corpus_.manifest().license;
        drill.corpus_version = corpus_.manifest().version;
        drill.validation_evidence = {
            "solution is legal in the presented FEN",
            "independent verifier A selected " + first_result.best_move,
            "independent verifier B selected " + second_result.best_move,
            "ambiguous alternatives within 50 centipawns were rejected",
        };
        result.push_back(std::move(drill));
        if (result.size() == limit)
            break;
    }
    return result;
}

} // namespace pct::training
