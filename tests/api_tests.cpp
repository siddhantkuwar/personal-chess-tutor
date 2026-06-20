#include "test.hpp"

#include "pct/service/http_server.hpp"

#include <filesystem>
#include <unistd.h>

using namespace pct;

namespace {

class ApiEngine final : public engine::AnalysisEngine {
  public:
    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   CancellationToken) override {
        chess::Board board = chess::Board::from_fen(request.fen);
        const auto moves = board.legal_moves();
        const std::string best = moves.empty() ? "(none)" : chess::uci(moves.front());
        return {{{1, request.depth, 0, std::nullopt, 1, 1, {best}}}, best, {}};
    }
};

struct ApiFixture {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("pct-api-" + std::to_string(::getpid()) + ".log");
    storage::EventLog log;
    app::Repository repository;
    import::ImportService importer;
    ApiEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer;
    app::JobManager jobs;
    service::Api api;

    ApiFixture()
        : log((std::filesystem::remove(path), path)), repository(log), analyzer(engine, cache),
          jobs(repository, analyzer), api(importer, repository, jobs) {}
    ~ApiFixture() = default;
};

} // namespace

TEST_CASE("API imports PGN and returns navigable game data immediately") {
    ApiFixture fixture;
    const std::string body = json::dump(json::Value::Object{
        {"pgn", "[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1. e4 e5 1-0"},
    });
    const auto imported = fixture.api.handle(service::Request{"POST", "/api/import", {}, body});
    CHECK_EQ(imported.status, 202);
    const std::string id = json::parse(imported.body).at("game_id").as_string();
    const auto game = fixture.api.handle(service::Request{"GET", "/api/games/" + id, {}, {}});
    CHECK_EQ(game.status, 200);
    CHECK_EQ(json::parse(game.body).at("game").at("plies").as_array().size(), 2ULL);
    const auto move =
        fixture.api.handle(service::Request{"GET", "/api/games/" + id + "/moves/0", {}, {}});
    CHECK_EQ(move.status, 200);
    CHECK_EQ(json::parse(move.body).at("san").as_string(), "e4");
}

TEST_CASE("API validates request shape and unknown resources") {
    ApiFixture fixture;
    CHECK_EQ(fixture.api.handle(service::Request{"POST", "/api/import", {}, "{}"}).status, 400);
    CHECK_EQ(fixture.api.handle(service::Request{"GET", "/api/games/missing", {}, {}}).status, 404);
    CHECK_EQ(fixture.api.handle(service::Request{"GET", "/api/unknown", {}, {}}).status, 404);
}

TEST_CASE("API path router decodes identifiers safely") {
    ApiFixture fixture;
    const auto response = fixture.api.handle(
        service::Request{"GET", "/api/drills/game%3A0", {}, {}});
    CHECK_EQ(response.status, 404);
    CHECK_EQ(fixture.api.handle(service::Request{"GET", "/api/drills/bad%XX", {}, {}}).status,
             400);
}

TEST_CASE("API exposes settings mistakes and analysis contracts") {
    ApiFixture fixture;
    const auto settings = fixture.api.handle(service::Request{"GET", "/api/settings", {}, {}});
    CHECK_EQ(settings.status, 200);
    CHECK_EQ(json::parse(settings.body).at("bind_address").as_string(), "127.0.0.1");
    const auto mistakes = fixture.api.handle(service::Request{"GET", "/api/mistakes", {}, {}});
    CHECK_EQ(mistakes.status, 200);
    CHECK(json::parse(mistakes.body).at("mistakes").as_array().empty());
}

TEST_CASE("API exposes phase two profile drills resources and batch contracts") {
    ApiFixture fixture;
    CHECK_EQ(fixture.api.handle(service::Request{"GET", "/api/drills", {}, {}}).status, 200);
    const auto profile = fixture.api.handle(service::Request{"GET", "/api/profile", {}, {}});
    CHECK_EQ(profile.status, 200);
    CHECK_EQ(json::parse(profile.body).at("projection_version").as_string(), "profile-1");
    CHECK_EQ(fixture.api.handle(service::Request{"GET", "/api/resources", {}, {}}).status, 200);
    const std::string pgn = "[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1. e4 e5 1-0";
    const std::string body = json::dump(json::Value::Object{
        {"pgns", json::Value::Array{pgn, pgn, "invalid"}},
    });
    const auto batch = fixture.api.handle(service::Request{"POST", "/api/import/batch", {}, body});
    CHECK_EQ(batch.status, 202);
    const auto result = json::parse(batch.body);
    CHECK_EQ(result.at("discovered").as_size(), 3ULL);
    CHECK_EQ(result.at("imported").as_size(), 1ULL);
    CHECK_EQ(result.at("duplicates").as_size(), 1ULL);
    CHECK_EQ(result.at("failed").as_size(), 1ULL);
    CHECK_EQ(result.at("batch_id").as_string(), "batch-1");
    const auto batches = fixture.api.handle(service::Request{"GET", "/api/batches", {}, {}});
    CHECK_EQ(batches.status, 200);
    CHECK_EQ(json::parse(batches.body).at("batches").as_array().size(), 1ULL);
    CHECK(json::parse(batches.body).as_object().contains("cache_hits"));
}

TEST_CASE("batch import applies bounded backpressure") {
    ApiFixture fixture;
    json::Value::Array pgns;
    for (int index = 0; index < 101; ++index)
        pgns.emplace_back("invalid");
    const auto response = fixture.api.handle(service::Request{
        "POST", "/api/import/batch", {},
        json::dump(json::Value::Object{{"pgns", std::move(pgns)}})});
    CHECK_EQ(response.status, 400);
}

TEST_CASE("storage maintenance API creates snapshots and performs verified compaction") {
    ApiFixture fixture;
    const auto snapshot =
        fixture.api.handle(service::Request{"POST", "/api/storage/snapshot", {}, "{}"});
    CHECK_EQ(snapshot.status, 200);
    CHECK(json::parse(snapshot.body).at("created").as_bool());
    const auto compact =
        fixture.api.handle(service::Request{"POST", "/api/storage/compact", {}, "{}"});
    CHECK_EQ(compact.status, 200);
    CHECK(json::parse(compact.body).at("compacted").as_bool());
}

TEST_CASE("drill session and hint endpoints reject unknown drills") {
    ApiFixture fixture;
    CHECK_EQ(fixture.api.handle(
                 service::Request{"POST", "/api/drills/missing/session", {}, "{}"})
                 .status,
             404);
    CHECK_EQ(fixture.api.handle(
                 service::Request{"POST", "/api/drills/missing/hint", {}, "{}"})
                 .status,
             404);
}
