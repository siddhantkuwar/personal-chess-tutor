#include "test.hpp"

#include "pct/service/http_server.hpp"

#include <filesystem>
#include <thread>
#include <unistd.h>

using namespace pct;

namespace {
class Phase2Engine final : public engine::AnalysisEngine {
  public:
    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   CancellationToken) override {
        chess::Board board = chess::Board::from_fen(request.fen);
        const auto moves = board.legal_moves();
        const std::string best = moves.empty() ? "(none)" : chess::uci(moves.front());
        engine::AnalysisResult result;
        result.best_move = best;
        for (int pv = 1; pv <= request.multipv; ++pv)
            result.lines.push_back({pv, request.depth, 160 - pv, std::nullopt, 100, 1, {best}});
        return result;
    }
};

std::string encode_id(std::string value) {
    std::string result;
    for (const char character : value)
        result += character == ':' ? "%3A" : std::string(1, character);
    return result;
}
} // namespace

TEST_CASE("phase two API completes the personal improvement loop end to end") {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("pct-phase2-e2e-" + std::to_string(::getpid()));
    std::filesystem::remove_all(directory);
    storage::EventLog log(directory / "events.log");
    app::Repository repository(log);
    import::ImportService importer;
    Phase2Engine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{2, 3, 80, 3, 2});
    app::JobManager jobs(repository, analyzer);
    service::Api api(importer, repository, jobs);
    const std::string pgn =
        "[White \"Learner\"]\n[Black \"Coach\"]\n[Result \"1-0\"]\n\n1. e4 e5 2. Nf3 Nc6 1-0";
    const auto imported = api.handle(service::Request{
        "POST", "/api/import", {}, json::dump(json::Value::Object{{"pgn", pgn}})});
    CHECK_EQ(imported.status, 202);
    const auto import_body = json::parse(imported.body);
    CHECK(!import_body.as_object().contains("job"));
    const std::string game_id = import_body.at("game_id").as_string();
    const auto started = api.handle(
        service::Request{"POST", "/api/games/" + game_id + "/analysis", {}, {}});
    CHECK_EQ(started.status, 202);
    const auto job_id = static_cast<std::uint64_t>(json::parse(started.body).at("id").as_number());
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto job = jobs.get(job_id);
        if (job && (job->status == app::JobStatus::Complete ||
                    job->status == app::JobStatus::Failed))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(jobs.get(job_id)->status == app::JobStatus::Complete);
    const auto drills_response = api.handle(service::Request{"GET", "/api/drills", {}, {}});
    CHECK_EQ(drills_response.status, 200);
    const auto drills_document = json::parse(drills_response.body);
    const auto& drills = drills_document.at("drills").as_array();
    CHECK(!drills.empty());
    const std::string drill_id = drills.front().at("id").as_string();
    const std::string solution = drills.front().at("solutions").as_array().front().as_string();
    CHECK(!drills.front().at("changed_threat").as_string().empty());
    CHECK(drills.front().at("attacked_pieces").is_array());
    const std::string drill_path = "/api/drills/" + encode_id(drill_id);
    CHECK_EQ(api.handle(service::Request{"POST", drill_path + "/session", {}, "{}"}).status,
             200);
    CHECK_EQ(api.handle(service::Request{"POST", drill_path + "/hint", {}, "{}"}).status,
             400);
    chess::Board board = chess::Board::from_fen(drills.front().at("fen").as_string());
    std::string wrong_move;
    for (const auto& move : board.legal_moves()) {
        const std::string candidate = chess::uci(move);
        if (candidate != solution) {
            wrong_move = candidate;
            break;
        }
    }
    CHECK(!wrong_move.empty());
    const auto failed = api.handle(service::Request{
        "POST", drill_path + "/attempt", {},
        json::dump(json::Value::Object{{"move", wrong_move}, {"response_time_ms", 1000}})});
    CHECK_EQ(failed.status, 200);
    CHECK(!json::parse(failed.body).at("attempt").at("correct").as_bool());
    CHECK_EQ(json::parse(failed.body).at("drill").at("available_hint_level").as_int(), 1);
    const auto hint = api.handle(service::Request{"POST", drill_path + "/hint", {}, "{}"});
    CHECK_EQ(hint.status, 200);
    CHECK_EQ(json::parse(hint.body).at("hint_level").as_int(), 1);
    const auto attempt = api.handle(service::Request{
        "POST", drill_path + "/attempt", {},
        json::dump(json::Value::Object{{"move", solution},
                                       {"response_time_ms", 2500},
                                       {"hint_level", 0}})});
    CHECK_EQ(attempt.status, 200);
    CHECK(json::parse(attempt.body).at("attempt").at("correct").as_bool());
    CHECK_EQ(json::parse(attempt.body).at("drill").at("schedule").at("interval_days").as_int(),
             1);
    const auto profile = json::parse(api.handle(service::Request{"GET", "/api/profile", {}, {}}).body);
    CHECK_EQ(profile.at("games_analyzed").as_size(), 1ULL);
    CHECK_EQ(profile.at("drill_attempts").as_size(), 2ULL);
    CHECK_EQ(profile.at("drill_correct").as_size(), 1ULL);
    CHECK_EQ(profile.at("retention_reviews").as_size(), 1ULL);
    CHECK_EQ(profile.at("retained_reviews").as_size(), 1ULL);
    const auto resources = json::parse(
        api.handle(service::Request{"GET", "/api/resources", {}, {}}).body);
    CHECK(!resources.at("resources").as_array().empty());
    CHECK(!resources.at("resources").as_array().front().at("evidence").as_string().empty());
    std::filesystem::remove_all(directory);
}
