#pragma once

#include "pct/engine/stockfish.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace pct::engine {

struct EnginePoolOptions {
    std::size_t workers{1};
    std::size_t max_pending{256};
    std::size_t retry_limit{1};
};

struct EnginePoolStats {
    std::uint64_t submitted{0};
    std::uint64_t completed{0};
    std::uint64_t failed{0};
    std::uint64_t retried{0};
    std::uint64_t rejected{0};
    std::uint64_t active{0};
    std::uint64_t queued_interactive{0};
    std::uint64_t queued_current_game{0};
    std::uint64_t queued_historical{0};
    std::uint64_t maximum_queue_latency_ms{0};
};

using EngineFactory = std::function<std::unique_ptr<AnalysisEngine>(std::size_t worker_index)>;

class EnginePool final : public AnalysisEngine {
  public:
    EnginePool(EngineFactory factory, EnginePoolOptions options = {});
    ~EnginePool();

    EnginePool(const EnginePool&) = delete;
    EnginePool& operator=(const EnginePool&) = delete;

    [[nodiscard]] AnalysisResult analyze(const AnalysisRequest& request,
                                         CancellationToken stop_token = {}) override;
    [[nodiscard]] EnginePoolStats stats() const;
    [[nodiscard]] std::size_t worker_count() const noexcept;

  private:
    class State;
    std::unique_ptr<State> state_;
};

} // namespace pct::engine
