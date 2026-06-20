#pragma once

#include "pct/analysis/analyzer.hpp"
#include "pct/app/repository.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace pct::app {

enum class JobStatus { Queued, Running, Complete, Failed, Cancelled };

struct AnalysisJob {
    std::uint64_t id{0};
    std::string game_id;
    JobStatus status{JobStatus::Queued};
    analysis::Progress progress;
    std::string error;
    CancellationSource cancellation;
};

using JobObserver = std::function<void(const AnalysisJob&)>;

class JobManager {
  public:
    JobManager(Repository& repository, analysis::Analyzer& analyzer);
    ~JobManager();

    JobManager(const JobManager&) = delete;
    JobManager& operator=(const JobManager&) = delete;

    [[nodiscard]] AnalysisJob start(std::string game_id);
    [[nodiscard]] std::vector<AnalysisJob> start_batch(
        const std::vector<std::string>& game_ids);
    [[nodiscard]] bool cancel(std::uint64_t job_id);
    [[nodiscard]] std::optional<AnalysisJob> get(std::uint64_t job_id) const;
    [[nodiscard]] std::vector<AnalysisJob> list() const;
    void pause();
    void resume();
    [[nodiscard]] bool paused() const;
    [[nodiscard]] std::size_t cache_hits() const { return analyzer_.cache_hits(); }
    void set_observer(JobObserver observer);

  private:
    Repository& repository_;
    analysis::Analyzer& analyzer_;
    mutable std::mutex mutex_;
    std::condition_variable_any condition_;
    std::map<std::uint64_t, AnalysisJob> jobs_;
    struct Task {
        std::uint64_t job_id{0};
        bool deep{false};
    };
    std::deque<Task> queue_;
    std::uint64_t next_id_{1};
    JobObserver observer_;
    std::thread worker_;
    CancellationSource worker_cancellation_;
    bool paused_{false};

    void work(CancellationToken stop_token);
    void notify(const AnalysisJob& job);
};

[[nodiscard]] std::string_view name(JobStatus status);
[[nodiscard]] json::Value to_json(const AnalysisJob& job);

} // namespace pct::app
