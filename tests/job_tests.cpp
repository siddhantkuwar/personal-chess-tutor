#include "test.hpp"

#include "pct/app/job_manager.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unistd.h>

using namespace pct;

namespace {

class QuietEngine final : public engine::AnalysisEngine {
  public:
    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   std::stop_token) override {
        chess::Board board = chess::Board::from_fen(request.fen);
        const auto moves = board.legal_moves();
        const std::string best = moves.empty() ? "(none)" : chess::uci(moves.front());
        return engine::AnalysisResult{
            {engine::PrincipalVariation{1, request.depth, 0, std::nullopt, 1, 1, {best}}},
            best,
            {}};
    }
};

class StagedEngine final : public engine::AnalysisEngine {
  public:
    std::atomic<bool> allow_deep{false};

    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   std::stop_token stop_token) override {
        {
            std::lock_guard lock(mutex_);
            depths_.push_back(request.depth);
        }
        while (request.depth >= 3 && !allow_deep.load() && !stop_token.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        chess::Board board = chess::Board::from_fen(request.fen);
        const auto moves = board.legal_moves();
        const std::string best = moves.empty() ? "(none)" : chess::uci(moves.front());
        return {{{1, request.depth, 160, std::nullopt, 1, 1, {best}}}, best, {}};
    }

    [[nodiscard]] std::vector<int> depths() const {
        std::lock_guard lock(mutex_);
        return depths_;
    }

  private:
    mutable std::mutex mutex_;
    std::vector<int> depths_;
};

} // namespace

TEST_CASE("job manager runs analysis in background and deduplicates active work") {
    const auto path = std::filesystem::temp_directory_path() /
                      ("pct-jobs-" + std::to_string(::getpid()) + ".log");
    std::filesystem::remove(path);
    storage::EventLog log(path);
    app::Repository repository(log);
    import::ImportService importer;
    const auto imported =
        importer.from_pgn("[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1. e4 e5 1-0");
    static_cast<void>(repository.add(imported));
    QuietEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{2, 3, 80, 2, 1});
    app::JobManager jobs(repository, analyzer);
    const auto first = jobs.start(imported.game.identity);
    const auto duplicate = jobs.start(imported.game.identity);
    CHECK_EQ(first.id, duplicate.id);
    app::JobStatus status = app::JobStatus::Queued;
    for (int attempt = 0; attempt < 100; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        status = jobs.get(first.id)->status;
        if (status == app::JobStatus::Complete || status == app::JobStatus::Failed)
            break;
    }
    CHECK(status == app::JobStatus::Complete);
    CHECK(repository.get(imported.game.identity)->analysis.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("paused analysis queue and incomplete work resume after restart") {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("pct-jobs-resume-" + std::to_string(::getpid()));
    const auto path = directory / "events.log";
    std::filesystem::remove_all(directory);
    std::string game_id;
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        import::ImportService importer;
        const auto imported = importer.from_pgn(
            "[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1. e4 e5 1-0");
        game_id = imported.game.identity;
        static_cast<void>(repository.add(imported));
        QuietEngine engine;
        analysis::AnalysisCache cache;
        analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{2, 3, 80, 2, 1});
        repository.save_shallow_analysis(analyzer.analyze_shallow(imported.game));
        repository.record_job_state(game_id, "running");
        repository.set_background_paused(true);
    }
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        QuietEngine engine;
        analysis::AnalysisCache cache;
        analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{2, 3, 80, 2, 1});
        app::JobManager jobs(repository, analyzer);
        CHECK(jobs.paused());
        CHECK(repository.get(game_id)->shallow_analysis.has_value());
        CHECK_EQ(jobs.list().size(), 1ULL);
        CHECK(jobs.list().front().status == app::JobStatus::Queued);
        CHECK(jobs.list().front().progress.stage == analysis::AnalysisStage::DeepAnalysis);
        jobs.resume();
        for (int attempt = 0; attempt < 100 && !repository.get(game_id)->analysis; ++attempt)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        CHECK(repository.get(game_id)->analysis.has_value());
    }
    std::filesystem::remove_all(directory);
}

TEST_CASE("queued batch work can be cancelled and retried idempotently") {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("pct-jobs-retry-" + std::to_string(::getpid()));
    const auto path = directory / "events.log";
    std::filesystem::remove_all(directory);
    storage::EventLog log(path);
    app::Repository repository(log);
    import::ImportService importer;
    const auto imported = importer.from_pgn(
        "[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1. d4 d5 1-0");
    static_cast<void>(repository.add(imported));
    repository.set_background_paused(true);
    QuietEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{2, 3, 80, 2, 1});
    app::JobManager jobs(repository, analyzer);
    const auto cancelled = jobs.start(imported.game.identity);
    CHECK(jobs.cancel(cancelled.id));
    CHECK(jobs.get(cancelled.id)->status == app::JobStatus::Cancelled);
    const auto retried = jobs.start(imported.game.identity);
    CHECK(retried.id != cancelled.id);
    jobs.resume();
    for (int attempt = 0; attempt < 100 && !repository.get(imported.game.identity)->analysis;
         ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(repository.get(imported.game.identity)->analysis.has_value());
    std::filesystem::remove_all(directory);
}

TEST_CASE("batch scheduling persists every shallow projection before deep work") {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("pct-jobs-staged-" + std::to_string(::getpid()));
    const auto path = directory / "events.log";
    std::filesystem::remove_all(directory);
    storage::EventLog log(path);
    app::Repository repository(log);
    import::ImportService importer;
    const auto first = importer.from_pgn(
        "[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1. e4 e5 1-0");
    const auto second = importer.from_pgn(
        "[White \"A\"]\n[Black \"C\"]\n[Result \"0-1\"]\n\n1. d4 d5 0-1");
    static_cast<void>(repository.add(first));
    static_cast<void>(repository.add(second));
    StagedEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{2, 3, 80, 2, 1});
    app::JobManager jobs(repository, analyzer);
    const auto started =
        jobs.start_batch({first.game.identity, second.game.identity});
    CHECK_EQ(started.size(), 2ULL);
    for (int attempt = 0; attempt < 200; ++attempt) {
        if (repository.get(first.game.identity)->shallow_analysis &&
            repository.get(second.game.identity)->shallow_analysis)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(repository.get(first.game.identity)->shallow_analysis.has_value());
    CHECK(repository.get(second.game.identity)->shallow_analysis.has_value());
    CHECK(!repository.get(first.game.identity)->analysis.has_value());
    CHECK(!repository.get(second.game.identity)->analysis.has_value());
    CHECK_EQ(repository.profile().games_shallow_analyzed, 2ULL);
    CHECK(cache.hit_count() > 0);
    const auto depths_before_release = engine.depths();
    const auto first_deep = std::find_if(depths_before_release.begin(), depths_before_release.end(),
                                         [](int depth) { return depth >= 3; });
    CHECK(first_deep != depths_before_release.end());
    CHECK(std::find_if(first_deep + 1, depths_before_release.end(),
                       [](int depth) { return depth < 3; }) == depths_before_release.end());
    engine.allow_deep = true;
    for (int attempt = 0; attempt < 200; ++attempt) {
        if (repository.get(first.game.identity)->analysis &&
            repository.get(second.game.identity)->analysis)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(repository.get(first.game.identity)->analysis.has_value());
    CHECK(repository.get(second.game.identity)->analysis.has_value());
    std::filesystem::remove_all(directory);
}

TEST_CASE("batch scheduling prioritizes recent shallow work before resumed deep work") {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("pct-jobs-priority-" + std::to_string(::getpid()));
    const auto path = directory / "events.log";
    std::filesystem::remove_all(directory);
    storage::EventLog log(path);
    app::Repository repository(log);
    import::ImportService importer;
    const auto older = importer.from_pgn(
        "[Date \"2025.01.01\"]\n[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1. e4 e5 1-0");
    const auto recent = importer.from_pgn(
        "[Date \"2026.06.20\"]\n[White \"A\"]\n[Black \"C\"]\n[Result \"0-1\"]\n\n1. d4 d5 0-1");
    static_cast<void>(repository.add(older));
    static_cast<void>(repository.add(recent));
    StagedEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer(engine, cache, analysis::AnalyzerOptions{2, 3, 80, 2, 1});
    repository.save_shallow_analysis(analyzer.analyze_shallow(older.game));
    repository.set_background_paused(true);
    app::JobManager jobs(repository, analyzer);
    const auto started = jobs.start_batch({older.game.identity, recent.game.identity});
    CHECK_EQ(started.size(), 2ULL);
    CHECK_EQ(started.front().game_id, recent.game.identity);
    jobs.resume();
    for (int attempt = 0; attempt < 200 &&
                          !repository.get(recent.game.identity)->shallow_analysis;
         ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(repository.get(recent.game.identity)->shallow_analysis.has_value());
    CHECK(!repository.get(older.game.identity)->analysis.has_value());
    engine.allow_deep = true;
    for (int attempt = 0; attempt < 200; ++attempt) {
        if (repository.get(older.game.identity)->analysis &&
            repository.get(recent.game.identity)->analysis)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(repository.get(older.game.identity)->analysis.has_value());
    CHECK(repository.get(recent.game.identity)->analysis.has_value());
    std::filesystem::remove_all(directory);
}

TEST_CASE("completed batch games are retained without being requeued") {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("pct-jobs-complete-" + std::to_string(::getpid()));
    const auto path = directory / "events.log";
    std::filesystem::remove_all(directory);
    storage::EventLog log(path);
    app::Repository repository(log);
    import::ImportService importer;
    const auto imported = importer.from_pgn(
        "[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1. c4 c5 1-0");
    static_cast<void>(repository.add(imported));
    analysis::GameAnalysis completed;
    completed.game_id = imported.game.identity;
    repository.save_analysis(completed);
    QuietEngine engine;
    analysis::AnalysisCache cache;
    analysis::Analyzer analyzer(engine, cache);
    app::JobManager jobs(repository, analyzer);
    const auto job = jobs.start(imported.game.identity);
    CHECK(job.status == app::JobStatus::Complete);
    CHECK_EQ(job.progress.message, "Loaded from storage");
    std::filesystem::remove_all(directory);
}
