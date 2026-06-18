#include "test.hpp"

#include "pct/service/http_server.hpp"

#include <filesystem>
#include <unistd.h>

using namespace pct;

namespace {

class ApiEngine final : public engine::AnalysisEngine {
  public:
    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   std::stop_token) override {
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

TEST_CASE("API exposes settings mistakes and analysis contracts") {
    ApiFixture fixture;
    const auto settings = fixture.api.handle(service::Request{"GET", "/api/settings", {}, {}});
    CHECK_EQ(settings.status, 200);
    CHECK_EQ(json::parse(settings.body).at("bind_address").as_string(), "127.0.0.1");
    const auto mistakes = fixture.api.handle(service::Request{"GET", "/api/mistakes", {}, {}});
    CHECK_EQ(mistakes.status, 200);
    CHECK(json::parse(mistakes.body).at("mistakes").as_array().empty());
}
