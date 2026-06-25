#include "test.hpp"

#include "pct/app/repository.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
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

json::Value read_json(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::stringstream contents;
    contents << input.rdbuf();
    return json::parse(contents.str());
}

void remove_repository_files(const std::filesystem::path& path) {
    std::filesystem::remove(path);
    for (const std::string name : {"games.idx", "positions.idx", "mistakes.idx", "drills.idx",
                                   "profile.idx", "resources.idx", "ratings.idx", "snapshots.idx"})
        std::filesystem::remove(path.parent_path() / name);
    std::filesystem::remove_all(path.parent_path() / "snapshots");
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
        CHECK(std::filesystem::exists(path.parent_path() / "drills.idx"));
        CHECK(std::filesystem::exists(path.parent_path() / "profile.idx"));
        CHECK(std::filesystem::exists(path.parent_path() / "resources.idx"));
        CHECK(std::filesystem::exists(path.parent_path() / "ratings.idx"));
        CHECK(std::filesystem::exists(path.parent_path() / "snapshots.idx"));
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
    remove_repository_files(path);
}

TEST_CASE("repository rebuilds deleted and corrupted derived indexes from the event log") {
    const auto path = repository_path();
    remove_repository_files(path);
    const chess::Game parsed = chess::parse_pgn(pgn);
    const import::ImportedGame imported{
        parsed, {}, std::string(pgn), import::ImportMethod::ManualPgn};
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        CHECK(repository.add(imported) == app::AddResult::Added);
        static_cast<void>(repository.create_snapshot());
    }

    const auto directory = path.parent_path();
    std::filesystem::remove(directory / "profile.idx");
    {
        std::ofstream output(directory / "resources.idx", std::ios::trunc);
        output << "not json";
    }
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        CHECK_EQ(repository.size(), 1ULL);
    }

    CHECK_EQ(read_json(directory / "profile.idx").at("version").as_int(), 1);
    CHECK_EQ(read_json(directory / "profile.idx").at("profile").at("games_imported").as_int(), 1);
    CHECK_EQ(read_json(directory / "resources.idx").at("catalog_version").as_string(),
             std::string(training::catalog_version));
    CHECK_EQ(read_json(directory / "ratings.idx").at("ratings").as_array().size(), 0ULL);
    const auto snapshots = read_json(directory / "snapshots.idx").at("snapshots").as_array();
    CHECK_EQ(snapshots.size(), 1ULL);
    CHECK(snapshots.front().at("valid").as_bool());
    remove_repository_files(path);
}

TEST_CASE("repository ignores interrupted and corrupted snapshots without losing events") {
    const auto path = repository_path();
    remove_repository_files(path);
    const chess::Game parsed = chess::parse_pgn(pgn);
    const import::ImportedGame imported{
        parsed, {}, std::string(pgn), import::ImportMethod::ManualPgn};
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        CHECK(repository.add(imported) == app::AddResult::Added);
    }
    const auto snapshots = path.parent_path() / "snapshots";
    std::filesystem::create_directories(snapshots);
    {
        std::ofstream partial(snapshots / "projection-999.json.tmp");
        partial << "{\"games\":[";
    }
    {
        std::ofstream corrupt(snapshots / "projection-998.json");
        corrupt << "not a snapshot";
    }
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        CHECK_EQ(repository.size(), 1ULL);
        CHECK(repository.get(parsed.identity).has_value());
    }
    const auto indexed = read_json(path.parent_path() / "snapshots.idx").at("snapshots").as_array();
    CHECK_EQ(indexed.size(), 1ULL);
    CHECK(!indexed.front().at("valid").as_bool());
    remove_repository_files(path);
}
