#include "pct/analysis/analyzer.hpp"

#include "pct/chess/san.hpp"
#include "pct/common/error.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>

namespace pct::analysis {
namespace {

int piece_value(chess::PieceType type) {
    switch (type) {
    case chess::PieceType::Pawn:
        return 100;
    case chess::PieceType::Knight:
        return 320;
    case chess::PieceType::Bishop:
        return 330;
    case chess::PieceType::Rook:
        return 500;
    case chess::PieceType::Queen:
        return 900;
    case chess::PieceType::King:
        return 20000;
    case chess::PieceType::None:
        return 0;
    }
    return 0;
}

int score_for_white(const engine::AnalysisResult& result, const chess::Board& board) {
    if (result.lines.empty())
        return 0;
    const auto& line = result.lines.front();
    int score = line.centipawns.value_or(0);
    if (line.mate)
        score = *line.mate > 0 ? 100000 - *line.mate : -100000 - *line.mate;
    return board.side_to_move() == chess::Color::White ? score : -score;
}

std::optional<chess::Move> parse_uci_move(chess::Board& board, std::string_view uci) {
    if (uci.size() < 4)
        return std::nullopt;
    try {
        chess::PieceType promotion = chess::PieceType::Queen;
        if (uci.size() >= 5) {
            switch (uci[4]) {
            case 'n':
                promotion = chess::PieceType::Knight;
                break;
            case 'b':
                promotion = chess::PieceType::Bishop;
                break;
            case 'r':
                promotion = chess::PieceType::Rook;
                break;
            default:
                break;
            }
        }
        return board.find_legal_move(chess::parse_square(uci.substr(0, 2)),
                                     chess::parse_square(uci.substr(2, 2)), promotion);
    } catch (const Error&) {
        return std::nullopt;
    }
}

MoveQuality quality_for(const chess::Game& game, std::size_t index, int loss,
                        const chess::Board& after) {
    const auto& ply = game.plies[index];
    if (loss >= 250)
        return MoveQuality::Blunder;
    if (loss >= 120)
        return MoveQuality::Mistake;
    if (loss >= 60)
        return MoveQuality::Inaccuracy;
    if (ply.move.has(chess::Capture)) {
        if (index > 0 && game.plies[index - 1].move.has(chess::Capture) &&
            ply.move.to == game.plies[index - 1].move.to) {
            return MoveQuality::Recapture;
        }
        return MoveQuality::Capture;
    }
    if (after.in_check(after.side_to_move()))
        return MoveQuality::Check;
    const chess::Piece moved = chess::Board::from_fen(ply.fen_before).at(ply.move.from);
    const int destination_rank = static_cast<int>(ply.move.to / 8);
    if ((moved.type == chess::PieceType::Knight || moved.type == chess::PieceType::Bishop) &&
        ((moved.color == chess::Color::White && destination_rank > 0) ||
         (moved.color == chess::Color::Black && destination_rank < 7))) {
        return MoveQuality::Developing;
    }
    return MoveQuality::Neutral;
}

std::string category_for(const chess::Game& game, std::size_t index,
                         const engine::AnalysisResult& deep,
                         const engine::AnalysisResult& after_result) {
    chess::Board before = chess::Board::from_fen(game.plies[index].fen_before);
    chess::Board after = chess::Board::from_fen(game.plies[index].fen_after);
    if (!after_result.lines.empty() && after_result.lines.front().mate &&
        *after_result.lines.front().mate > 0) {
        return "Failed response to mate threat";
    }
    if (auto response = parse_uci_move(after, after_result.best_move)) {
        const chess::Piece target =
            response->has(chess::EnPassant)
                ? chess::Piece{chess::opposite(after.side_to_move()), chess::PieceType::Pawn}
                : after.at(response->to);
        if (response->has(chess::Capture) && !target.empty()) {
            if (target.type == chess::PieceType::Queen)
                return "Hanging queen";
            if (piece_value(target.type) >= 300)
                return "Hanging piece";
            return "Ignored attack";
        }
    }
    if (index > 0 && game.plies[index - 1].move.has(chess::Capture) &&
        !game.plies[index].move.has(chess::Capture)) {
        const chess::Square capture_square = game.plies[index - 1].move.to;
        for (const chess::Move& move : before.legal_moves()) {
            if (move.to == capture_square && move.has(chess::Capture))
                return "Failed recapture";
        }
    }
    if (!deep.lines.empty() && !deep.lines.front().moves.empty()) {
        if (auto best = parse_uci_move(before, deep.lines.front().moves.front())) {
            if (best->has(chess::Capture) && !game.plies[index].move.has(chess::Capture)) {
                return "Missed free capture";
            }
            const chess::Undo undo = before.make_move(*best);
            const bool check = before.in_check(before.side_to_move());
            const bool mate = check && before.legal_moves().empty();
            before.unmake_move(*best, undo);
            if (mate)
                return "Missed mate";
            if (check)
                return "Missed check";
        }
    }
    return "One-move tactical loss";
}

std::string explanation_for(std::string_view category, const chess::Game& game, std::size_t index,
                            std::string_view punishment) {
    const std::string move = game.plies[index].san;
    if (category == "Hanging queen") {
        return move + " left your queen available to be captured. Move it, defend it, or create a "
                      "forcing threat.";
    }
    if (category == "Hanging piece") {
        return move + " left a valuable piece undefended. The opponent can take it with " +
               std::string(punishment) + ".";
    }
    if (category == "Ignored attack") {
        return move + " did not answer the opponent's immediate capture threat.";
    }
    if (category == "Failed recapture") {
        return "The previous move captured material, but " + move +
               " missed the available recapture. Check forcing captures before choosing another "
               "plan.";
    }
    if (category == "Missed free capture") {
        return move +
               " passed up a favorable capture. Look at every legal check and capture first.";
    }
    if (category == "Missed mate") {
        return move + " missed a forced checkmate. Calculate forcing checks before quieter moves.";
    }
    if (category == "Failed response to mate threat") {
        return move + " allowed the opponent a forced mate. When your king is threatened, "
                      "calculate every check and forcing reply first.";
    }
    if (category == "Missed check") {
        return move + " missed a strong forcing check that limited the opponent's replies.";
    }
    return move + " allowed a concrete tactical swing. Compare the opponent's strongest reply with "
                  "the safer candidate moves.";
}

} // namespace

std::string AnalysisCache::key(const engine::AnalysisRequest& request) {
    std::ostringstream key;
    key << std::hex << chess::Board::from_fen(request.fen).hash() << std::dec
        << "|d=" << request.depth << "|t=" << request.move_time.count()
        << "|pv=" << request.multipv;
    return key.str();
}

bool AnalysisCache::get(const engine::AnalysisRequest& request,
                        engine::AnalysisResult& result) const {
    std::lock_guard lock(mutex_);
    const auto found = values_.find(key(request));
    if (found == values_.end())
        return false;
    result = found->second;
    return true;
}

void AnalysisCache::put(const engine::AnalysisRequest& request, engine::AnalysisResult result) {
    std::lock_guard lock(mutex_);
    values_.insert_or_assign(key(request), std::move(result));
}

std::size_t AnalysisCache::size() const {
    std::lock_guard lock(mutex_);
    return values_.size();
}

Analyzer::Analyzer(engine::AnalysisEngine& engine, AnalysisCache& cache, AnalyzerOptions options)
    : engine_(engine), cache_(cache), options_(options) {}

engine::AnalysisResult Analyzer::analyze_cached(const engine::AnalysisRequest& request,
                                                std::stop_token stop_token) {
    engine::AnalysisResult result;
    if (cache_.get(request, result))
        return result;
    result = engine_.analyze(request, stop_token);
    cache_.put(request, result);
    return result;
}

GamePhase Analyzer::classify_phase(const chess::Board& board, std::size_t ply) {
    int non_pawn_material = 0;
    int queens = 0;
    int undeveloped = 0;
    for (chess::Square square = 0; square < 64; ++square) {
        const chess::Piece piece = board.at(square);
        if (piece.empty())
            continue;
        if (piece.type == chess::PieceType::Queen)
            ++queens;
        if (piece.type != chess::PieceType::Pawn && piece.type != chess::PieceType::King) {
            non_pawn_material += piece_value(piece.type);
        }
    }
    for (const std::string_view square : {"b1", "c1", "f1", "g1", "b8", "c8", "f8", "g8"}) {
        const chess::Piece piece = board.at(chess::parse_square(square));
        if (piece.type == chess::PieceType::Knight || piece.type == chess::PieceType::Bishop)
            ++undeveloped;
    }
    if (queens == 0 && non_pawn_material <= 2600)
        return GamePhase::Endgame;
    if (ply < 20 && undeveloped >= 3 && non_pawn_material > 3000)
        return GamePhase::Opening;
    return GamePhase::Middlegame;
}

GameAnalysis Analyzer::analyze(const chess::Game& game, ProgressCallback progress,
                               std::stop_token stop_token) {
    if (game.plies.empty())
        throw Error(ErrorCode::InvalidArgument, "cannot analyze an empty game");
    const auto report = [&](AnalysisStage stage, std::size_t complete, std::size_t total,
                            std::string message) {
        if (progress)
            progress(Progress{stage, complete, total, std::move(message)});
    };
    report(AnalysisStage::Parsing, 1, 1, "Game reconstructed");

    GameAnalysis analysis;
    analysis.game_id = game.identity;
    analysis.moves.reserve(game.plies.size());
    std::vector<engine::AnalysisResult> before_results(game.plies.size());
    std::vector<engine::AnalysisResult> after_results(game.plies.size());

    for (std::size_t index = 0; index < game.plies.size(); ++index) {
        if (stop_token.stop_requested())
            throw Error(ErrorCode::Timeout, "analysis cancelled");
        engine::AnalysisRequest before_request{game.plies[index].fen_before, options_.shallow_depth,
                                               std::chrono::milliseconds(0), 2};
        engine::AnalysisRequest after_request{game.plies[index].fen_after, options_.shallow_depth,
                                              std::chrono::milliseconds(0), 2};
        before_results[index] = analyze_cached(before_request, stop_token);
        after_results[index] = analyze_cached(after_request, stop_token);
        chess::Board before = chess::Board::from_fen(game.plies[index].fen_before);
        chess::Board after = chess::Board::from_fen(game.plies[index].fen_after);
        const int before_white = score_for_white(before_results[index], before);
        const int after_white = score_for_white(after_results[index], after);
        const chess::Color mover = before.side_to_move();
        const int before_mover = mover == chess::Color::White ? before_white : -before_white;
        const int after_mover = mover == chess::Color::White ? after_white : -after_white;
        const int loss = std::max(0, before_mover - after_mover);
        const int material_before =
            before.material(mover) - before.material(chess::opposite(mover));
        const int material_after = after.material(mover) - after.material(chess::opposite(mover));
        analysis.moves.push_back(MoveAssessment{
            index,
            game.plies[index].san,
            game.plies[index].fen_before,
            game.plies[index].fen_after,
            before_white,
            after_white,
            loss,
            material_after - material_before,
            quality_for(game, index, loss, after),
            classify_phase(before, index),
            after_results[index].best_move,
        });
        report(AnalysisStage::ShallowScan, index + 1, game.plies.size(), "Scanning positions");
    }

    std::vector<std::size_t> candidates;
    for (std::size_t index = 0; index < analysis.moves.size(); ++index) {
        if (analysis.moves[index].loss >= options_.candidate_threshold_cp ||
            analysis.moves[index].material_delta <= -300) {
            candidates.push_back(index);
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [&](std::size_t left, std::size_t right) {
                         return analysis.moves[left].loss > analysis.moves[right].loss;
                     });
    if (candidates.size() > options_.max_deep_candidates)
        candidates.resize(options_.max_deep_candidates);

    for (std::size_t candidate_index = 0; candidate_index < candidates.size(); ++candidate_index) {
        const std::size_t index = candidates[candidate_index];
        engine::AnalysisRequest request{game.plies[index].fen_before, options_.deep_depth,
                                        std::chrono::milliseconds(0), 3};
        engine::AnalysisResult deep = analyze_cached(request, stop_token);
        const std::string category = category_for(game, index, deep, after_results[index]);
        std::vector<std::string> better_moves;
        for (const auto& line : deep.lines) {
            if (!line.moves.empty())
                better_moves.push_back(line.moves.front());
        }
        const std::string punishment = after_results[index].best_move;
        analysis.mistakes.push_back(Mistake{
            0,
            index,
            game.plies[index].san,
            game.plies[index].fen_before,
            analysis.moves[index].evaluation_before,
            analysis.moves[index].evaluation_after,
            analysis.moves[index].loss,
            analysis.moves[index].phase,
            category,
            explanation_for(category, game, index, punishment),
            punishment,
            std::move(better_moves),
            std::move(deep),
        });
        report(AnalysisStage::DeepAnalysis, candidate_index + 1, candidates.size(),
               "Deep analysis");
    }
    std::stable_sort(
        analysis.mistakes.begin(), analysis.mistakes.end(),
        [](const Mistake& left, const Mistake& right) { return left.loss > right.loss; });
    if (analysis.mistakes.size() > options_.top_mistakes) {
        analysis.mistakes.resize(options_.top_mistakes);
    }
    for (std::size_t index = 0; index < analysis.mistakes.size(); ++index) {
        analysis.mistakes[index].rank = index + 1;
    }
    report(AnalysisStage::Complete, 1, 1, "Analysis complete");
    return analysis;
}

std::string_view name(AnalysisStage stage) {
    switch (stage) {
    case AnalysisStage::Parsing:
        return "parsing";
    case AnalysisStage::ShallowScan:
        return "shallow_scan";
    case AnalysisStage::DeepAnalysis:
        return "deep_analysis";
    case AnalysisStage::Complete:
        return "complete";
    }
    return "unknown";
}

std::string_view name(GamePhase phase) {
    switch (phase) {
    case GamePhase::Opening:
        return "opening";
    case GamePhase::Middlegame:
        return "middlegame";
    case GamePhase::Endgame:
        return "endgame";
    }
    return "unknown";
}

std::string_view name(MoveQuality quality) {
    switch (quality) {
    case MoveQuality::Developing:
        return "developing";
    case MoveQuality::Capture:
        return "capture";
    case MoveQuality::Check:
        return "check";
    case MoveQuality::Recapture:
        return "recapture";
    case MoveQuality::Threat:
        return "threat";
    case MoveQuality::Neutral:
        return "neutral";
    case MoveQuality::Inaccuracy:
        return "inaccuracy";
    case MoveQuality::Mistake:
        return "mistake";
    case MoveQuality::Blunder:
        return "blunder";
    }
    return "unknown";
}

} // namespace pct::analysis
