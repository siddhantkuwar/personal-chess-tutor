#include "test.hpp"

#include "pct/app/ingest_manager.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unistd.h>

using namespace pct;

namespace {

class IngestEngine final : public engine::AnalysisEngine {
  public:
    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   CancellationToken) override {
        chess::Board board = chess::Board::from_fen(request.fen);
        const auto moves = board.legal_moves();
        const std::string best = moves.empty() ? "(none)" : chess::uci(moves.front());
        return {{{1, request.depth, 0, std::nullopt, 1, 1, {best}}}, best, {}};
    }
};

std::string pgn_at(std::string_view id, std::string_view date, std::string_view time,
                   std::string_view white = "Alice") {
    return "[Event \"Archive\"]\n[Site \"https://www.chess.com/game/live/" +
           std::string(id) + "\"]\n[UTCDate \"" + std::string(date) +
           "\"]\n[UTCTime \"" + std::string(time) + "\"]\n"
           "[White \"" + std::string(white) + "\"]\n[Black \"Bob\"]\n[Result \"1-0\"]\n\n"
           "1. e4 e5 1-0";
}

std::string pgn(std::string_view id, std::string_view white = "Alice") {
    return pgn_at(id, "2026.07.15", "12:00:00", white);
}

struct IngestFixture {
    std::filesystem::path directory;
    storage::EventLog log;
    app::Repository repository;
    import::ImportService importer;
    IngestEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer;
    app::JobManager jobs;

    explicit IngestFixture(import::HttpTransport transport = {})
        : directory(std::filesystem::temp_directory_path() /
                    ("pct-ingest-manager-" + std::to_string(::getpid()) + "-" +
                     std::to_string(counter++))),
          log((std::filesystem::remove_all(directory), directory / "events.log")),
          repository(log), importer(transport), analyzer(engine, cache,
          analysis::AnalyzerOptions{2, 3, 80, 2, 1}), jobs(repository, analyzer) {}
    static inline std::atomic<unsigned> counter{0};
};

template <typename Getter>
auto wait_terminal(Getter getter) {
    auto value = getter();
    for (int attempt = 0; attempt < 400; ++attempt) {
        value = getter();
        if (value && value->status != "queued" && value->status != "running")
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return value;
}

import::HttpResponse response(std::string url, std::string body) {
    return {200, {}, std::move(url), std::move(body)};
}

std::string archives_json(std::size_t count) {
    json::Value::Array archives;
    for (std::size_t index = 0; index < count; ++index) {
        const int month = 7 - static_cast<int>(index);
        archives.emplace_back("https://api.chess.com/pub/player/alice/games/2026/0" +
                              std::to_string(month));
    }
    return json::dump(json::Value::Object{{"archives", std::move(archives)}});
}

} // namespace

TEST_CASE("ingest resolution uses exact local archive hit without starting analysis") {
    IngestFixture fixture;
    const std::string id = "171626462440";
    static_cast<void>(fixture.repository.index_chesscom_archive_chunk({
        {id, "https://www.chess.com/game/live/" + id, pgn(id), "alice", "2026-07",
         "rapid", 1, 2, "https://api.chess.com/pub/player/alice/games/2026/07"}}));
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs,
                               [](const import::HttpRequest&) -> import::HttpResponse {
                                   throw std::runtime_error("network must not be used");
                               });
    const auto started = manager.resolve(
        "https://chess.com/analysis/game/live/171626462440/analysis?ignored=x");
    const auto done = wait_terminal([&] { return manager.resolution(started.id); });
    CHECK(done.has_value());
    CHECK_EQ(done->status, "resolved");
    CHECK_EQ(done->source, "local_archive");
    CHECK(!done->imported_game_id.empty());
    CHECK(fixture.jobs.list().empty());
}

TEST_CASE("ingest resolution accepts current Chess.com generic Site PGN") {
    IngestFixture fixture;
    const std::string id = "171626817794";
    std::string current_pgn = pgn(id);
    const std::string exact_site =
        "[Site \"https://www.chess.com/game/live/" + id + "\"]";
    current_pgn.replace(current_pgn.find(exact_site), exact_site.size(),
                        "[Site \"Chess.com\"]");
    static_cast<void>(fixture.repository.index_chesscom_archive_chunk({
        {id, "https://www.chess.com/game/live/" + id, current_pgn, "hikaru", "2026-07",
         "rapid", 1, 2, "https://api.chess.com/pub/player/hikaru/games/2026/07"}}));
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs,
                               [](const import::HttpRequest&) -> import::HttpResponse {
                                   throw std::runtime_error("network must not be used");
                               });

    const auto started = manager.resolve("https://www.chess.com/game/live/" + id);
    const auto done = wait_terminal([&] { return manager.resolution(started.id); });
    CHECK(done.has_value());
    CHECK_EQ(done->status, "resolved");
    CHECK_EQ(done->source, "local_archive");
}

TEST_CASE("ingest resolution scans connected archive newest first and returns recovery without profile") {
    std::vector<std::string> calls;
    std::mutex calls_mutex;
    auto transport = [&](const import::HttpRequest& request) {
        std::lock_guard lock(calls_mutex);
        calls.push_back(request.url);
        if (request.url.ends_with("/archives"))
            return response(request.url, archives_json(3));
        if (request.url.ends_with("/2026/07"))
            return response(request.url, json::dump(json::Value::Object{{"games", json::Value::Array{
                json::Value::Object{{"url", "https://www.chess.com/game/live/222"},
                                    {"pgn", pgn("222")}, {"time_class", "rapid"}}}}}));
        return response(request.url, "<html>changed page</html>");
    };
    IngestFixture fixture(transport);
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs, transport);
    manager.configure_profile("Alice");
    auto hit = manager.resolve("https://www.chess.com/game/live/222");
    auto hit_done = wait_terminal([&] { return manager.resolution(hit.id); });
    CHECK_EQ(hit_done->status, "resolved");
    CHECK_EQ(hit_done->source, "profile_archive");
    CHECK(calls.size() >= 3);
    CHECK_EQ(calls[0], "https://www.chess.com/game/live/222");
    CHECK(calls[2].ends_with("/2026/07"));

    IngestFixture no_profile(transport);
    app::IngestManager no_profile_manager(no_profile.importer, no_profile.repository,
                                           no_profile.jobs, transport);
    auto missing = no_profile_manager.resolve("https://www.chess.com/game/live/999");
    auto missing_done = wait_terminal([&] { return no_profile_manager.resolution(missing.id); });
    CHECK_EQ(missing_done->status, "needs_recovery");
    CHECK_EQ(missing_done->code, "profile_required");
    CHECK(!missing_done->actions.empty());
    CHECK(missing_done->error.find("PGN") != std::string::npos);
}

TEST_CASE("resolution discovers both players from public page when configured profile is unrelated") {
    const std::string target = "171626462440";
    auto transport = [&](const import::HttpRequest& request) {
        if (request.url == "https://www.chess.com/game/live/" + target)
            return response(request.url,
                            "<html><head><meta property=\"og:title\" content=\"Chess: "
                            "superking116 vs CartaaaaZ\"></head></html>");
        if (request.url.ends_with("/player/hikaru/games/archives"))
            return response(request.url, R"json({"archives":[]})json");
        if (request.url.ends_with("/player/superking116/games/archives"))
            return response(request.url,
                            R"json({"archives":["https://api.chess.com/pub/player/superking116/games/2026/07"]})json");
        if (request.url.ends_with("/player/superking116/games/2026/07"))
            return response(request.url, json::dump(json::Value::Object{{"games", json::Value::Array{
                json::Value::Object{{"url", "https://www.chess.com/game/live/" + target},
                                    {"pgn", pgn(target, "superking116")},
                                    {"time_class", "rapid"}}}}}));
        throw std::runtime_error("unexpected URL: " + request.url);
    };
    IngestFixture fixture(transport);
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs, transport);
    manager.configure_profile("hikaru");

    const auto started = manager.resolve("https://www.chess.com/game/live/" + target);
    const auto done = wait_terminal([&] { return manager.resolution(started.id); });
    CHECK(done.has_value());
    CHECK_EQ(done->status, "resolved");
    CHECK_EQ(done->source, "profile_archive");
    CHECK_EQ(done->username, "superking116");
}

TEST_CASE("sync day windows are bounded newest first serial checkpointed and deduplicated") {
    CHECK_EQ(app::IngestManager::max_months_for_days(7), 2ULL);
    CHECK_EQ(app::IngestManager::max_months_for_days(30), 2ULL);
    CHECK_EQ(app::IngestManager::max_months_for_days(90), 4ULL);
    CHECK_THROWS(app::IngestManager::max_months_for_days(31));

    std::vector<std::string> months;
    std::atomic<int> active{0};
    std::atomic<int> maximum{0};
    auto transport = [&](const import::HttpRequest& request) {
        const int current = ++active;
        maximum.store(std::max(maximum.load(), current));
        import::HttpResponse result;
        if (request.url.ends_with("/archives")) {
            result = response(request.url, archives_json(5));
        } else {
            months.push_back(request.url.substr(request.url.size() - 7));
            const std::string id = std::to_string(300 + months.size());
            result = response(request.url, json::dump(json::Value::Object{{"games", json::Value::Array{
                json::Value::Object{{"url", "https://www.chess.com/game/live/" + id},
                                    {"pgn", pgn(id)}, {"time_class", "rapid"}}}}}));
        }
        --active;
        return result;
    };
    IngestFixture fixture(transport);
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs, transport);
    manager.configure_profile("alice");
    const auto started = manager.start_sync(90);
    const auto done = wait_terminal([&] { return manager.sync(started.id); });
    CHECK_EQ(done->status, "succeeded");
    CHECK_EQ(done->months_completed, 4ULL);
    CHECK_EQ(months.size(), 4ULL);
    CHECK_EQ(months.front(), "2026/07");
    CHECK_EQ(months.back(), "2026/04");
    CHECK_EQ(maximum.load(), 1);
    CHECK_EQ(fixture.repository.chesscom_month_checkpoints("alice").size(), 4ULL);
    CHECK(fixture.jobs.list().empty());
}

TEST_CASE("sync cancellation is cooperative and interactive work is prioritized between months") {
    std::atomic<bool> hold{true};
    std::vector<std::string> events;
    auto transport = [&](const import::HttpRequest& request) {
        if (request.url.ends_with("/archives"))
            return response(request.url, archives_json(3));
        while (hold.load() && !request.cancellation.stop_requested())
            std::this_thread::yield();
        if (request.cancellation.stop_requested())
            throw import::ChessComClientError(import::ChessComFailure::Cancelled,
                                               ErrorCode::NetworkError, "cancelled");
        return response(request.url, R"({"games":[]})");
    };
    IngestFixture fixture(transport);
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs, transport);
    manager.configure_profile("alice");
    const auto started = manager.start_sync(30);
    for (int attempt = 0; attempt < 200 && manager.sync(started.id)->status == "queued"; ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    CHECK(manager.cancel_sync("current"));
    hold = false;
    const auto done = wait_terminal([&] { return manager.sync(started.id); });
    CHECK_EQ(done->status, "cancelled");
    CHECK_EQ(fixture.repository.chesscom_sync_state().status, "paused");
}

TEST_CASE("interactive resolution completes while a historical month request remains blocked") {
    std::atomic<bool> first_month_entered{false};
    std::atomic<bool> release_first_month{false};
    std::atomic<int> month_calls{0};
    auto transport = [&](const import::HttpRequest& request) {
        if (request.url.ends_with("/archives"))
            return response(request.url, archives_json(2));
        const int call = ++month_calls;
        if (call == 1) {
            first_month_entered = true;
            while (!release_first_month.load())
                std::this_thread::yield();
        }
        return response(request.url, R"({"games":[]})");
    };
    IngestFixture fixture(transport);
    const std::string game_id = "777";
    static_cast<void>(fixture.repository.index_chesscom_archive_chunk({
        {game_id, "https://www.chess.com/game/live/" + game_id, pgn(game_id), "alice",
         "2026-07", "rapid", 1, 2, "https://api.chess.com/archive"}}));
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs, transport);
    manager.configure_profile("alice");
    const auto sync = manager.start_sync(30);
    for (int attempt = 0; attempt < 400 && !first_month_entered.load(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    CHECK(first_month_entered.load());
    const auto resolution = manager.resolve("https://www.chess.com/game/live/777");
    CHECK_EQ(wait_terminal([&] { return manager.resolution(resolution.id); })->status,
             "resolved");
    CHECK(!release_first_month.load());
    CHECK_EQ(month_calls.load(), 1);
    release_first_month = true;
    CHECK_EQ(wait_terminal([&] { return manager.sync(sync.id); })->status, "succeeded");
    CHECK_EQ(month_calls.load(), 2);
}

TEST_CASE("sync applies exact UTC cutoff boundaries across month rollover and rejects invalid dates") {
    constexpr std::int64_t now = 1784116800000LL; // 2026-07-15 12:00:00 UTC
    auto transport = [&](const import::HttpRequest& request) {
        if (request.url.ends_with("/archives"))
            return response(request.url, archives_json(2));
        if (request.url.ends_with("/2026/07")) {
            json::Value::Array games;
            const auto add = [&](std::string id, std::string date, std::string time) {
                games.emplace_back(json::Value::Object{
                    {"url", "https://www.chess.com/game/live/" + id},
                    {"pgn", pgn_at(id, date, time, id)}, {"time_class", "rapid"}});
            };
            add("801", "2026.07.08", "11:59:59");
            add("802", "2026.07.08", "12:00:00");
            add("803", "2026.07.15", "12:00:00");
            add("804", "2026.07.15", "12:00:01");
            add("805", "2026.02.30", "12:00:00");
            return response(request.url,
                            json::dump(json::Value::Object{{"games", std::move(games)}}));
        }
        return response(request.url, R"({"games":[]})");
    };
    IngestFixture fixture(transport);
    app::IngestOptions options;
    options.clock = [] { return now; };
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs,
                               transport, {}, options);
    manager.configure_profile("alice");
    const auto started = manager.start_sync(7);
    const auto done = wait_terminal([&] { return manager.sync(started.id); });
    CHECK_EQ(done->cutoff_ms, now - 7LL * 24LL * 60LL * 60LL * 1000LL);
    CHECK_EQ(done->status, "succeeded");
    CHECK(fixture.repository.chesscom_archive_entry("802").has_value());
    CHECK(fixture.repository.chesscom_archive_entry("803").has_value());
    CHECK(!fixture.repository.chesscom_archive_entry("801").has_value());
    CHECK(!fixture.repository.chesscom_archive_entry("804").has_value());
    CHECK(!fixture.repository.chesscom_archive_entry("805").has_value());
    CHECK(fixture.repository.chesscom_sync_state().cursor.find("cutoff_ms=") !=
          std::string::npos);
}

TEST_CASE("seven day cutoff includes the exact prior-month boundary") {
    constexpr std::int64_t now = 1772323200000LL; // 2026-03-01 00:00:00 UTC
    std::atomic<int> month_calls{0};
    auto transport = [&](const import::HttpRequest& request) {
        if (request.url.ends_with("/archives"))
            return response(request.url,
                R"({"archives":["https://api.chess.com/pub/player/alice/games/2026/02","https://api.chess.com/pub/player/alice/games/2026/03"]})");
        ++month_calls;
        if (request.url.ends_with("/2026/02")) {
            return response(request.url, json::dump(json::Value::Object{{"games", json::Value::Array{
                json::Value::Object{{"url", "https://www.chess.com/game/live/811"},
                                    {"pgn", pgn_at("811", "2026.02.21", "23:59:59")}},
                json::Value::Object{{"url", "https://www.chess.com/game/live/812"},
                                    {"pgn", pgn_at("812", "2026.02.22", "00:00:00")}}
            }}}));
        }
        return response(request.url, R"({"games":[]})");
    };
    IngestFixture fixture(transport);
    app::IngestOptions options;
    options.clock = [] { return now; };
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs,
                               transport, {}, options);
    manager.configure_profile("alice");
    const auto started = manager.start_sync(7);
    CHECK_EQ(wait_terminal([&] { return manager.sync(started.id); })->status, "succeeded");
    CHECK_EQ(month_calls.load(), 2);
    CHECK(!fixture.repository.chesscom_archive_entry("811").has_value());
    CHECK(fixture.repository.chesscom_archive_entry("812").has_value());
}

TEST_CASE("archive persistence uses at most one hundred entries per durable chunk before checkpoint") {
    constexpr std::int64_t now = 1784116800000LL;
    auto transport = [&](const import::HttpRequest& request) {
        if (request.url.ends_with("/archives"))
            return response(request.url, archives_json(1));
        json::Value::Array games;
        for (int index = 0; index < 205; ++index) {
            const std::string id = std::to_string(10000 + index);
            games.emplace_back(json::Value::Object{{"url", "https://www.chess.com/game/live/" + id},
                {"pgn", pgn_at(id, "2026.07.15", "11:00:00", id)}, {"time_class", "rapid"}});
        }
        return response(request.url,
                        json::dump(json::Value::Object{{"games", std::move(games)}}));
    };
    IngestFixture fixture(transport);
    app::IngestOptions options;
    options.clock = [] { return now; };
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs,
                               transport, {}, options);
    manager.configure_profile("alice");
    const auto began = std::chrono::steady_clock::now();
    const auto started = manager.start_sync(7);
    std::optional<app::IngestSync> completed;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        completed = manager.sync(started.id);
        if (completed && completed->status != "queued" && completed->status != "running")
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const auto elapsed = std::chrono::steady_clock::now() - began;
    CHECK(completed.has_value());
    CHECK_EQ(completed->status, "succeeded");
    CHECK(elapsed < std::chrono::seconds(5));
    CHECK_EQ(fixture.repository.size(), 205ULL);
    CHECK(fixture.jobs.list().empty());
    app::ChessComArchiveSearch search;
    search.limit = app::chesscom_archive_search_limit;
    CHECK_EQ(fixture.repository.search_chesscom_archive(search).entries.size(),
             app::chesscom_archive_search_limit);
    std::size_t chunks = 0;
    std::size_t checkpoint_index = 0;
    const auto events = fixture.log.replay().events;
    for (std::size_t index = 0; index < events.size(); ++index) {
        if (events[index].type == storage::EventType::ChessComArchiveChunkIndexed) ++chunks;
        if (events[index].type == storage::EventType::ChessComMonthCheckpointed)
            checkpoint_index = index;
    }
    CHECK_EQ(chunks, 3ULL);
    CHECK(events[checkpoint_index - 1].type == storage::EventType::ChessComArchiveChunkIndexed);
}

TEST_CASE("blocked resolution cancellation propagates to transport and clears active dedup") {
    std::atomic<bool> entered{false};
    auto transport = [&](const import::HttpRequest& request) -> import::HttpResponse {
        entered = true;
        while (!request.cancellation.stop_requested())
            std::this_thread::yield();
        throw import::ChessComClientError(import::ChessComFailure::Cancelled,
                                           ErrorCode::NetworkError, "cancelled");
    };
    IngestFixture fixture(transport);
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs, transport);
    const auto first = manager.resolve("https://www.chess.com/game/live/900");
    const auto duplicate = manager.resolve("https://chess.com/game/live/900");
    CHECK_EQ(first.id, duplicate.id);
    for (int attempt = 0; attempt < 400 && !entered.load(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    CHECK(entered.load());
    CHECK(manager.cancel_resolution(first.id));
    CHECK_EQ(wait_terminal([&] { return manager.resolution(first.id); })->status, "cancelled");
}

TEST_CASE("profile resolution chunks more than one hundred archive games") {
    auto transport = [&](const import::HttpRequest& request) {
        if (request.url.ends_with("/archives"))
            return response(request.url, archives_json(1));
        json::Value::Array games;
        for (int index = 0; index < 205; ++index) {
            const std::string id = std::to_string(12000 + index);
            games.emplace_back(json::Value::Object{
                {"url", "https://www.chess.com/game/live/" + id},
                {"pgn", pgn(id, id)}, {"time_class", "rapid"}});
        }
        return response(request.url,
                        json::dump(json::Value::Object{{"games", std::move(games)}}));
    };
    IngestFixture fixture(transport);
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs, transport);
    const auto started = manager.resolve("https://www.chess.com/game/live/12204", "alice");
    CHECK_EQ(wait_terminal([&] { return manager.resolution(started.id); })->status, "resolved");
    std::size_t chunks = 0;
    for (const auto& event : fixture.log.replay().events)
        if (event.type == storage::EventType::ChessComArchiveChunkIndexed) ++chunks;
    CHECK_EQ(chunks, 3ULL);
}

TEST_CASE("interactive queue rejection is atomic and leaves no phantom resolution") {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    auto transport = [&](const import::HttpRequest& request) {
        entered = true;
        while (!release.load() && !request.cancellation.stop_requested())
            std::this_thread::yield();
        return response(request.url, "<html>no pgn</html>");
    };
    IngestFixture fixture(transport);
    app::IngestOptions options;
    options.max_pending = 1;
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs,
                               transport, {}, options);
    const auto running = manager.resolve("https://www.chess.com/game/live/13001");
    for (int attempt = 0; attempt < 400 && !entered.load(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const auto queued = manager.resolve("https://www.chess.com/game/live/13002");
    CHECK(!queued.id.empty());
    CHECK_THROWS(manager.resolve("https://www.chess.com/game/live/13003"));
    CHECK_EQ(manager.snapshot().at("resolutions").as_array().size(), 2ULL);
    release = true;
    CHECK(wait_terminal([&] { return manager.resolution(running.id); }).has_value());
}

TEST_CASE("active sync deduplicates identical parameters and rejects conflicts without profile mutation") {
    std::atomic<bool> entered{false};
    auto transport = [&](const import::HttpRequest& request) {
        if (request.url.ends_with("/archives"))
            return response(request.url, archives_json(1));
        entered = true;
        while (!request.cancellation.stop_requested()) std::this_thread::yield();
        throw import::ChessComClientError(import::ChessComFailure::Cancelled,
                                           ErrorCode::NetworkError, "cancelled");
    };
    IngestFixture fixture(transport);
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs, transport);
    const auto first = manager.start_sync(30, "alice");
    CHECK_EQ(manager.start_sync(30, "Alice").id, first.id);
    bool conflict = false;
    try {
        static_cast<void>(manager.start_sync(90, "bob"));
    } catch (const app::IngestConflict&) {
        conflict = true;
    }
    CHECK(conflict);
    CHECK_EQ(manager.profile()->normalized_username, "alice");
    CHECK(manager.cancel_sync(first.id));
}

TEST_CASE("checkpointed current month refreshes and resumed progress does not double count") {
    constexpr std::int64_t now = 1784116800000LL;
    std::atomic<int> july_fetches{0};
    auto transport = [&](const import::HttpRequest& request) {
        if (request.url.ends_with("/archives"))
            return response(request.url,
                R"({"archives":["https://api.chess.com/pub/player/alice/games/2026/06","https://api.chess.com/pub/player/alice/games/2026/07"]})");
        if (request.url.ends_with("/2026/07")) ++july_fetches;
        return response(request.url, R"({"games":[]})");
    };
    IngestFixture fixture(transport);
    fixture.repository.save_chesscom_profile({"alice", "", {}, "", 0, ""});
    fixture.repository.checkpoint_chesscom_month(
        {"alice", "2026-06", "https://api.chess.com/pub/player/alice/games/2026/06", 0, now - 2});
    fixture.repository.checkpoint_chesscom_month(
        {"alice", "2026-07", "https://api.chess.com/pub/player/alice/games/2026/07", 0, now - 1});
    fixture.repository.save_chesscom_sync_state(
        {"running", "alice", "v1;days=30;cutoff_ms=1781524800000;upper_ms=1784116800000",
         "2026-06", 1, 0, now - 10, now - 1, ""});
    app::IngestOptions options;
    options.clock = [] { return now; };
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs,
                               transport, {}, options);
    const auto done = wait_terminal([&] { return manager.sync("current"); });
    CHECK_EQ(done->status, "succeeded");
    CHECK_EQ(done->months_completed, 2ULL);
    CHECK_EQ(july_fetches.load(), 1);
}

TEST_CASE("startup profile reconciliation prevents a different persisted sync from resuming") {
    IngestFixture fixture;
    fixture.repository.save_chesscom_profile({"bob", "", {}, "", 0, ""});
    fixture.repository.save_chesscom_sync_state(
        {"running", "alice", "v1;days=30;cutoff_ms=1;upper_ms=2", "", 0, 0, 1, 1, ""});
    app::IngestManager manager(fixture.importer, fixture.repository, fixture.jobs,
                               {}, {}, {}, "bob");
    CHECK(!manager.sync("current").has_value());
    CHECK_EQ(fixture.repository.chesscom_sync_state().status, "paused");
}
