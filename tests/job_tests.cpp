#include "test.hpp"

#include "pct/app/job_manager.hpp"

#include <filesystem>
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
