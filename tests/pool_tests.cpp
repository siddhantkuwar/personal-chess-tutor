#include "test.hpp"

#include "pct/common/error.hpp"
#include "pct/engine/pool.hpp"
#include "pct/chess/board.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace pct;
using namespace pct::engine;

namespace {

AnalysisResult result_for(const AnalysisRequest& request) {
    return {{{1, request.depth, 0, std::nullopt, 1, 1, {"e2e4"}}}, "e2e4", {}};
}

struct GateState {
    std::mutex mutex;
    std::condition_variable condition;
    bool release{false};
    std::size_t started{0};
    std::vector<AnalysisPriority> order;
};

class GatedEngine final : public AnalysisEngine {
  public:
    explicit GatedEngine(std::shared_ptr<GateState> state) : state_(std::move(state)) {}

    AnalysisResult analyze(const AnalysisRequest& request, CancellationToken) override {
        std::unique_lock lock(state_->mutex);
        const std::size_t sequence = state_->started++;
        state_->order.push_back(request.priority);
        state_->condition.notify_all();
        if (sequence == 0)
            state_->condition.wait(lock, [&] { return state_->release; });
        return result_for(request);
    }

  private:
    std::shared_ptr<GateState> state_;
};

} // namespace

TEST_CASE("engine pool runs queued work by priority") {
    auto state = std::make_shared<GateState>();
    EnginePool pool([state](std::size_t) { return std::make_unique<GatedEngine>(state); },
                    EnginePoolOptions{1, 8, 0});
    AnalysisRequest current;
    current.priority = AnalysisPriority::CurrentGame;
    auto first = std::async(std::launch::async, [&] { return pool.analyze(current); });
    {
        std::unique_lock lock(state->mutex);
        state->condition.wait(lock, [&] { return state->started == 1; });
    }
    AnalysisRequest historical;
    historical.priority = AnalysisPriority::Historical;
    auto low = std::async(std::launch::async, [&] { return pool.analyze(historical); });
    while (pool.stats().queued_historical != 1)
        std::this_thread::yield();
    AnalysisRequest interactive;
    interactive.priority = AnalysisPriority::Interactive;
    auto high = std::async(std::launch::async, [&] { return pool.analyze(interactive); });
    while (pool.stats().queued_interactive != 1)
        std::this_thread::yield();
    {
        std::lock_guard lock(state->mutex);
        state->release = true;
    }
    state->condition.notify_all();
    static_cast<void>(first.get());
    static_cast<void>(high.get());
    static_cast<void>(low.get());
    CHECK_EQ(state->order.size(), 3ULL);
    CHECK(state->order[1] == AnalysisPriority::Interactive);
    CHECK(state->order[2] == AnalysisPriority::Historical);
}

TEST_CASE("engine pool enforces bounded backpressure") {
    auto state = std::make_shared<GateState>();
    EnginePool pool([state](std::size_t) { return std::make_unique<GatedEngine>(state); },
                    EnginePoolOptions{1, 2, 0});
    AnalysisRequest request;
    auto first = std::async(std::launch::async, [&] { return pool.analyze(request); });
    {
        std::unique_lock lock(state->mutex);
        state->condition.wait(lock, [&] { return state->started == 1; });
    }
    auto second = std::async(std::launch::async, [&] { return pool.analyze(request); });
    while (pool.stats().submitted != 2)
        std::this_thread::yield();
    CHECK_THROWS(pool.analyze(request));
    CHECK_EQ(pool.stats().rejected, 1ULL);
    {
        std::lock_guard lock(state->mutex);
        state->release = true;
    }
    state->condition.notify_all();
    static_cast<void>(first.get());
    static_cast<void>(second.get());
}

TEST_CASE("engine pool retries an isolated worker failure") {
    auto calls = std::make_shared<std::atomic<int>>(0);
    EnginePool pool(
        [calls](std::size_t) {
            class FlakyEngine final : public AnalysisEngine {
              public:
                explicit FlakyEngine(std::shared_ptr<std::atomic<int>> calls)
                    : calls_(std::move(calls)) {}
                AnalysisResult analyze(const AnalysisRequest& request, CancellationToken) override {
                    if (calls_->fetch_add(1) == 0)
                        throw Error(ErrorCode::EngineError, "injected worker crash");
                    return result_for(request);
                }
              private:
                std::shared_ptr<std::atomic<int>> calls_;
            };
            return std::make_unique<FlakyEngine>(calls);
        },
        EnginePoolOptions{1, 4, 1});
    CHECK_EQ(pool.analyze(AnalysisRequest{}).best_move, "e2e4");
    CHECK_EQ(pool.stats().retried, 1ULL);
    CHECK_EQ(pool.stats().completed, 1ULL);
}

TEST_CASE("engine pool replaces a crashed Stockfish process without stalling") {
    auto creations = std::make_shared<std::atomic<int>>(0);
    EnginePool pool(
        [creations](std::size_t) {
            const char* executable = creations->fetch_add(1) == 0
                                         ? PCT_CRASH_STOCKFISH_PATH
                                         : PCT_FAKE_STOCKFISH_PATH;
            return std::make_unique<Stockfish>(StockfishOptions{executable, 16, 1});
        },
        EnginePoolOptions{1, 4, 1});
    AnalysisRequest request;
    request.fen = chess::Board::initial().to_fen();
    request.depth = 8;
    request.timeout = std::chrono::seconds(1);
    CHECK_EQ(pool.analyze(request).best_move, "e2e4");
    CHECK_EQ(pool.stats().retried, 1ULL);
    CHECK_EQ(pool.stats().failed, 0ULL);
}

TEST_CASE("engine pool propagates cancellation to its active worker") {
    EnginePool pool(
        [](std::size_t) {
            class WaitingEngine final : public AnalysisEngine {
              public:
                AnalysisResult analyze(const AnalysisRequest&, CancellationToken token) override {
                    while (!token.stop_requested())
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    throw Error(ErrorCode::Timeout, "cancelled");
                }
            };
            return std::make_unique<WaitingEngine>();
        },
        EnginePoolOptions{1, 4, 0});
    CancellationSource cancellation;
    auto pending = std::async(std::launch::async, [&] {
        return pool.analyze(AnalysisRequest{}, cancellation.get_token());
    });
    while (pool.stats().active != 1)
        std::this_thread::yield();
    cancellation.request_stop();
    CHECK_THROWS(pending.get());
    CHECK_EQ(pool.stats().failed, 1ULL);
}

TEST_CASE("engine pool uses multiple workers concurrently") {
    struct State {
        std::atomic<int> active{0};
        std::atomic<int> maximum{0};
        std::atomic<bool> release{false};
    };
    auto state = std::make_shared<State>();
    EnginePool pool(
        [state](std::size_t) {
            class ParallelEngine final : public AnalysisEngine {
              public:
                explicit ParallelEngine(std::shared_ptr<State> state) : state_(std::move(state)) {}
                AnalysisResult analyze(const AnalysisRequest& request, CancellationToken) override {
                    const int active = state_->active.fetch_add(1) + 1;
                    int prior = state_->maximum.load();
                    while (active > prior && !state_->maximum.compare_exchange_weak(prior, active)) {}
                    while (!state_->release.load())
                        std::this_thread::yield();
                    state_->active.fetch_sub(1);
                    return result_for(request);
                }
              private:
                std::shared_ptr<State> state_;
            };
            return std::make_unique<ParallelEngine>(state);
        },
        EnginePoolOptions{2, 4, 0});
    auto first = std::async(std::launch::async, [&] { return pool.analyze(AnalysisRequest{}); });
    auto second = std::async(std::launch::async, [&] { return pool.analyze(AnalysisRequest{}); });
    while (state->maximum.load() < 2)
        std::this_thread::yield();
    state->release = true;
    static_cast<void>(first.get());
    static_cast<void>(second.get());
    CHECK_EQ(state->maximum.load(), 2);
}

TEST_CASE("engine pool reserves capacity from historical work for interactive requests") {
    struct State {
        std::atomic<int> historical_active{0};
        std::atomic<bool> release{false};
    };
    auto state = std::make_shared<State>();
    EnginePool pool(
        [state](std::size_t) {
            class ReservedEngine final : public AnalysisEngine {
              public:
                explicit ReservedEngine(std::shared_ptr<State> state) : state_(std::move(state)) {}
                AnalysisResult analyze(const AnalysisRequest& request, CancellationToken) override {
                    if (request.priority == AnalysisPriority::Historical) {
                        state_->historical_active.fetch_add(1);
                        while (!state_->release.load())
                            std::this_thread::yield();
                        state_->historical_active.fetch_sub(1);
                    }
                    return result_for(request);
                }
              private:
                std::shared_ptr<State> state_;
            };
            return std::make_unique<ReservedEngine>(state);
        },
        EnginePoolOptions{2, 8, 0});
    AnalysisRequest historical;
    historical.priority = AnalysisPriority::Historical;
    auto first = std::async(std::launch::async, [&] { return pool.analyze(historical); });
    auto second = std::async(std::launch::async, [&] { return pool.analyze(historical); });
    while (state->historical_active.load() != 1 || pool.stats().queued_historical != 1)
        std::this_thread::yield();
    AnalysisRequest interactive;
    interactive.priority = AnalysisPriority::Interactive;
    auto high = std::async(std::launch::async, [&] { return pool.analyze(interactive); });
    CHECK(high.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready);
    static_cast<void>(high.get());
    CHECK_EQ(state->historical_active.load(), 1);
    state->release = true;
    static_cast<void>(first.get());
    static_cast<void>(second.get());
}
