#include "pct/app/repository.hpp"

#include "pct/common/error.hpp"
#include "pct/common/log.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <utility>

namespace pct::app {
namespace {

std::string method_name(import::ImportMethod method) {
    switch (method) {
    case import::ImportMethod::PublicApi:
        return "public_api";
    case import::ImportMethod::PublicPage:
        return "public_page";
    case import::ImportMethod::ManualPgn:
        return "manual_pgn";
    }
    return "manual_pgn";
}

import::ImportMethod parse_method(std::string_view method) {
    if (method == "public_api")
        return import::ImportMethod::PublicApi;
    if (method == "public_page")
        return import::ImportMethod::PublicPage;
    return import::ImportMethod::ManualPgn;
}

std::optional<chess::Move> parse_uci(chess::Board& board, std::string_view uci) {
    if (uci.size() < 4)
        return std::nullopt;
    chess::PieceType promotion = chess::PieceType::Queen;
    if (uci.size() >= 5 && uci[4] == 'n') promotion = chess::PieceType::Knight;
    if (uci.size() >= 5 && uci[4] == 'b') promotion = chess::PieceType::Bishop;
    if (uci.size() >= 5 && uci[4] == 'r') promotion = chess::PieceType::Rook;
    try {
        return board.find_legal_move(chess::parse_square(uci.substr(0, 2)),
                                     chess::parse_square(uci.substr(2, 2)), promotion);
    } catch (const Error&) {
        return std::nullopt;
    }
}

std::string piece_name(chess::PieceType type) {
    switch (type) {
    case chess::PieceType::Pawn: return "pawn";
    case chess::PieceType::Knight: return "knight";
    case chess::PieceType::Bishop: return "bishop";
    case chess::PieceType::Rook: return "rook";
    case chess::PieceType::Queen: return "queen";
    case chess::PieceType::King: return "king";
    case chess::PieceType::None: return "piece";
    }
    return "piece";
}

std::vector<std::string> attacked_piece_descriptions(const chess::Board& board,
                                                     chess::Color owner) {
    std::vector<std::string> result;
    for (chess::Square square = 0; square < 64; ++square) {
        const chess::Piece piece = board.at(square);
        if (piece.empty() || piece.color != owner ||
            !board.is_square_attacked(square, chess::opposite(owner)))
            continue;
        result.push_back(piece_name(piece.type) + " on " + chess::square_name(square));
    }
    return result;
}

json::Value line_json(const engine::PrincipalVariation& line) {
    json::Value::Array moves;
    for (const std::string& move : line.moves)
        moves.emplace_back(move);
    json::Value::Object value{
        {"multipv", line.multipv},
        {"depth", line.depth},
        {"nodes", static_cast<double>(line.nodes)},
        {"time_ms", static_cast<double>(line.time_ms)},
        {"moves", std::move(moves)},
    };
    value.emplace("centipawns", line.centipawns ? json::Value(*line.centipawns) : json::Value{});
    value.emplace("mate", line.mate ? json::Value(*line.mate) : json::Value{});
    return value;
}

json::Value engine_json(const engine::AnalysisResult& result) {
    json::Value::Array lines;
    for (const auto& line : result.lines)
        lines.push_back(line_json(line));
    return json::Value::Object{
        {"best_move", result.best_move},
        {"ponder_move", result.ponder_move},
        {"lines", std::move(lines)},
    };
}

engine::AnalysisResult engine_from_json(const json::Value& value) {
    engine::AnalysisResult result;
    result.best_move = value.at("best_move").as_string();
    result.ponder_move = value.at("ponder_move").as_string();
    for (const auto& item : value.at("lines").as_array()) {
        engine::PrincipalVariation line;
        line.multipv = item.at("multipv").as_int();
        line.depth = item.at("depth").as_int();
        if (!item.at("centipawns").is_null())
            line.centipawns = item.at("centipawns").as_int();
        if (!item.at("mate").is_null())
            line.mate = item.at("mate").as_int();
        line.nodes = static_cast<std::uint64_t>(item.at("nodes").as_number());
        line.time_ms = static_cast<std::uint64_t>(item.at("time_ms").as_number());
        for (const auto& move : item.at("moves").as_array())
            line.moves.push_back(move.as_string());
        result.lines.push_back(std::move(line));
    }
    return result;
}

json::Value move_json(const analysis::MoveAssessment& move) {
    return json::Value::Object{
        {"ply", move.ply},
        {"san", move.san},
        {"fen_before", move.fen_before},
        {"fen_after", move.fen_after},
        {"evaluation_before", move.evaluation_before},
        {"evaluation_after", move.evaluation_after},
        {"loss", move.loss},
        {"material_delta", move.material_delta},
        {"quality", std::string(analysis::name(move.quality))},
        {"phase", std::string(analysis::name(move.phase))},
        {"best_response", move.best_response},
    };
}

analysis::GamePhase parse_phase(std::string_view value) {
    if (value == "opening")
        return analysis::GamePhase::Opening;
    if (value == "endgame")
        return analysis::GamePhase::Endgame;
    return analysis::GamePhase::Middlegame;
}

analysis::MoveQuality parse_quality(std::string_view value) {
    if (value == "developing")
        return analysis::MoveQuality::Developing;
    if (value == "capture")
        return analysis::MoveQuality::Capture;
    if (value == "check")
        return analysis::MoveQuality::Check;
    if (value == "recapture")
        return analysis::MoveQuality::Recapture;
    if (value == "threat")
        return analysis::MoveQuality::Threat;
    if (value == "inaccuracy")
        return analysis::MoveQuality::Inaccuracy;
    if (value == "mistake")
        return analysis::MoveQuality::Mistake;
    if (value == "blunder")
        return analysis::MoveQuality::Blunder;
    return analysis::MoveQuality::Neutral;
}

analysis::MoveAssessment move_from_json(const json::Value& value) {
    return analysis::MoveAssessment{
        value.at("ply").as_size(),
        value.at("san").as_string(),
        value.at("fen_before").as_string(),
        value.at("fen_after").as_string(),
        value.at("evaluation_before").as_int(),
        value.at("evaluation_after").as_int(),
        value.at("loss").as_int(),
        value.at("material_delta").as_int(),
        parse_quality(value.at("quality").as_string()),
        parse_phase(value.at("phase").as_string()),
        value.at("best_response").as_string(),
    };
}

json::Value mistake_json(const analysis::Mistake& mistake) {
    json::Value::Array better;
    for (const auto& move : mistake.better_moves)
        better.emplace_back(move);
    json::Value::Array evidence;
    for (const auto& item : mistake.evidence)
        evidence.emplace_back(item);
    return json::Value::Object{
        {"rank", mistake.rank},
        {"ply", mistake.ply},
        {"san", mistake.san},
        {"fen", mistake.fen},
        {"evaluation_before", mistake.evaluation_before},
        {"evaluation_after", mistake.evaluation_after},
        {"loss", mistake.loss},
        {"phase", std::string(analysis::name(mistake.phase))},
        {"category", mistake.category},
        {"explanation", mistake.explanation},
        {"punishment", mistake.punishment},
        {"better_moves", std::move(better)},
        {"engine", engine_json(mistake.engine_details)},
        {"evidence", std::move(evidence)},
        {"confidence", mistake.confidence},
        {"classifier_version", mistake.classifier_version},
    };
}

analysis::Mistake mistake_from_json(const json::Value& value) {
    analysis::Mistake mistake;
    mistake.rank = value.at("rank").as_size();
    mistake.ply = value.at("ply").as_size();
    mistake.san = value.at("san").as_string();
    mistake.fen = value.at("fen").as_string();
    mistake.evaluation_before = value.at("evaluation_before").as_int();
    mistake.evaluation_after = value.at("evaluation_after").as_int();
    mistake.loss = value.at("loss").as_int();
    mistake.phase = parse_phase(value.at("phase").as_string());
    mistake.category = value.at("category").as_string();
    mistake.explanation = value.at("explanation").as_string();
    mistake.punishment = value.at("punishment").as_string();
    for (const auto& move : value.at("better_moves").as_array()) {
        mistake.better_moves.push_back(move.as_string());
    }
    mistake.engine_details = engine_from_json(value.at("engine"));
    for (const auto& item : value.get("evidence", json::Value::Array{}).as_array())
        mistake.evidence.push_back(item.as_string());
    mistake.confidence = value.get("confidence", "proven").as_string();
    mistake.classifier_version = value.get("classifier_version", "taxonomy-1").as_string();
    return mistake;
}

training::Drill drill_from_json(const json::Value& payload) {
    training::Drill drill;
    drill.id = payload.at("id").as_string();
    drill.source_game_id = payload.at("source_game_id").as_string();
    drill.source_ply = payload.at("source_ply").as_size();
    drill.fen = payload.at("fen").as_string();
    drill.category = payload.at("category").as_string();
    drill.phase = payload.at("phase").as_string();
    drill.explanation = payload.at("explanation").as_string();
    drill.punishment = payload.at("punishment").as_string();
    drill.difficulty = payload.at("difficulty").as_int();
    drill.impact_cp = payload.at("impact_cp").as_int();
    drill.created_at_ms = static_cast<std::int64_t>(payload.at("created_at_ms").as_number());
    for (const auto& move : payload.at("solutions").as_array())
        drill.solutions.push_back(move.as_string());
    const json::Value empty_attempts{json::Value::Array{}};
    for (const auto& item : payload.get("attempts", empty_attempts).as_array()) {
        drill.attempts.push_back(training::DrillAttempt{
            static_cast<std::uint64_t>(item.at("id").as_number()),
            static_cast<std::int64_t>(item.at("attempted_at_ms").as_number()),
            item.at("correct").as_bool(), item.at("move").as_string(),
            static_cast<std::uint64_t>(item.at("response_time_ms").as_number()),
            item.at("hint_level").as_int(), item.at("retries").as_size()});
    }
    drill.played_move = payload.get("played_move", "").as_string();
    drill.fen_after_mistake = payload.get("fen_after_mistake", drill.fen).as_string();
    drill.fen_after_punishment =
        payload.get("fen_after_punishment", drill.fen_after_mistake).as_string();
    drill.session_hint_level = payload.get("session_hint_level", 0).as_int();
    drill.session_started_at_ms = static_cast<std::int64_t>(
        payload.get("session_started_at_ms", 0).as_number());
    drill.changed_threat =
        payload.get("changed_threat", "The opponent's strongest reply is " + drill.punishment + ".")
            .as_string();
    const json::Value empty_pieces{json::Value::Array{}};
    for (const auto& piece : payload.get("attacked_pieces", empty_pieces).as_array())
        drill.attacked_pieces.push_back(piece.as_string());
    drill.opponent_response = payload.get("opponent_response", drill.punishment).as_string();
    return drill;
}

json::Value imported_event(const import::ImportedGame& imported) {
    return json::Value::Object{
        {"game_id", imported.game.identity},
        {"source_url", imported.source_url},
        {"pgn", imported.pgn},
        {"method", method_name(imported.method)},
    };
}

void write_index(const std::filesystem::path& path, const json::Value& value) {
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            throw Error(ErrorCode::IoError, "failed to create derived index " + path.string());
        output << json::dump(value);
        output.flush();
        if (!output)
            throw Error(ErrorCode::IoError, "failed to write derived index " + path.string());
    }
    std::filesystem::rename(temporary, path);
}

std::string hash_string(std::uint64_t hash) {
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

} // namespace

Repository::Repository(storage::EventLog& log) : log_(log) {
    replay();
    rebuild_indexes();
}

void Repository::replay() {
    const storage::ReplayResult events = log_.replay();
    std::uint64_t snapshot_event_id = 0;
    const std::filesystem::path snapshot_directory = log_.path().parent_path() / "snapshots";
    if (std::filesystem::exists(snapshot_directory)) {
        for (const auto& entry : std::filesystem::directory_iterator(snapshot_directory)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;
            try {
                std::ifstream input(entry.path());
                std::stringstream contents;
                contents << input.rdbuf();
                const json::Value snapshot = json::parse(contents.str());
                const auto event_id = static_cast<std::uint64_t>(snapshot.at("last_event_id").as_number());
                if (snapshot.at("snapshot_version").as_int() != 1 || event_id <= snapshot_event_id)
                    continue;
                std::map<std::string, StoredGame> games;
                std::map<std::string, training::Drill> drills;
                std::map<std::string, std::int64_t> completions;
                std::map<std::string, std::string> job_states;
                std::map<std::string, json::Value> batches;
                std::set<std::string> recommendations;
                const json::Value null_value;
                for (const auto& value : snapshot.at("games").as_array()) {
                    const std::string pgn = value.at("pgn").as_string();
                    import::ImportedGame imported{chess::parse_pgn(pgn),
                                                  value.at("source_url").as_string(), pgn,
                                                  parse_method(value.at("import_method").as_string())};
                    std::optional<analysis::GameAnalysis> completed;
                    const std::string analysis_status =
                        value.get("analysis_status", "pending").as_string();
                    if (analysis_status == "complete" && !value.at("analysis").is_null())
                        completed = analysis_from_json(value.at("analysis"));
                    std::optional<analysis::GameAnalysis> shallow;
                    if (analysis_status == "shallow" &&
                        !value.get("shallow_analysis", null_value).is_null())
                        shallow = analysis_from_json(value.at("shallow_analysis"));
                    const std::string game_id = imported.game.identity;
                    games.emplace(
                        game_id,
                        StoredGame{std::move(imported), std::move(completed),
                                   static_cast<std::int64_t>(
                                       value.get("imported_at_ms", 0).as_number()),
                                   static_cast<std::int64_t>(
                                       value.get("analyzed_at_ms", 0).as_number()),
                                   std::move(shallow)});
                }
                for (const auto& value : snapshot.at("drills").as_array()) {
                    training::Drill drill = drill_from_json(value);
                    drills.emplace(drill.id, std::move(drill));
                }
                for (const auto& value : snapshot.at("resource_completions").as_array())
                    completions.emplace(value.at("resource_id").as_string(),
                                        static_cast<std::int64_t>(value.at("completed_at_ms").as_number()));
                const json::Value empty_jobs{json::Value::Array{}};
                for (const auto& value : snapshot.get("analysis_jobs", empty_jobs).as_array())
                    job_states.emplace(value.at("game_id").as_string(),
                                       value.at("status").as_string());
                const json::Value empty_batches{json::Value::Array{}};
                for (const auto& value : snapshot.get("batches", empty_batches).as_array())
                    batches.emplace(value.at("id").as_string(), value);
                const json::Value empty_recommendations{json::Value::Array{}};
                for (const auto& value :
                     snapshot.get("recommended_resources", empty_recommendations).as_array())
                    recommendations.insert(value.as_string());
                games_ = std::move(games);
                drills_ = std::move(drills);
                resource_completions_ = std::move(completions);
                analysis_job_states_ = std::move(job_states);
                batches_ = std::move(batches);
                recommended_resources_ = std::move(recommendations);
                for (const auto& [_, batch] : batches_)
                    next_batch_id_ = std::max(
                        next_batch_id_,
                        static_cast<std::uint64_t>(batch.at("sequence").as_number()) + 1);
                background_paused_ = snapshot.get("background_paused", false).as_bool();
                for (const auto& [_, drill] : drills_)
                    for (const auto& attempt : drill.attempts)
                        next_attempt_id_ = std::max(next_attempt_id_, attempt.id + 1);
                snapshot_event_id = event_id;
            } catch (const std::exception& error) {
                log(LogLevel::Warning, "repository",
                    "skipping invalid snapshot " + entry.path().filename().string() + ": " +
                        error.what());
                continue;
            }
        }
    }
    for (const storage::Event& event : events.events) {
        if (event.id <= snapshot_event_id)
            continue;
        try {
            const json::Value payload = json::parse(event.payload);
            if (event.type == storage::EventType::GameImported) {
                const std::string pgn = payload.at("pgn").as_string();
                import::ImportedGame imported{
                    chess::parse_pgn(pgn),
                    payload.at("source_url").as_string(),
                    pgn,
                    parse_method(payload.at("method").as_string()),
                };
                const std::string id = imported.game.identity;
                games_.insert_or_assign(
                    id, StoredGame{std::move(imported), std::nullopt, event.timestamp_ms, 0,
                                   std::nullopt});
            } else if (event.type == storage::EventType::AnalysisCompleted) {
                const std::string id = payload.at("game_id").as_string();
                const auto found = games_.find(id);
                if (found != games_.end())
                    found->second.analysis = analysis_from_json(payload.at("analysis"));
                if (found != games_.end())
                    found->second.analyzed_at_ms = event.timestamp_ms;
            } else if (event.type == storage::EventType::ShallowAnalysisCompleted) {
                const auto found = games_.find(payload.at("game_id").as_string());
                if (found != games_.end() && !found->second.analysis)
                    found->second.shallow_analysis =
                        analysis_from_json(payload.at("analysis"));
            } else if (event.type == storage::EventType::DrillCreated) {
                training::Drill drill = drill_from_json(payload);
                drills_.insert_or_assign(drill.id, std::move(drill));
            } else if (event.type == storage::EventType::DrillAttempted) {
                const auto found = drills_.find(payload.at("drill_id").as_string());
                if (found != drills_.end()) {
                    training::DrillAttempt attempt;
                    attempt.id = static_cast<std::uint64_t>(payload.at("attempt_id").as_number());
                    attempt.attempted_at_ms = static_cast<std::int64_t>(payload.at("attempted_at_ms").as_number());
                    attempt.correct = payload.at("correct").as_bool();
                    attempt.move = payload.at("move").as_string();
                    attempt.response_time_ms = static_cast<std::uint64_t>(payload.at("response_time_ms").as_number());
                    attempt.hint_level = payload.at("hint_level").as_int();
                    attempt.retries = payload.at("retries").as_size();
                    next_attempt_id_ = std::max(next_attempt_id_, attempt.id + 1);
                    found->second.attempts.push_back(std::move(attempt));
                }
            } else if (event.type == storage::EventType::DrillSessionUpdated) {
                const auto found = drills_.find(payload.at("drill_id").as_string());
                if (found != drills_.end()) {
                    found->second.session_hint_level = payload.at("hint_level").as_int();
                    found->second.session_started_at_ms = static_cast<std::int64_t>(
                        payload.at("started_at_ms").as_number());
                }
            } else if (event.type == storage::EventType::ResourceCompleted) {
                resource_completions_.insert_or_assign(
                    payload.at("resource_id").as_string(),
                    static_cast<std::int64_t>(payload.at("completed_at_ms").as_number()));
            } else if (event.type == storage::EventType::ResourceRecommended) {
                recommended_resources_.insert(payload.at("resource_id").as_string());
            } else if (event.type == storage::EventType::AnalysisJobStateChanged) {
                analysis_job_states_.insert_or_assign(payload.at("game_id").as_string(),
                                                      payload.at("status").as_string());
            } else if (event.type == storage::EventType::BatchStateChanged) {
                if (payload.as_object().contains("paused"))
                    background_paused_ = payload.at("paused").as_bool();
            } else if (event.type == storage::EventType::BatchCreated) {
                const std::string id = payload.at("id").as_string();
                batches_.insert_or_assign(id, payload);
                next_batch_id_ = std::max(next_batch_id_,
                                          static_cast<std::uint64_t>(payload.at("sequence").as_number()) + 1);
            }
        } catch (const Error& error) {
            log(LogLevel::Warning, "repository",
                "skipping event " + std::to_string(event.id) + ": " + error.what());
        }
    }
}

AddResult Repository::add(const import::ImportedGame& imported) {
    std::lock_guard lock(mutex_);
    if (games_.contains(imported.game.identity))
        return AddResult::Duplicate;
    static_cast<void>(
        log_.append(storage::EventType::GameImported, json::dump(imported_event(imported))));
    static_cast<void>(
        log_.append(storage::EventType::GameParsed, json::dump(json::Value::Object{
                                                        {"game_id", imported.game.identity},
                                                        {"plies", imported.game.plies.size()},
                                                    })));
    const auto imported_at = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
    games_.emplace(imported.game.identity,
                   StoredGame{imported, std::nullopt, imported_at, 0, std::nullopt});
    for (const auto& color : {std::string("White"), std::string("Black")}) {
        const std::string rating = imported.game.tag(color + "Elo");
        if (rating.empty())
            continue;
        try {
            const int value = std::stoi(rating);
            static_cast<void>(log_.append(
                storage::EventType::RatingObserved,
                json::dump(json::Value::Object{{"game_id", imported.game.identity},
                                                {"player", imported.game.tag(color)},
                                                {"color", color == "White" ? "white" : "black"},
                                                {"rating", value}})));
        } catch (const std::exception&) {
        }
    }
    rebuild_indexes();
    return AddResult::Added;
}

void Repository::save_analysis(const analysis::GameAnalysis& analysis) {
    std::unique_lock lock(mutex_);
    const auto found = games_.find(analysis.game_id);
    if (found == games_.end())
        throw Error(ErrorCode::NotFound, "cannot save analysis for unknown game");
    for (const auto& move : analysis.moves) {
        static_cast<void>(
            log_.append(storage::EventType::PositionAnalyzed, json::dump(json::Value::Object{
                                                                  {"game_id", analysis.game_id},
                                                                  {"move", move_json(move)},
                                                              })));
    }
    for (const auto& mistake : analysis.mistakes) {
        json::Value::Array classification_evidence;
        for (const auto& evidence : mistake.evidence)
            classification_evidence.emplace_back(evidence);
        static_cast<void>(
            log_.append(storage::EventType::MistakeDetected, json::dump(json::Value::Object{
                                                                 {"game_id", analysis.game_id},
                                                                 {"mistake", mistake_json(mistake)},
                                                             })));
        static_cast<void>(
            log_.append(storage::EventType::MistakeClassified, json::dump(json::Value::Object{
                                                                   {"game_id", analysis.game_id},
                                                                   {"ply", mistake.ply},
                                                                   {"category", mistake.category},
                                                                   {"evidence", std::move(classification_evidence)},
                                                                   {"confidence", mistake.confidence},
                                                                   {"classifier_version", mistake.classifier_version},
                                                               })));
        static_cast<void>(log_.append(storage::EventType::ExplanationCreated,
                                      json::dump(json::Value::Object{
                                          {"game_id", analysis.game_id},
                                          {"ply", mistake.ply},
                                          {"explanation", mistake.explanation},
                                      })));
    }
    static_cast<void>(
        log_.append(storage::EventType::AnalysisCompleted, json::dump(json::Value::Object{
                                                               {"game_id", analysis.game_id},
                                                               {"analysis", to_json(analysis)},
                                                           })));
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    for (const auto& mistake : analysis.mistakes) {
        const std::string id = analysis.game_id + ":" + std::to_string(mistake.ply);
        if (drills_.contains(id) || mistake.better_moves.empty())
            continue;
        std::set<std::string> engine_moves;
        for (const auto& line : mistake.engine_details.lines)
            if (!line.moves.empty())
                engine_moves.insert(line.moves.front());
        std::vector<std::string> verified_solutions;
        chess::Board solution_board = chess::Board::from_fen(mistake.fen);
        for (const auto& solution : mistake.better_moves) {
            if (!engine_moves.contains(solution))
                continue;
            chess::Board candidate_board = solution_board;
            if (parse_uci(candidate_board, solution))
                verified_solutions.push_back(solution);
        }
        if (verified_solutions.empty())
            continue;
        training::Drill drill;
        drill.id = id;
        drill.source_game_id = analysis.game_id;
        drill.source_ply = mistake.ply;
        drill.fen = mistake.fen;
        drill.category = mistake.category;
        drill.phase = std::string(analysis::name(mistake.phase));
        drill.explanation = mistake.explanation;
        drill.punishment = mistake.punishment;
        drill.solutions = std::move(verified_solutions);
        drill.difficulty = std::clamp(1 + mistake.loss / 150, 1, 5);
        drill.impact_cp = mistake.loss;
        drill.created_at_ms = now;
        drill.opponent_response = mistake.punishment;
        if (mistake.ply < found->second.imported.game.plies.size()) {
            drill.played_move = chess::uci(found->second.imported.game.plies[mistake.ply].move);
            chess::Board lesson_board = chess::Board::from_fen(drill.fen);
            if (const auto played = parse_uci(lesson_board, drill.played_move)) {
                static_cast<void>(lesson_board.make_move(*played));
                drill.fen_after_mistake = lesson_board.to_fen();
                drill.attacked_pieces = attacked_piece_descriptions(
                    lesson_board, chess::opposite(lesson_board.side_to_move()));
                if (const auto reply = parse_uci(lesson_board, drill.punishment)) {
                    const chess::Piece captured =
                        reply->has(chess::EnPassant)
                            ? chess::Piece{chess::opposite(lesson_board.side_to_move()),
                                           chess::PieceType::Pawn}
                            : lesson_board.at(reply->to);
                    static_cast<void>(lesson_board.make_move(*reply));
                    drill.fen_after_punishment = lesson_board.to_fen();
                    const bool check = lesson_board.in_check(lesson_board.side_to_move());
                    const bool mate = check && lesson_board.legal_moves().empty();
                    if (mate) {
                        drill.changed_threat = "The reply " + drill.punishment + " is checkmate.";
                    } else if (!captured.empty()) {
                        drill.changed_threat = "The reply " + drill.punishment + " captures your " +
                                               piece_name(captured.type) + " on " +
                                               chess::square_name(reply->to) + ".";
                    } else if (check) {
                        drill.changed_threat =
                            "The reply " + drill.punishment + " gives check and takes the initiative.";
                    } else {
                        drill.changed_threat =
                            "The opponent's strongest reply is " + drill.punishment + ".";
                    }
                } else {
                    drill.fen_after_punishment = drill.fen_after_mistake;
                }
            }
        }
        if (drill.changed_threat.empty())
            drill.changed_threat = "The opponent's strongest reply is " + drill.punishment + ".";
        json::Value::Array solutions;
        for (const auto& solution : drill.solutions)
            solutions.emplace_back(solution);
        json::Value::Array attacked_pieces;
        for (const auto& piece : drill.attacked_pieces)
            attacked_pieces.emplace_back(piece);
        static_cast<void>(log_.append(
            storage::EventType::DrillCreated,
            json::dump(json::Value::Object{
                {"id", drill.id}, {"source_game_id", drill.source_game_id},
                {"source_ply", drill.source_ply}, {"fen", drill.fen},
                {"category", drill.category}, {"phase", drill.phase},
                {"explanation", drill.explanation}, {"punishment", drill.punishment},
                {"solutions", std::move(solutions)}, {"difficulty", drill.difficulty},
                {"impact_cp", drill.impact_cp},
                {"created_at_ms", static_cast<double>(drill.created_at_ms)},
                {"played_move", drill.played_move},
                {"fen_after_mistake", drill.fen_after_mistake},
                {"fen_after_punishment", drill.fen_after_punishment},
                {"changed_threat", drill.changed_threat},
                {"attacked_pieces", std::move(attacked_pieces)},
                {"opponent_response", drill.opponent_response},
                {"classifier_version", "taxonomy-2"},
            })));
        drills_.emplace(id, std::move(drill));
    }
    found->second.analysis = analysis;
    found->second.shallow_analysis.reset();
    found->second.analyzed_at_ms = now;
    rebuild_indexes();
    const std::size_t analyzed_count = static_cast<std::size_t>(std::count_if(
        games_.begin(), games_.end(), [](const auto& entry) { return entry.second.analysis.has_value(); }));
    lock.unlock();
    if (analyzed_count > 0 && analyzed_count % 10 == 0)
        static_cast<void>(create_snapshot());
}

void Repository::save_shallow_analysis(const analysis::GameAnalysis& analysis) {
    std::lock_guard lock(mutex_);
    const auto found = games_.find(analysis.game_id);
    if (found == games_.end())
        throw Error(ErrorCode::NotFound,
                    "cannot save shallow analysis for unknown game");
    if (found->second.analysis)
        return;
    static_cast<void>(log_.append(
        storage::EventType::ShallowAnalysisCompleted,
        json::dump(json::Value::Object{{"game_id", analysis.game_id},
                                       {"analysis", to_json(analysis)}})));
    found->second.shallow_analysis = analysis;
    rebuild_indexes();
}

std::optional<StoredGame> Repository::get(std::string_view id) const {
    std::lock_guard lock(mutex_);
    const auto found = games_.find(std::string(id));
    if (found == games_.end())
        return std::nullopt;
    return found->second;
}

std::vector<StoredGame> Repository::list() const {
    std::lock_guard lock(mutex_);
    std::vector<StoredGame> result;
    result.reserve(games_.size());
    for (const auto& [_, game] : games_)
        result.push_back(game);
    return result;
}

std::size_t Repository::size() const {
    std::lock_guard lock(mutex_);
    return games_.size();
}

std::vector<training::Drill> Repository::drills(std::int64_t now_ms) const {
    std::lock_guard lock(mutex_);
    std::vector<training::Drill> result;
    for (const auto& [_, drill] : drills_)
        result.push_back(drill);
    return training::review_queue(std::move(result), now_ms);
}

std::optional<training::Drill> Repository::drill(std::string_view id) const {
    std::lock_guard lock(mutex_);
    const auto found = drills_.find(std::string(id));
    return found == drills_.end() ? std::nullopt : std::optional<training::Drill>(found->second);
}

training::DrillAttempt Repository::record_attempt(std::string_view drill_id, std::string move,
                                                  std::uint64_t response_time_ms, int hint_level,
                                                  std::int64_t attempted_at_ms) {
    std::lock_guard lock(mutex_);
    const auto found = drills_.find(std::string(drill_id));
    if (found == drills_.end())
        throw Error(ErrorCode::NotFound, "drill does not exist");
    chess::Board board = chess::Board::from_fen(found->second.fen);
    bool legal = false;
    for (const auto& candidate : board.legal_moves()) {
        if (chess::uci(candidate) == move) {
            legal = true;
            break;
        }
    }
    if (!legal)
        throw Error(ErrorCode::InvalidArgument, "attempt move is not legal in the drill position");
    const bool correct =
        std::find(found->second.solutions.begin(), found->second.solutions.end(), move) !=
        found->second.solutions.end();
    std::size_t retries = 0;
    for (auto it = found->second.attempts.rbegin();
         it != found->second.attempts.rend() && !it->correct; ++it)
        ++retries;
    static_cast<void>(hint_level);
    const std::uint64_t measured_response = found->second.session_started_at_ms > 0 &&
                                                    attempted_at_ms >=
                                                        found->second.session_started_at_ms
                                                ? static_cast<std::uint64_t>(
                                                      attempted_at_ms -
                                                      found->second.session_started_at_ms)
                                                : response_time_ms;
    training::DrillAttempt attempt{next_attempt_id_++, attempted_at_ms, correct, std::move(move),
                                   measured_response, found->second.session_hint_level, retries};
    static_cast<void>(log_.append(
        storage::EventType::DrillAttempted,
        json::dump(json::Value::Object{
            {"drill_id", found->second.id}, {"attempt_id", static_cast<double>(attempt.id)},
            {"attempted_at_ms", static_cast<double>(attempt.attempted_at_ms)},
            {"correct", attempt.correct}, {"move", attempt.move},
            {"response_time_ms", static_cast<double>(attempt.response_time_ms)},
            {"hint_level", attempt.hint_level}, {"retries", attempt.retries},
            {"scheduler_version", std::string(training::scheduler_version)},
        })));
    found->second.attempts.push_back(attempt);
    if (correct) {
        found->second.session_hint_level = 0;
        found->second.session_started_at_ms = 0;
        static_cast<void>(log_.append(
            storage::EventType::DrillSessionUpdated,
            json::dump(json::Value::Object{{"drill_id", found->second.id},
                                            {"hint_level", 0},
                                            {"started_at_ms", 0}})));
    }
    rebuild_indexes();
    return attempt;
}

training::Drill Repository::begin_drill_session(std::string_view drill_id,
                                                std::int64_t now_ms) {
    std::lock_guard lock(mutex_);
    const auto found = drills_.find(std::string(drill_id));
    if (found == drills_.end())
        throw Error(ErrorCode::NotFound, "drill does not exist");
    if (found->second.session_started_at_ms == 0) {
        found->second.session_started_at_ms = now_ms;
        static_cast<void>(log_.append(
            storage::EventType::DrillSessionUpdated,
            json::dump(json::Value::Object{
                {"drill_id", found->second.id},
                {"hint_level", found->second.session_hint_level},
                {"started_at_ms", static_cast<double>(now_ms)},
            })));
    }
    return found->second;
}

training::Drill Repository::advance_hint(std::string_view drill_id, std::int64_t now_ms) {
    std::lock_guard lock(mutex_);
    const auto found = drills_.find(std::string(drill_id));
    if (found == drills_.end())
        throw Error(ErrorCode::NotFound, "drill does not exist");
    static_cast<void>(now_ms);
    const int available = training::available_hint_level(found->second);
    if (found->second.session_hint_level >= available)
        throw Error(ErrorCode::InvalidArgument,
                    "another failed attempt is required before the next hint");
    found->second.session_hint_level =
        std::min(3, found->second.session_hint_level + 1);
    static_cast<void>(log_.append(
        storage::EventType::DrillSessionUpdated,
        json::dump(json::Value::Object{
            {"drill_id", found->second.id},
            {"hint_level", found->second.session_hint_level},
            {"started_at_ms", static_cast<double>(found->second.session_started_at_ms)},
        })));
    return found->second;
}

training::Profile Repository::profile_unlocked() const {
    training::Profile result;
    result.games_imported = games_.size();
    std::map<std::string, std::size_t> player_frequency;
    for (const auto& [_, game] : games_) {
        const std::string white = game.imported.game.tag("White");
        const std::string black = game.imported.game.tag("Black");
        if (!white.empty()) ++player_frequency[white];
        if (!black.empty()) ++player_frequency[black];
    }
    for (const auto& [player, count] : player_frequency)
        if (result.player_name.empty() || count > player_frequency[result.player_name])
            result.player_name = player;
    std::map<std::string, training::Weakness> weaknesses;
    std::map<std::string, std::set<std::string>> category_games;
    std::map<std::string, std::vector<std::int64_t>> category_times;
    std::map<std::string, training::Profile::OpeningPerformance> openings;
    std::int64_t latest_rating_at = 0;
    double loss_total = 0.0;
    std::size_t move_count = 0;
    const auto profile_now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
    constexpr std::int64_t day_ms = 24LL * 60LL * 60LL * 1000LL;
    const std::int64_t today_start = profile_now / day_ms * day_ms;
    std::map<std::int64_t, training::Profile::TrendPoint> trend;
    for (int offset = 13; offset >= 0; --offset) {
        const std::int64_t day = today_start - static_cast<std::int64_t>(offset) * day_ms;
        trend.emplace(day, training::Profile::TrendPoint{day});
    }
    for (const auto& [game_id, game] : games_) {
        const bool player_is_white = game.imported.game.tag("White") == result.player_name;
        const bool player_is_black = game.imported.game.tag("Black") == result.player_name;
        if (player_is_white || player_is_black) {
            const std::string rating =
                game.imported.game.tag(player_is_white ? "WhiteElo" : "BlackElo");
            if (!rating.empty()) {
                try {
                    if (game.imported_at_ms >= latest_rating_at) {
                        result.latest_rating = std::stoi(rating);
                        latest_rating_at = game.imported_at_ms;
                    }
                    ++result.rating_observations;
                } catch (const std::exception&) {
                }
            }
        }
        const analysis::GameAnalysis* projected =
            game.analysis ? &*game.analysis
                          : game.shallow_analysis ? &*game.shallow_analysis : nullptr;
        if (!projected)
            continue;
        if (game.analysis) {
            ++result.games_analyzed;
            const std::int64_t analyzed_day = game.analyzed_at_ms / day_ms * day_ms;
            if (const auto point = trend.find(analyzed_day); point != trend.end())
                ++point->second.games_analyzed;
            if (game.analyzed_at_ms > 0 && profile_now - game.analyzed_at_ms <= 7 * day_ms)
                ++result.games_analyzed_7_days;
            if (game.analyzed_at_ms > 0 && profile_now - game.analyzed_at_ms <= 30 * day_ms)
                ++result.games_analyzed_30_days;
        } else {
            ++result.games_shallow_analyzed;
        }
        auto& opening = openings[projected->eco + "|" + projected->opening];
        opening.eco = projected->eco;
        opening.name = projected->opening;
        ++opening.games;
        result.total_positions += projected->moves.size();
        const bool reached_endgame = std::any_of(
            projected->moves.begin(), projected->moves.end(),
            [](const auto& move) { return move.phase == analysis::GamePhase::Endgame; });
        if (reached_endgame && (player_is_white || player_is_black)) {
            ++result.endgame_conversion.denominator;
            const std::string result_tag = game.imported.game.tag("Result");
            if ((player_is_white && result_tag == "1-0") ||
                (player_is_black && result_tag == "0-1"))
                ++result.endgame_conversion.numerator;
        }
        bool king_safety_violation = false;
        const bool has_clock_data = std::any_of(
            game.imported.game.plies.begin(), game.imported.game.plies.end(),
            [](const auto& ply) {
                return ply.clock_ms.has_value() || ply.elapsed_ms.has_value();
            });
        bool time_management_failure = false;
        for (const auto& move : projected->moves) {
            loss_total += move.loss;
            opening.average_centipawn_loss += move.loss;
            ++move_count;
        }
        for (const auto& mistake : projected->mistakes) {
            ++opening.mistakes;
            ++result.total_mistakes;
            if (game.analysis) {
                const std::int64_t analyzed_day = game.analyzed_at_ms / day_ms * day_ms;
                if (const auto point = trend.find(analyzed_day); point != trend.end())
                    ++point->second.mistakes;
            }
            auto& weakness = weaknesses[mistake.category];
            weakness.category = mistake.category;
            ++weakness.occurrences;
            if (mistake.category.find("king") != std::string::npos ||
                mistake.category.find("mate") != std::string::npos ||
                mistake.category.find("Back-rank") != std::string::npos)
                king_safety_violation = true;
            if (mistake.category == "Instant-move blunder" ||
                mistake.category == "Excessive early time use" ||
                mistake.category == "Time-management failure")
                time_management_failure = true;
            if (game.analyzed_at_ms > 0 && profile_now - game.analyzed_at_ms <= 7 * day_ms)
                ++weakness.occurrences_7_days;
            if (game.analyzed_at_ms > 0 && profile_now - game.analyzed_at_ms <= 30 * day_ms)
                ++weakness.occurrences_30_days;
            weakness.average_loss_cp += mistake.loss;
            ++weakness.phases[std::string(analysis::name(mistake.phase))];
            category_games[mistake.category].insert(game_id);
            if (game.analyzed_at_ms > 0)
                category_times[mistake.category].push_back(game.analyzed_at_ms);
        }
        if (game.analysis) {
            ++result.king_safety_violations.denominator;
            if (king_safety_violation)
                ++result.king_safety_violations.numerator;
            if (has_clock_data) {
                ++result.time_management_failures.denominator;
                if (time_management_failure)
                    ++result.time_management_failures.numerator;
            }
        }
    }
    for (const auto& [_, drill] : drills_) {
        auto& weakness = weaknesses[drill.category];
        weakness.category = drill.category;
        for (const auto& attempt : drill.attempts) {
            ++weakness.attempts;
            ++result.drill_attempts;
            const std::int64_t attempt_day = attempt.attempted_at_ms / day_ms * day_ms;
            if (const auto point = trend.find(attempt_day); point != trend.end()) {
                ++point->second.drill_attempts;
                if (attempt.correct)
                    ++point->second.drill_correct;
            }
            if (attempt.correct) {
                ++weakness.correct;
                ++result.drill_correct;
            }
        }
        if (!drill.attempts.empty()) {
            ++result.retention_reviews;
            const auto& latest = drill.attempts.back();
            if (latest.correct && latest.hint_level < 3)
                ++result.retained_reviews;
        }
    }
    for (auto& [category, weakness] : weaknesses) {
        weakness.games = category_games[category].size();
        weakness.drill_accuracy = weakness.attempts == 0
                                      ? 0.0
                                      : static_cast<double>(weakness.correct) /
                                            static_cast<double>(weakness.attempts);
        weakness.average_loss_cp = weakness.occurrences == 0
                                       ? 0.0
                                       : weakness.average_loss_cp /
                                             static_cast<double>(weakness.occurrences);
        weakness.recurrence_rate = result.games_analyzed == 0
                                       ? 0.0
                                       : static_cast<double>(weakness.games) /
                                             static_cast<double>(result.games_analyzed);
        auto& times = category_times[category];
        std::sort(times.begin(), times.end());
        if (times.size() >= 2) {
            std::int64_t total_gap = 0;
            for (std::size_t index = 1; index < times.size(); ++index)
                total_gap += times[index] - times[index - 1];
            weakness.repeated_interval_days =
                static_cast<double>(total_gap) /
                static_cast<double>(times.size() - 1) / static_cast<double>(day_ms);
        }
        result.weaknesses.push_back(weakness);
    }
    std::stable_sort(result.weaknesses.begin(), result.weaknesses.end(),
                     [](const auto& left, const auto& right) {
                         return left.occurrences > right.occurrences;
                     });
    result.analysis_completion_rate = result.games_imported == 0
                                          ? 0.0
                                          : static_cast<double>(result.games_analyzed) /
                                                static_cast<double>(result.games_imported);
    result.drill_accuracy = result.drill_attempts == 0
                                ? 0.0
                                : static_cast<double>(result.drill_correct) /
                                      static_cast<double>(result.drill_attempts);
    result.retention_rate = result.retention_reviews == 0
                                ? 0.0
                                : static_cast<double>(result.retained_reviews) /
                                      static_cast<double>(result.retention_reviews);
    result.average_centipawn_loss =
        move_count == 0 ? 0.0 : loss_total / static_cast<double>(move_count);
    for (const auto& [_, point] : trend)
        result.activity_trend.push_back(point);
    for (auto& [_, opening] : openings) {
        std::size_t positions = 0;
        for (const auto& [__, stored] : games_) {
            const analysis::GameAnalysis* projected =
                stored.analysis ? &*stored.analysis
                                : stored.shallow_analysis ? &*stored.shallow_analysis : nullptr;
            if (projected && projected->eco == opening.eco &&
                projected->opening == opening.name)
                positions += projected->moves.size();
        }
        opening.average_centipawn_loss = positions == 0
                                             ? 0.0
                                             : opening.average_centipawn_loss /
                                                   static_cast<double>(positions);
        result.openings.push_back(opening);
    }
    const auto finalize_rate = [](training::Profile::RateMetric& metric) {
        if (metric.denominator >= 5)
            metric.rate = static_cast<double>(metric.numerator) /
                          static_cast<double>(metric.denominator);
    };
    finalize_rate(result.endgame_conversion);
    finalize_rate(result.king_safety_violations);
    finalize_rate(result.time_management_failures);
    return result;
}

training::Profile Repository::profile() const {
    std::lock_guard lock(mutex_);
    return profile_unlocked();
}

std::vector<training::Recommendation> Repository::recommendations() {
    std::lock_guard lock(mutex_);
    auto recommendations =
        training::recommend(profile_unlocked(), training::default_catalog(), resource_completions_);
    bool changed = false;
    for (const auto& recommendation : recommendations) {
        if (!recommended_resources_.insert(recommendation.resource.id).second)
            continue;
        changed = true;
        static_cast<void>(log_.append(
            storage::EventType::ResourceRecommended,
            json::dump(json::Value::Object{
                {"resource_id", recommendation.resource.id},
                {"evidence", recommendation.evidence}, {"priority", recommendation.priority},
                {"catalog_version", std::string(training::catalog_version)},
            })));
    }
    if (changed)
        rebuild_indexes();
    return recommendations;
}

void Repository::complete_resource(std::string resource_id, std::int64_t completed_at_ms) {
    std::lock_guard lock(mutex_);
    const auto catalog = training::default_catalog();
    const bool known = std::any_of(catalog.begin(), catalog.end(), [&](const auto& resource) {
        return resource.id == resource_id;
    });
    if (!known)
        throw Error(ErrorCode::NotFound, "resource does not exist");
    static_cast<void>(log_.append(
        storage::EventType::ResourceCompleted,
        json::dump(json::Value::Object{
            {"resource_id", resource_id},
            {"completed_at_ms", static_cast<double>(completed_at_ms)},
            {"catalog_version", std::string(training::catalog_version)},
        })));
    resource_completions_.insert_or_assign(std::move(resource_id), completed_at_ms);
    rebuild_indexes();
}

std::filesystem::path Repository::create_snapshot() {
    std::lock_guard lock(mutex_);
    const auto replayed = log_.replay();
    if (!replayed.corruptions.empty() || replayed.truncated_tail)
        throw Error(ErrorCode::IoError, "cannot snapshot an invalid event log");
    const std::uint64_t last_event_id = replayed.events.empty() ? 0 : replayed.events.back().id;
    json::Value::Array games;
    for (const auto& [_, game] : games_)
        games.push_back(to_json(game, true));
    json::Value::Array drills;
    for (const auto& [_, drill] : drills_)
        drills.push_back(training::to_json(drill, 0));
    json::Value::Array completions;
    for (const auto& [id, completed_at] : resource_completions_)
        completions.emplace_back(json::Value::Object{
            {"resource_id", id}, {"completed_at_ms", static_cast<double>(completed_at)}});
    json::Value::Array jobs;
    for (const auto& [game_id, status] : analysis_job_states_)
        jobs.emplace_back(json::Value::Object{{"game_id", game_id}, {"status", status}});
    json::Value::Array batches;
    for (const auto& [_, batch] : batches_)
        batches.push_back(batch);
    json::Value::Array recommendations;
    for (const auto& id : recommended_resources_)
        recommendations.emplace_back(id);
    const std::filesystem::path directory = log_.path().parent_path() / "snapshots";
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / ("projection-" + std::to_string(last_event_id) + ".json");
    write_index(path, json::Value::Object{
                          {"snapshot_version", 1},
                          {"profile_version", std::string(training::profile_version)},
                          {"last_event_id", static_cast<double>(last_event_id)},
                          {"games", std::move(games)}, {"drills", std::move(drills)},
                          {"resource_completions", std::move(completions)},
                          {"analysis_jobs", std::move(jobs)},
                          {"batches", std::move(batches)},
                          {"recommended_resources", std::move(recommendations)},
                          {"background_paused", background_paused_},
                      });
    static_cast<void>(log_.append(storage::EventType::ProfileSnapshotCreated,
                                  json::dump(json::Value::Object{
                                      {"path", path.filename().string()},
                                      {"last_event_id", static_cast<double>(last_event_id)},
                                      {"snapshot_version", 1},
                                  })));
    rebuild_indexes();
    return path;
}

std::size_t Repository::compact_storage() {
    std::lock_guard lock(mutex_);
    const std::size_t events = log_.compact();
    rebuild_indexes();
    return events;
}

void Repository::record_job_state(std::string game_id, std::string status) {
    std::lock_guard lock(mutex_);
    static_cast<void>(log_.append(
        storage::EventType::AnalysisJobStateChanged,
        json::dump(json::Value::Object{{"game_id", game_id}, {"status", status}})));
    analysis_job_states_.insert_or_assign(std::move(game_id), std::move(status));
}

std::vector<std::string> Repository::recoverable_analysis_jobs() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [game_id, status] : analysis_job_states_) {
        const auto game = games_.find(game_id);
        if ((status == "queued" || status == "running") && game != games_.end() &&
            !game->second.analysis)
            result.push_back(game_id);
    }
    return result;
}

void Repository::set_background_paused(bool paused) {
    std::lock_guard lock(mutex_);
    static_cast<void>(log_.append(
        storage::EventType::BatchStateChanged,
        json::dump(json::Value::Object{{"paused", paused}, {"scope", "analysis_queue"}})));
    background_paused_ = paused;
}

bool Repository::background_paused() const {
    std::lock_guard lock(mutex_);
    return background_paused_;
}

json::Value Repository::create_batch(std::vector<std::string> game_ids, std::size_t discovered,
                                     std::size_t imported, std::size_t duplicates,
                                     std::size_t failed) {
    std::lock_guard lock(mutex_);
    const std::uint64_t sequence = next_batch_id_++;
    const std::string id = "batch-" + std::to_string(sequence);
    json::Value::Array ids;
    for (auto& game_id : game_ids)
        ids.emplace_back(std::move(game_id));
    json::Value value{json::Value::Object{
        {"id", id}, {"sequence", static_cast<double>(sequence)}, {"discovered", discovered},
        {"imported", imported}, {"duplicates", duplicates}, {"failed", failed},
        {"game_ids", std::move(ids)},
    }};
    static_cast<void>(log_.append(storage::EventType::BatchCreated, json::dump(value)));
    batches_.insert_or_assign(id, value);
    return value;
}

json::Value Repository::batches() const {
    std::lock_guard lock(mutex_);
    json::Value::Array result;
    for (const auto& [_, stored] : batches_) {
        json::Value::Object batch = stored.as_object();
        std::size_t queued = 0;
        std::size_t completed = 0;
        std::size_t job_failed = 0;
        std::size_t positions_analyzed = 0;
        std::size_t positions_remaining = 0;
        for (const auto& id : stored.at("game_ids").as_array()) {
            const auto found = analysis_job_states_.find(id.as_string());
            if (found == analysis_job_states_.end() || found->second == "queued" ||
                found->second == "running")
                ++queued;
            else if (found->second == "complete")
                ++completed;
            else if (found->second == "failed" || found->second == "cancelled")
                ++job_failed;
            const auto game = games_.find(id.as_string());
            if (game != games_.end()) {
                if (game->second.analysis)
                    positions_analyzed += game->second.analysis->moves.size();
                else if (game->second.shallow_analysis)
                    positions_analyzed += game->second.shallow_analysis->moves.size();
                else
                    positions_remaining += game->second.imported.game.plies.size();
            }
        }
        batch.insert_or_assign("queued", queued);
        batch.insert_or_assign("completed", completed);
        batch.insert_or_assign("job_failures", job_failed);
        batch.insert_or_assign("remaining", queued);
        batch.insert_or_assign("positions_analyzed", positions_analyzed);
        batch.insert_or_assign("positions_remaining", positions_remaining);
        batch.insert_or_assign("paused", background_paused_);
        result.emplace_back(std::move(batch));
    }
    return json::Value::Object{{"batches", std::move(result)},
                               {"paused", background_paused_}};
}

void Repository::rebuild_indexes() const {
    json::Value::Array games;
    json::Value::Array positions;
    json::Value::Array mistakes;
    json::Value::Array drills;
    json::Value::Array ratings;
    for (const auto& [id, stored] : games_) {
        games.emplace_back(json::Value::Object{
            {"game_id", id},
            {"white", stored.imported.game.tag("White")},
            {"black", stored.imported.game.tag("Black")},
            {"plies", stored.imported.game.plies.size()},
        });
        for (const std::string color : {"White", "Black"}) {
            const std::string rating = stored.imported.game.tag(color + "Elo");
            if (rating.empty())
                continue;
            try {
                ratings.emplace_back(json::Value::Object{
                    {"game_id", id},
                    {"player", stored.imported.game.tag(color)},
                    {"color", color},
                    {"rating", std::stoi(rating)},
                    {"observed_at_ms", static_cast<double>(stored.imported_at_ms)},
                });
            } catch (const std::exception&) {
                // Invalid source ratings are ignored just as they are in the profile projection.
            }
        }
        if (!stored.imported.game.plies.empty()) {
            const auto& first = stored.imported.game.plies.front();
            positions.emplace_back(json::Value::Object{
                {"hash", hash_string(chess::Board::from_fen(first.fen_before).hash())},
                {"game_id", id},
                {"ply", 0},
            });
        }
        for (std::size_t ply = 0; ply < stored.imported.game.plies.size(); ++ply) {
            positions.emplace_back(json::Value::Object{
                {"hash",
                 hash_string(
                     chess::Board::from_fen(stored.imported.game.plies[ply].fen_after).hash())},
                {"game_id", id},
                {"ply", ply + 1},
            });
        }
        if (stored.analysis) {
            for (const auto& mistake : stored.analysis->mistakes) {
                mistakes.emplace_back(json::Value::Object{
                    {"game_id", id},
                    {"ply", mistake.ply},
                    {"category", mistake.category},
                });
            }
        }
    }
    for (const auto& [id, drill] : drills_) {
        drills.emplace_back(json::Value::Object{{"drill_id", id},
                                                 {"game_id", drill.source_game_id},
                                                 {"ply", drill.source_ply},
                                                 {"category", drill.category}});
    }
    const std::filesystem::path directory = log_.path().parent_path();
    json::Value::Array resources;
    for (const auto& resource : training::default_catalog()) {
        const auto completed = resource_completions_.find(resource.id);
        resources.emplace_back(json::Value::Object{
            {"resource_id", resource.id},
            {"kind", resource.kind},
            {"opening", resource.opening},
            {"recommended", recommended_resources_.contains(resource.id)},
            {"completed_at_ms",
             completed == resource_completions_.end()
                 ? json::Value{}
                 : json::Value(static_cast<double>(completed->second))},
        });
    }
    json::Value::Array snapshots;
    const std::filesystem::path snapshot_directory = directory / "snapshots";
    if (std::filesystem::exists(snapshot_directory)) {
        std::vector<std::filesystem::path> paths;
        for (const auto& entry : std::filesystem::directory_iterator(snapshot_directory))
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                paths.push_back(entry.path());
        std::sort(paths.begin(), paths.end());
        for (const auto& path : paths) {
            json::Value::Object snapshot{{"path", path.filename().string()}, {"valid", false}};
            try {
                std::ifstream input(path);
                std::stringstream contents;
                contents << input.rdbuf();
                const auto value = json::parse(contents.str());
                snapshot.insert_or_assign("last_event_id", value.at("last_event_id"));
                snapshot.insert_or_assign("snapshot_version", value.at("snapshot_version"));
                snapshot.insert_or_assign("valid", true);
            } catch (const std::exception&) {
                // Snapshot files are accelerators; invalid ones are recorded but never authoritative.
            }
            snapshots.emplace_back(std::move(snapshot));
        }
    }
    write_index(directory / "games.idx",
                json::Value::Object{{"version", 1}, {"games", std::move(games)}});
    write_index(directory / "positions.idx",
                json::Value::Object{{"version", 1}, {"positions", std::move(positions)}});
    write_index(directory / "mistakes.idx",
                json::Value::Object{{"version", 1}, {"mistakes", std::move(mistakes)}});
    write_index(directory / "drills.idx",
                json::Value::Object{{"version", 1}, {"drills", std::move(drills)}});
    write_index(directory / "profile.idx",
                json::Value::Object{{"version", 1}, {"profile", training::to_json(profile_unlocked())}});
    write_index(directory / "resources.idx",
                json::Value::Object{{"version", 1},
                                    {"catalog_version", std::string(training::catalog_version)},
                                    {"resources", std::move(resources)}});
    write_index(directory / "ratings.idx",
                json::Value::Object{{"version", 1}, {"ratings", std::move(ratings)}});
    write_index(directory / "snapshots.idx",
                json::Value::Object{{"version", 1}, {"snapshots", std::move(snapshots)}});
}

json::Value to_json(const chess::Game& game) {
    json::Value::Array plies;
    for (std::size_t index = 0; index < game.plies.size(); ++index) {
        const auto& ply = game.plies[index];
        plies.emplace_back(json::Value::Object{
            {"ply", index},
            {"san", ply.san},
            {"uci", chess::uci(ply.move)},
            {"fen_before", ply.fen_before},
            {"fen_after", ply.fen_after},
            {"clock_ms", ply.clock_ms ? json::Value(static_cast<double>(*ply.clock_ms))
                                        : json::Value{}},
            {"elapsed_ms", ply.elapsed_ms ? json::Value(static_cast<double>(*ply.elapsed_ms))
                                            : json::Value{}},
        });
    }
    json::Value::Object tags;
    for (const auto& [key, value] : game.tags)
        tags.emplace(key, value);
    return json::Value::Object{
        {"id", game.identity},
        {"tags", std::move(tags)},
        {"plies", std::move(plies)},
    };
}

json::Value to_json(const analysis::GameAnalysis& analysis) {
    json::Value::Array moves;
    for (const auto& move : analysis.moves)
        moves.push_back(move_json(move));
    json::Value::Array mistakes;
    for (const auto& mistake : analysis.mistakes)
        mistakes.push_back(mistake_json(mistake));
    return json::Value::Object{
        {"game_id", analysis.game_id},
        {"moves", std::move(moves)},
        {"mistakes", std::move(mistakes)},
        {"eco", analysis.eco},
        {"opening", analysis.opening},
        {"book_ply", analysis.book_ply},
        {"departure_ply", analysis.departure_ply ? json::Value(*analysis.departure_ply)
                                                   : json::Value{}},
        {"opening_book_version", analysis.opening_book_version},
    };
}

json::Value to_json(const StoredGame& stored, bool include_pgn) {
    json::Value::Object result{
        {"game", to_json(stored.imported.game)},
        {"source_url", stored.imported.source_url},
        {"import_method", method_name(stored.imported.method)},
        {"analysis_status", stored.analysis ? "complete"
                                             : stored.shallow_analysis ? "shallow" : "pending"},
        {"analysis", stored.analysis
                         ? to_json(*stored.analysis)
                         : stored.shallow_analysis ? to_json(*stored.shallow_analysis)
                                                   : json::Value{}},
        {"shallow_analysis", stored.shallow_analysis
                                 ? to_json(*stored.shallow_analysis)
                                 : json::Value{}},
        {"imported_at_ms", static_cast<double>(stored.imported_at_ms)},
        {"analyzed_at_ms", static_cast<double>(stored.analyzed_at_ms)},
    };
    if (include_pgn)
        result.emplace("pgn", stored.imported.pgn);
    return result;
}

analysis::GameAnalysis analysis_from_json(const json::Value& value) {
    analysis::GameAnalysis result;
    result.game_id = value.at("game_id").as_string();
    for (const auto& move : value.at("moves").as_array())
        result.moves.push_back(move_from_json(move));
    for (const auto& mistake : value.at("mistakes").as_array()) {
        result.mistakes.push_back(mistake_from_json(mistake));
    }
    result.eco = value.get("eco", "A00").as_string();
    result.opening = value.get("opening", "Uncommon Opening").as_string();
    result.book_ply = value.get("book_ply", 0).as_size();
    const json::Value null_value;
    const auto& departure = value.get("departure_ply", null_value);
    if (!departure.is_null())
        result.departure_ply = departure.as_size();
    result.opening_book_version =
        value.get("opening_book_version", "legacy").as_string();
    return result;
}

} // namespace pct::app
