#include "test.hpp"

#include "pct/app/job_manager.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <thread>
#include <vector>
#include <unistd.h>

using namespace pct;

namespace {

class LoadEngine final : public engine::AnalysisEngine {
  public:
    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   CancellationToken token) override {
        if (request.priority == engine::AnalysisPriority::Historical) {
            const int active = historical_active.fetch_add(1) + 1;
            int prior = maximum_historical.load();
            while (active > prior &&
                   !maximum_historical.compare_exchange_weak(prior, active)) {}
            for (int wait = 0; wait < 4 && !token.stop_requested(); ++wait)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            historical_active.fetch_sub(1);
        }
        chess::Board board = chess::Board::from_fen(request.fen);
        const auto moves = board.legal_moves();
        const std::string best = moves.empty() ? "(none)" : chess::uci(moves.front());
        return {{{1, request.depth, 0, std::nullopt, 1, 1, {best}}}, best, {}};
    }

    std::atomic<int> historical_active{0};
    std::atomic<int> maximum_historical{0};
};

std::string synthetic_pgn(int index) {
    return "[Event \"Synthetic " + std::to_string(index) +
           "\"]\n[White \"Player\"]\n[Black \"Opponent " + std::to_string(index) +
           "\"]\n[Result \"1-0\"]\n\n1. e4 e5 2. Nf3 Nc6 1-0";
}

} // namespace

TEST_CASE("historical endurance load preserves interactive capacity and durable completion") {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("pct-phase3-load-" + std::to_string(::getpid()));
    std::filesystem::remove_all(directory);
    const auto path = directory / "events.log";
    storage::EventLog log(path);
    app::Repository repository(log);
    import::ImportService importer;
    std::vector<std::string> historical;
    for (int index = 0; index < 12; ++index) {
        const auto imported = importer.from_pgn(synthetic_pgn(index));
        static_cast<void>(repository.add(imported));
        historical.push_back(imported.game.identity);
    }
    const auto current = importer.from_pgn(
        "[Event \"Interactive\"]\n[White \"Player\"]\n[Black \"Now\"]\n"
        "[Result \"1-0\"]\n\n1. d4 d5 2. c4 e6 1-0");
    static_cast<void>(repository.add(current));
    training::Drill live_drill;
    live_drill.id = "phase3-live-drill";
    live_drill.source_game_id = current.game.identity;
    live_drill.fen = chess::Board::initial().to_fen();
    live_drill.category = "Interactive load";
    live_drill.phase = "opening";
    live_drill.explanation = "A legal move remains responsive during historical analysis.";
    live_drill.solutions = {"a2a3"};
    live_drill.validation_evidence = {"legal fixture", "deterministic load verifier"};
    CHECK(repository.add_validated_drill(live_drill));
    LoadEngine engine;
    analysis::AnalysisCache cache(128);
    analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{2, 3, 80, 2, 1});
    app::JobManager jobs(repository, analyzer, app::JobManagerOptions{2, 32, 1});
    static_cast<void>(jobs.start_batch(historical));
    const auto started = std::chrono::steady_clock::now();
    const auto interactive = jobs.start(current.game.identity);
    static_cast<void>(repository.begin_drill_session(live_drill.id, 1000));
    const auto live_attempt = repository.record_attempt(live_drill.id, "a2a3", 25, 0, 1025);
    CHECK(live_attempt.correct);
    for (int attempt = 0; attempt < 200 && !repository.get(current.game.identity)->analysis;
         ++attempt) {
        static_cast<void>(repository.profile());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const auto interactive_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - started)
                                    .count();
    CHECK(repository.get(current.game.identity)->analysis.has_value());
    CHECK(interactive_ms < 400);
    CHECK_EQ(engine.maximum_historical.load(), 1);

    for (int attempt = 0; attempt < 1000; ++attempt) {
        const bool complete = std::all_of(historical.begin(), historical.end(), [&](const auto& id) {
            return repository.get(id)->analysis.has_value();
        });
        if (complete)
            break;
        static_cast<void>(repository.drills(0));
        static_cast<void>(repository.profile());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    for (const auto& id : historical)
        CHECK(repository.get(id)->analysis.has_value());
    CHECK(jobs.queued_count() == 0);

    std::map<std::string, int> completions;
    for (const auto& event : log.replay().events) {
        if (event.type == storage::EventType::AnalysisCompleted)
            ++completions[json::parse(event.payload).at("game_id").as_string()];
    }
    for (const auto& [_, count] : completions)
        CHECK_EQ(count, 1);
    CHECK_EQ(repository.drill(live_drill.id)->attempts.size(), 1ULL);
    std::filesystem::remove_all(directory);
}

TEST_CASE("job queue rejects work beyond its configured bound without partial admission") {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("pct-phase3-bound-" + std::to_string(::getpid()));
    std::filesystem::remove_all(directory);
    storage::EventLog log(directory / "events.log");
    app::Repository repository(log);
    import::ImportService importer;
    std::vector<std::string> ids;
    for (int index = 0; index < 2; ++index) {
        const auto imported = importer.from_pgn(synthetic_pgn(index + 100));
        static_cast<void>(repository.add(imported));
        ids.push_back(imported.game.identity);
    }
    repository.set_background_paused(true);
    LoadEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer(engine, cache);
    app::JobManager jobs(repository, analyzer, app::JobManagerOptions{1, 1, 0});
    CHECK_THROWS(jobs.start_batch(ids));
    CHECK_EQ(jobs.queued_count(), 0ULL);
    CHECK(jobs.list().empty());
    std::filesystem::remove_all(directory);
}
