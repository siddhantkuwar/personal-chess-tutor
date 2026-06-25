#include "pct/analysis/analyzer.hpp"
#include "pct/app/repository.hpp"
#include "pct/chess/bitboard.hpp"
#include "pct/chess/pgn.hpp"
#include "pct/common/json.hpp"
#include "pct/engine/pool.hpp"
#include "pct/import/import_service.hpp"
#include "pct/storage/event_log.hpp"

#include <chrono>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace allocation_counter {
std::atomic<bool> enabled{false};
std::atomic<std::uint64_t> count{0};
}

void* operator new(std::size_t size) {
    if (allocation_counter::enabled.load(std::memory_order_relaxed))
        allocation_counter::count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
#include <unistd.h>

namespace {

std::uint64_t sink = 0;

class SyntheticEngine final : public pct::engine::AnalysisEngine {
  public:
    pct::engine::AnalysisResult analyze(const pct::engine::AnalysisRequest&,
                                        pct::CancellationToken) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return {{{1, 1, 0, std::nullopt, 1, 10, {"e2e4"}}}, "e2e4", {}};
    }
};

template <typename Function>
pct::json::Value measure(std::string name, std::size_t operations, Function function) {
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < operations; ++index)
        sink ^= static_cast<std::uint64_t>(function(index));
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    return pct::json::Value::Object{
        {"name", std::move(name)},
        {"operations", operations},
        {"total_ns", static_cast<double>(elapsed)},
        {"ns_per_operation", static_cast<double>(elapsed) / static_cast<double>(operations)},
    };
}

template <typename Function>
pct::json::Value measure_allocations(std::string name, std::size_t operations, Function function) {
    allocation_counter::count = 0;
    allocation_counter::enabled = true;
    for (std::size_t index = 0; index < operations; ++index)
        sink ^= static_cast<std::uint64_t>(function(index));
    allocation_counter::enabled = false;
    const auto allocations = allocation_counter::count.load();
    return pct::json::Value::Object{
        {"name", std::move(name)}, {"operations", operations},
        {"allocations", static_cast<double>(allocations)},
        {"allocations_per_operation", static_cast<double>(allocations) /
                                           static_cast<double>(operations)},
    };
}

pct::json::Value measure_pool(std::size_t workers) {
    pct::engine::EnginePool pool(
        [](std::size_t) { return std::make_unique<SyntheticEngine>(); },
        pct::engine::EnginePoolOptions{workers, 64, 0});
    constexpr std::size_t requests = 16;
    std::vector<std::future<pct::engine::AnalysisResult>> pending;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < requests; ++index) {
        pending.push_back(std::async(std::launch::async, [&] {
            return pool.analyze(pct::engine::AnalysisRequest{});
        }));
    }
    for (auto& result : pending)
        sink ^= result.get().lines.size();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    return pct::json::Value::Object{
        {"name", "synthetic_engine_pool_" + std::to_string(workers)},
        {"operations", requests},
        {"total_us", static_cast<double>(elapsed)},
        {"requests_per_second", static_cast<double>(requests) * 1000000.0 /
                                    static_cast<double>(elapsed)},
        {"maximum_queue_latency_ms",
         static_cast<std::size_t>(pool.stats().maximum_queue_latency_ms)},
    };
}

} // namespace

int main() {
    using namespace pct;
    using namespace pct::chess;
    json::Value::Array results;
    const std::string initial_fen = Board::initial().to_fen();
    const std::string pgn =
        "[White \"Synthetic\"]\n[Black \"Fixture\"]\n[Result \"1-0\"]\n\n"
        "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 1-0";

    results.push_back(measure("fen_parse", 20000, [&](std::size_t) {
        return Board::from_fen(initial_fen).hash();
    }));
    results.push_back(measure("pgn_parse", 2000, [&](std::size_t) {
        return parse_pgn(pgn).plies.size();
    }));
    results.push_back(measure_allocations("fen_parse_allocations", 1000, [&](std::size_t) {
        return Board::from_fen(initial_fen).hash();
    }));
    results.push_back(measure_allocations("pgn_parse_allocations", 100, [&](std::size_t) {
        return parse_pgn(pgn).plies.size();
    }));
    results.push_back(measure("legal_move_generation", 10000, [&](std::size_t) {
        Board board = Board::initial();
        return board.legal_moves().size();
    }));
    Board array_moves = Board::initial();
    const Move array_move = array_moves.legal_moves().front();
    results.push_back(measure("array_make_unmake", 20000, [&](std::size_t index) {
        static_cast<void>(index);
        const Undo undo = array_moves.make_move(array_move);
        array_moves.unmake_move(array_move, undo);
        return array_moves.hash();
    }));
    BitboardBoard bitboard_moves = BitboardBoard::initial();
    const Move bitboard_move = bitboard_moves.legal_moves().front();
    results.push_back(measure("bitboard_make_unmake", 20000, [&](std::size_t index) {
        static_cast<void>(index);
        const Undo undo = bitboard_moves.make_move(bitboard_move);
        bitboard_moves.unmake_move(bitboard_move, undo);
        return bitboard_moves.hash();
    }));
    const Board array_lookup = Board::initial();
    results.push_back(measure("array_square_lookup", 1000000, [&](std::size_t index) {
        return static_cast<std::size_t>(array_lookup.at(static_cast<Square>(index % 64)).type);
    }));
    const BitboardBoard bitboard_lookup = BitboardBoard::initial();
    results.push_back(measure("bitboard_square_lookup", 1000000, [&](std::size_t index) {
        return static_cast<std::size_t>(bitboard_lookup.at(static_cast<Square>(index % 64)).type);
    }));
    results.push_back(measure("zobrist_hash_validation", 200000, [&](std::size_t) {
        return array_lookup.hash_is_consistent() ? 1 : 0;
    }));
    results.push_back(measure("perft_depth_4", 1, [&](std::size_t) {
        Board board = Board::initial();
        return board.perft(4);
    }));
    storage::Event event{storage::current_schema_version, storage::EventType::GameImported, 1, 1,
                         std::string(512, 'x')};
    results.push_back(measure("event_serialize_512_bytes", 20000, [&](std::size_t) {
        return storage::EventLog::serialize(event).size();
    }));

    analysis::AnalysisCache cache(1024);
    engine::AnalysisRequest cache_request;
    cache_request.fen = initial_fen;
    cache.put(cache_request, engine::AnalysisResult{});
    results.push_back(measure("cache_hit", 200000, [&](std::size_t) {
        engine::AnalysisResult value;
        return cache.get(cache_request, value) ? 1 : 0;
    }));

    const auto replay_path = std::filesystem::temp_directory_path() /
                             ("pct-benchmark-" + std::to_string(::getpid()) + ".log");
    std::filesystem::remove(replay_path);
    storage::EventLog log(replay_path);
    for (int index = 0; index < 100; ++index)
        static_cast<void>(log.append(storage::EventType::GameImported, std::string(512, 'x')));
    results.push_back(measure("event_replay_100_records", 100, [&](std::size_t) {
        return log.replay().events.size();
    }));
    std::filesystem::remove(replay_path);

    const auto repository_directory = std::filesystem::temp_directory_path() /
                                      ("pct-repository-benchmark-" +
                                       std::to_string(::getpid()));
    std::filesystem::remove_all(repository_directory);
    const auto repository_path = repository_directory / "events.log";
    {
        storage::EventLog repository_log(repository_path);
        app::Repository repository(repository_log);
        import::ImportService importer;
        for (int index = 0; index < 10; ++index) {
            const std::string fixture =
                "[Event \"Synthetic " + std::to_string(index) +
                "\"]\n[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n"
                "1. e4 e5 2. Nf3 Nc6 1-0";
            static_cast<void>(repository.add(importer.from_pgn(fixture)));
        }
        static_cast<void>(repository.create_snapshot());
    }
    results.push_back(measure("snapshot_repository_load_10_games", 20, [&](std::size_t) {
        storage::EventLog repository_log(repository_path);
        app::Repository repository(repository_log);
        return repository.size();
    }));
    results.push_back(measure("derived_index_rebuild_10_games", 1, [&](std::size_t) {
        std::filesystem::remove(repository_directory / "games.idx");
        storage::EventLog repository_log(repository_path);
        app::Repository repository(repository_log);
        return repository.size();
    }));
    std::filesystem::remove_all(repository_directory);
    results.push_back(measure_pool(1));
    results.push_back(measure_pool(2));
    results.push_back(measure_pool(4));

    std::cout << json::dump(json::Value::Object{
                     {"schema", "pct-benchmark-1"},
                     {"dataset_profiles", json::Value::Array{
                                              json::Value::Object{{"name", "small"}, {"games", 10}},
                                              json::Value::Object{{"name", "medium"}, {"games", 100}},
                                              json::Value::Object{{"name", "large"}, {"games", 1000}},
                                          }},
                     {"results", std::move(results)},
                     {"sink", static_cast<double>(sink)},
                 })
              << '\n';
}
