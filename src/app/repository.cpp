#include "pct/app/repository.hpp"

#include "pct/common/error.hpp"
#include "pct/common/log.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
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
    return mistake;
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
    for (const storage::Event& event : events.events) {
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
                games_.insert_or_assign(id, StoredGame{std::move(imported), std::nullopt});
            } else if (event.type == storage::EventType::AnalysisCompleted) {
                const std::string id = payload.at("game_id").as_string();
                const auto found = games_.find(id);
                if (found != games_.end())
                    found->second.analysis = analysis_from_json(payload.at("analysis"));
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
    games_.emplace(imported.game.identity, StoredGame{imported, std::nullopt});
    rebuild_indexes();
    return AddResult::Added;
}

void Repository::save_analysis(const analysis::GameAnalysis& analysis) {
    std::lock_guard lock(mutex_);
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
    found->second.analysis = analysis;
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

void Repository::rebuild_indexes() const {
    json::Value::Array games;
    json::Value::Array positions;
    json::Value::Array mistakes;
    for (const auto& [id, stored] : games_) {
        games.emplace_back(json::Value::Object{
            {"game_id", id},
            {"white", stored.imported.game.tag("White")},
            {"black", stored.imported.game.tag("Black")},
            {"plies", stored.imported.game.plies.size()},
        });
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
    const std::filesystem::path directory = log_.path().parent_path();
    write_index(directory / "games.idx",
                json::Value::Object{{"version", 1}, {"games", std::move(games)}});
    write_index(directory / "positions.idx",
                json::Value::Object{{"version", 1}, {"positions", std::move(positions)}});
    write_index(directory / "mistakes.idx",
                json::Value::Object{{"version", 1}, {"mistakes", std::move(mistakes)}});
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
    };
}

json::Value to_json(const StoredGame& stored, bool include_pgn) {
    json::Value::Object result{
        {"game", to_json(stored.imported.game)},
        {"source_url", stored.imported.source_url},
        {"import_method", method_name(stored.imported.method)},
        {"analysis_status", stored.analysis ? "complete" : "pending"},
        {"analysis", stored.analysis ? to_json(*stored.analysis) : json::Value{}},
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
    return result;
}

} // namespace pct::app
