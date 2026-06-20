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

bool piece_attacks(const chess::Board& board, chess::Square from, chess::Square target,
                   chess::Piece piece) {
    const int from_file = static_cast<int>(from % 8);
    const int from_rank = static_cast<int>(from / 8);
    const int to_file = static_cast<int>(target % 8);
    const int to_rank = static_cast<int>(target / 8);
    const int file_delta = to_file - from_file;
    const int rank_delta = to_rank - from_rank;
    if (piece.type == chess::PieceType::Pawn)
        return std::abs(file_delta) == 1 &&
               rank_delta == (piece.color == chess::Color::White ? 1 : -1);
    if (piece.type == chess::PieceType::Knight)
        return (std::abs(file_delta) == 1 && std::abs(rank_delta) == 2) ||
               (std::abs(file_delta) == 2 && std::abs(rank_delta) == 1);
    if (piece.type == chess::PieceType::King)
        return std::max(std::abs(file_delta), std::abs(rank_delta)) == 1;
    const bool diagonal = std::abs(file_delta) == std::abs(rank_delta);
    const bool straight = file_delta == 0 || rank_delta == 0;
    if (piece.type == chess::PieceType::Bishop && !diagonal)
        return false;
    if (piece.type == chess::PieceType::Rook && !straight)
        return false;
    if (piece.type == chess::PieceType::Queen && !diagonal && !straight)
        return false;
    if (!diagonal && !straight)
        return false;
    const int file_step = (file_delta > 0) - (file_delta < 0);
    const int rank_step = (rank_delta > 0) - (rank_delta < 0);
    int file = from_file + file_step;
    int rank = from_rank + rank_step;
    while (file != to_file || rank != to_rank) {
        if (!board.at(static_cast<chess::Square>(rank * 8 + file)).empty())
            return false;
        file += file_step;
        rank += rank_step;
    }
    return true;
}

std::string tactical_motif(chess::Board before, const chess::Move& best) {
    const chess::Color mover = before.side_to_move();
    const chess::Color enemy = chess::opposite(mover);
    const chess::Piece captured = before.at(best.to);
    if (!captured.empty()) {
        for (chess::Square defender_square = 0; defender_square < 64; ++defender_square) {
            const auto defender = before.at(defender_square);
            if (defender.empty() || defender.color != enemy || defender_square == best.to ||
                !piece_attacks(before, defender_square, best.to, defender))
                continue;
            int simultaneous_duties = 1;
            for (chess::Square target_square = 0; target_square < 64; ++target_square) {
                const auto target = before.at(target_square);
                if (target.empty() || target.color != enemy || target_square == best.to)
                    continue;
                if (piece_attacks(before, defender_square, target_square, defender) &&
                    before.is_square_attacked(target_square, mover))
                    ++simultaneous_duties;
            }
            if (simultaneous_duties >= 2)
                return "Overloaded defender";
        }
    }
    std::vector<chess::Square> newly_attacked;
    for (chess::Square square = 0; square < 64; ++square) {
        const auto target = before.at(square);
        if (!target.empty() && target.color != mover && !before.is_square_attacked(square, mover))
            newly_attacked.push_back(square);
    }
    before.make_move(best);
    const chess::Piece moved = before.at(best.to);
    if (!captured.empty()) {
        for (const chess::Square square : newly_attacked) {
            const auto target = before.at(square);
            if (!target.empty() && target.color == enemy && piece_value(target.type) >= 300 &&
                piece_attacks(before, best.to, square, moved))
                return "Removal of defender";
        }
    }
    int valuable_attacks = 0;
    for (chess::Square square = 0; square < 64; ++square) {
        const auto target = before.at(square);
        if (!target.empty() && target.color != mover && piece_value(target.type) >= 300 &&
            piece_attacks(before, best.to, square, moved))
            ++valuable_attacks;
    }
    if (valuable_attacks >= 2)
        return "Fork";
    for (const auto [file_step, rank_step] :
         {std::pair{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}}) {
        std::vector<chess::Piece> line;
        int file = static_cast<int>(best.to % 8) + file_step;
        int rank = static_cast<int>(best.to / 8) + rank_step;
        while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
            const auto piece = before.at(static_cast<chess::Square>(rank * 8 + file));
            if (!piece.empty()) {
                if (piece.color == mover)
                    break;
                line.push_back(piece);
                if (line.size() == 2)
                    break;
            }
            file += file_step;
            rank += rank_step;
        }
        if (line.size() == 2 && line[1].type == chess::PieceType::King)
            return "Pin";
        if (line.size() == 2 && piece_value(line[0].type) > piece_value(line[1].type))
            return "Skewer";
    }
    for (const chess::Square square : newly_attacked) {
        const auto target = before.at(square);
        if (!target.empty() && target.color != mover && before.is_square_attacked(square, mover) &&
            !piece_attacks(before, best.to, square, moved))
            return "Discovered attack";
    }
    if ((moved.type == chess::PieceType::Rook || moved.type == chess::PieceType::Queen) &&
        before.in_check(before.side_to_move())) {
        for (chess::Square square = 0; square < 64; ++square) {
            const auto piece = before.at(square);
            if (piece.type == chess::PieceType::King && piece.color != mover &&
                (square / 8 == 0 || square / 8 == 7))
                return "Back-rank weakness";
        }
    }
    return {};
}

bool has_dangerous_passed_pawn(const chess::Board& board, chess::Color color) {
    const chess::Color enemy = chess::opposite(color);
    for (chess::Square square = 0; square < 64; ++square) {
        const auto pawn = board.at(square);
        if (pawn.type != chess::PieceType::Pawn || pawn.color != color)
            continue;
        const int file = static_cast<int>(square % 8);
        const int rank = static_cast<int>(square / 8);
        if ((color == chess::Color::White && rank < 4) ||
            (color == chess::Color::Black && rank > 3))
            continue;
        bool blocked_by_pawn = false;
        for (chess::Square other = 0; other < 64; ++other) {
            const auto piece = board.at(other);
            if (piece.type != chess::PieceType::Pawn || piece.color != enemy)
                continue;
            const int other_file = static_cast<int>(other % 8);
            const int other_rank = static_cast<int>(other / 8);
            const bool ahead = color == chess::Color::White ? other_rank > rank : other_rank < rank;
            if (ahead && std::abs(other_file - file) <= 1) {
                blocked_by_pawn = true;
                break;
            }
        }
        if (!blocked_by_pawn)
            return true;
    }
    return false;
}

int king_shield_pawns(const chess::Board& board, chess::Color color) {
    chess::Square king = chess::no_square;
    for (chess::Square square = 0; square < 64; ++square) {
        const auto piece = board.at(square);
        if (piece.type == chess::PieceType::King && piece.color == color)
            king = square;
    }
    if (king == chess::no_square)
        return 0;
    const int rank_step = color == chess::Color::White ? 1 : -1;
    int count = 0;
    for (int file_delta = -1; file_delta <= 1; ++file_delta) {
        const int file = static_cast<int>(king % 8) + file_delta;
        const int rank = static_cast<int>(king / 8) + rank_step;
        if (file < 0 || file >= 8 || rank < 0 || rank >= 8)
            continue;
        const auto piece = board.at(static_cast<chess::Square>(rank * 8 + file));
        if (piece.type == chess::PieceType::Pawn && piece.color == color)
            ++count;
    }
    return count;
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

std::string classify_category(const chess::Game& game, std::size_t index,
                              const engine::AnalysisResult& deep,
                              const engine::AnalysisResult& after_result) {
    chess::Board before = chess::Board::from_fen(game.plies[index].fen_before);
    chess::Board after = chess::Board::from_fen(game.plies[index].fen_after);
    if (game.plies[index].elapsed_ms && *game.plies[index].elapsed_ms <= 2000)
        return "Instant-move blunder";
    if (index < 20 && game.plies[index].elapsed_ms && *game.plies[index].elapsed_ms >= 120000)
        return "Excessive early time use";
    if (game.plies[index].clock_ms && *game.plies[index].clock_ms <= 30000)
        return "Time-management failure";
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
            if (const std::string motif = tactical_motif(before, *best); !motif.empty())
                return motif;
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
    const chess::Piece moved = before.at(game.plies[index].move.from);
    const chess::Color mover = before.side_to_move();
    if (Analyzer::classify_phase(before, index) == GamePhase::Endgame) {
        if (has_dangerous_passed_pawn(before, chess::opposite(mover)) &&
            !game.plies[index].move.has(chess::Capture))
            return "Ignored passed pawn";
        if (moved.type == chess::PieceType::King &&
            std::abs(static_cast<int>(game.plies[index].move.to / 8) - 3) >
                std::abs(static_cast<int>(game.plies[index].move.from / 8) - 3))
            return "Incorrect king activity";
    }
    if (index >= 12 && king_shield_pawns(before, mover) <= 1)
        return "Open king position";
    if (index < 24) {
        if (moved.type == chess::PieceType::Queen && index < 12)
            return "Premature queen development";
        if (moved.type == chess::PieceType::Pawn) {
            const char file = chess::square_name(game.plies[index].move.from)[0];
            if ((file == 'a' || file == 'h') && index < 16)
                return "Unnecessary flank-pawn moves";
        }
        for (std::size_t earlier = index % 2; earlier + 2 <= index; earlier += 2) {
            if (game.plies[earlier].move.to == game.plies[index].move.from &&
                game.plies[earlier].move.from != game.plies[index].move.to)
                return "Repeated piece movement";
        }
        int undeveloped = 0;
        for (const std::string_view square : {"b1", "c1", "f1", "g1", "b8", "c8", "f8", "g8"}) {
            const chess::Piece piece = before.at(chess::parse_square(square));
            if (piece.type == chess::PieceType::Knight || piece.type == chess::PieceType::Bishop)
                ++undeveloped;
        }
        if (index >= 14 && undeveloped >= 4)
            return "Delayed development";
        const std::string king_square = before.side_to_move() == chess::Color::White ? "e1" : "e8";
        if (index >= 16 && before.at(chess::parse_square(king_square)).type == chess::PieceType::King &&
            before.castling_rights() != 0)
            return "Delayed castling";
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

std::string classify_tactical_motif(chess::Board board, const chess::Move& best_move) {
    return tactical_motif(std::move(board), best_move);
}

std::string classify_mistake_category(const chess::Game& game, std::size_t ply,
                                      const engine::AnalysisResult& best_before,
                                      const engine::AnalysisResult& best_after) {
    if (ply >= game.plies.size())
        throw Error(ErrorCode::InvalidArgument, "mistake ply is outside the game");
    return classify_category(game, ply, best_before, best_after);
}

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
    ++hits_;
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

std::size_t AnalysisCache::hit_count() const {
    std::lock_guard lock(mutex_);
    return hits_;
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
    int pawns = 0;
    int advanced_pawns = 0;
    for (chess::Square square = 0; square < 64; ++square) {
        const chess::Piece piece = board.at(square);
        if (piece.empty())
            continue;
        if (piece.type == chess::PieceType::Queen)
            ++queens;
        if (piece.type == chess::PieceType::Pawn) {
            ++pawns;
            const int rank = static_cast<int>(square / 8);
            if ((piece.color == chess::Color::White && rank >= 3) ||
                (piece.color == chess::Color::Black && rank <= 4))
                ++advanced_pawns;
        }
        if (piece.type != chess::PieceType::Pawn && piece.type != chess::PieceType::King) {
            non_pawn_material += piece_value(piece.type);
        }
    }
    for (const std::string_view square : {"b1", "c1", "f1", "g1", "b8", "c8", "f8", "g8"}) {
        const chess::Piece piece = board.at(chess::parse_square(square));
        if (piece.type == chess::PieceType::Knight || piece.type == chess::PieceType::Bishop)
            ++undeveloped;
    }
    const bool castling_still_relevant = board.castling_rights() != 0;
    const bool simplified_pawn_structure = pawns <= 10;
    if (queens == 0 && non_pawn_material <= 3200 &&
        (simplified_pawn_structure || ply >= 30))
        return GamePhase::Endgame;
    int opening_signals = 0;
    opening_signals += ply < 20 ? 1 : 0;
    opening_signals += undeveloped >= 3 ? 1 : 0;
    opening_signals += castling_still_relevant ? 1 : 0;
    opening_signals += advanced_pawns <= 6 ? 1 : 0;
    opening_signals += queens == 2 ? 1 : 0;
    if (opening_signals >= 4 && non_pawn_material > 3000)
        return GamePhase::Opening;
    return GamePhase::Middlegame;
}

GameAnalysis Analyzer::analyze_shallow(const chess::Game& game, ProgressCallback progress,
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
    const OpeningMatch opening = recognize_opening(game);
    analysis.eco = opening.eco;
    analysis.opening = opening.name;
    analysis.book_ply = opening.book_ply;
    analysis.departure_ply = opening.departure_ply;
    analysis.opening_book_version = opening.book_version;
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
    return analysis;
}

GameAnalysis Analyzer::analyze_deep(const chess::Game& game, GameAnalysis analysis,
                                    ProgressCallback progress, std::stop_token stop_token) {
    if (analysis.game_id != game.identity || analysis.moves.size() != game.plies.size())
        throw Error(ErrorCode::InvalidArgument,
                    "shallow analysis does not match the requested game");
    analysis.mistakes.clear();
    const auto report = [&](AnalysisStage stage, std::size_t complete, std::size_t total,
                            std::string message) {
        if (progress)
            progress(Progress{stage, complete, total, std::move(message)});
    };

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
        engine::AnalysisRequest after_request{game.plies[index].fen_after,
                                              options_.shallow_depth,
                                              std::chrono::milliseconds(0), 2};
        const engine::AnalysisResult after_result =
            analyze_cached(after_request, stop_token);
        const std::string category = classify_mistake_category(game, index, deep, after_result);
        std::vector<std::string> better_moves;
        for (const auto& line : deep.lines) {
            if (!line.moves.empty())
                better_moves.push_back(line.moves.front());
        }
        const std::string punishment = after_result.best_move;
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
            {},
            "proven",
            "taxonomy-2",
        });
        auto& classified = analysis.mistakes.back();
        classified.evidence.push_back("evaluation loss " + std::to_string(classified.loss) +
                                      " centipawns");
        if (!classified.punishment.empty())
            classified.evidence.push_back("strongest reply " + classified.punishment);
        classified.evidence.push_back("deterministic classifier rule " + classified.category);
        if (classified.category == "One-move tactical loss" ||
            classified.category == "Premature queen development" ||
            classified.category == "Unnecessary flank-pawn moves" ||
            classified.category == "Repeated piece movement" ||
            classified.category == "Delayed development" ||
            classified.category == "Delayed castling" ||
            classified.category == "Open king position" ||
            classified.category == "Ignored passed pawn" ||
            classified.category == "Incorrect king activity")
            classified.confidence = "suggestive";
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

GameAnalysis Analyzer::analyze(const chess::Game& game, ProgressCallback progress,
                               std::stop_token stop_token) {
    GameAnalysis shallow = analyze_shallow(game, progress, stop_token);
    return analyze_deep(game, std::move(shallow), progress, stop_token);
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
