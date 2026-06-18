#include "test.hpp"

#include "pct/app/repository.hpp"

#include <filesystem>
#include <unistd.h>

using namespace pct;

namespace {

constexpr std::string_view pgn = R"pgn([White "Alex"]
[Black "Morgan"]
[Result "1-0"]
1. e4 e5 2. Nf3 Nc6 1-0)pgn";

std::filesystem::path repository_path() {
    return std::filesystem::temp_directory_path() /
           ("pct-repository-" + std::to_string(::getpid()) + ".log");
}

} // namespace

TEST_CASE("repository deduplicates imports and replays completed analysis") {
    const auto path = repository_path();
    std::filesystem::remove(path);
    const chess::Game parsed = chess::parse_pgn(pgn);
    const import::ImportedGame imported{
        parsed, {}, std::string(pgn), import::ImportMethod::ManualPgn};
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        CHECK(repository.add(imported) == app::AddResult::Added);
        CHECK(repository.add(imported) == app::AddResult::Duplicate);
        analysis::GameAnalysis completed;
        completed.game_id = parsed.identity;
        repository.save_analysis(completed);
        CHECK(repository.get(parsed.identity)->analysis.has_value());
        CHECK(std::filesystem::exists(path.parent_path() / "games.idx"));
        CHECK(std::filesystem::exists(path.parent_path() / "positions.idx"));
        CHECK(std::filesystem::exists(path.parent_path() / "mistakes.idx"));
    }
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        CHECK_EQ(repository.size(), 1ULL);
        CHECK_EQ(repository.list().front().imported.game.identity, parsed.identity);
        const auto restored = repository.get(parsed.identity);
        CHECK(restored.has_value());
        CHECK(restored->analysis.has_value());
        CHECK_EQ(restored->imported.game.plies.size(), 4ULL);
    }
    std::filesystem::remove(path);
    std::filesystem::remove(path.parent_path() / "games.idx");
    std::filesystem::remove(path.parent_path() / "positions.idx");
    std::filesystem::remove(path.parent_path() / "mistakes.idx");
}
