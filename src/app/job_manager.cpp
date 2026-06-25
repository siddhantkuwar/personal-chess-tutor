#include "pct/app/job_manager.hpp"

#include "pct/common/error.hpp"
#include "pct/common/log.hpp"

#include <algorithm>
#include <cctype>

namespace pct::app {
namespace {

std::string recency_key(const StoredGame& game) {
    std::string value = game.imported.game.tag("UTCDate");
    if (value.empty())
        value = game.imported.game.tag("Date");
    value += game.imported.game.tag("UTCTime");
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char character) {
                    return !std::isdigit(character);
                }),
                value.end());
    return value.empty() ? std::to_string(game.imported_at_ms) : value;
}

} // namespace

JobManager::JobManager(Repository& repository, analysis::Analyzer& analyzer,
                       JobManagerOptions options)
    : repository_(repository), analyzer_(analyzer), options_(options),
      paused_(repository.background_paused()) {
    if (options_.workers == 0 || options_.max_queued == 0)
        throw Error(ErrorCode::InvalidArgument, "job manager bounds must be positive");
    for (const auto& game_id : repository_.recoverable_analysis_jobs())
        static_cast<void>(start(game_id, engine::AnalysisPriority::Historical));
    workers_.reserve(options_.workers);
    for (std::size_t index = 0; index < options_.workers; ++index)
        workers_.emplace_back([this] { work(worker_cancellation_.get_token()); });
}

JobManager::~JobManager() {
    worker_cancellation_.request_stop();
    {
        std::lock_guard lock(mutex_);
        for (auto& [_, job] : jobs_)
            if (job.status == JobStatus::Queued || job.status == JobStatus::Running)
                job.cancellation.request_stop();
    }
    condition_.notify_all();
    for (auto& worker : workers_)
        if (worker.joinable())
            worker.join();
}

AnalysisJob JobManager::start(std::string game_id, engine::AnalysisPriority priority) {
    return start_with_priority(std::vector<std::string>{std::move(game_id)}, priority).front();
}

std::vector<AnalysisJob> JobManager::start_batch(const std::vector<std::string>& game_ids) {
    return start_with_priority(game_ids, engine::AnalysisPriority::Historical);
}

std::vector<AnalysisJob>
JobManager::start_with_priority(const std::vector<std::string>& game_ids,
                                engine::AnalysisPriority priority) {
    struct Input {
        std::string game_id;
        bool has_shallow{false};
        bool complete{false};
        std::string recency;
    };
    std::vector<Input> inputs;
    inputs.reserve(game_ids.size());
    for (const auto& game_id : game_ids) {
        const auto stored = repository_.get(game_id);
        if (!stored)
            throw Error(ErrorCode::NotFound, "game does not exist");
        inputs.push_back(Input{game_id, stored->shallow_analysis.has_value(),
                               stored->analysis.has_value(), recency_key(*stored)});
    }
    std::stable_sort(inputs.begin(), inputs.end(), [](const Input& left, const Input& right) {
        if (left.complete != right.complete)
            return !left.complete;
        return left.recency > right.recency;
    });
    std::vector<AnalysisJob> results;
    std::vector<AnalysisJob> created;
    {
        std::lock_guard lock(mutex_);
        std::vector<Task> shallow_tasks;
        std::vector<Task> deep_tasks;
        for (const auto& input : inputs) {
            const auto& game_id = input.game_id;
            const auto existing = std::find_if(jobs_.begin(), jobs_.end(), [&](const auto& entry) {
                const auto& job = entry.second;
                return job.game_id == game_id &&
                       (job.status == JobStatus::Queued || job.status == JobStatus::Running ||
                        job.status == JobStatus::Complete);
            });
            if (existing != jobs_.end()) {
                results.push_back(existing->second);
                continue;
            }
            AnalysisJob result;
            result.id = next_id_++;
            result.game_id = game_id;
            result.status = input.complete ? JobStatus::Complete : JobStatus::Queued;
            result.progress = input.complete
                                  ? analysis::Progress{analysis::AnalysisStage::Complete, 1, 1,
                                                       "Loaded from storage"}
                                  : analysis::Progress{
                                        input.has_shallow ? analysis::AnalysisStage::DeepAnalysis
                                                          : analysis::AnalysisStage::Parsing,
                                        0, 1, input.has_shallow ? "Deep analysis queued"
                                                                : "Shallow analysis queued"};
            jobs_.emplace(result.id, result);
            if (!input.complete) {
                (input.has_shallow ? deep_tasks : shallow_tasks)
                    .push_back(Task{result.id, input.has_shallow, priority, 0});
            }
            created.push_back(result);
            results.push_back(result);
        }
        if (queued_count_unlocked() + shallow_tasks.size() + deep_tasks.size() >
            options_.max_queued) {
            for (const auto& job : created)
                jobs_.erase(job.id);
            throw Error(ErrorCode::InvalidArgument, "analysis queue backpressure limit reached");
        }
        for (const auto& task : shallow_tasks)
            enqueue_unlocked(task);
        for (const auto& task : deep_tasks)
            enqueue_unlocked(task);
    }
    condition_.notify_all();
    for (const auto& job : created) {
        repository_.record_job_state(job.game_id, std::string(name(job.status)));
        notify(job);
    }
    return results;
}

bool JobManager::cancel(std::uint64_t job_id) {
    AnalysisJob snapshot;
    {
        std::lock_guard lock(mutex_);
        const auto found = jobs_.find(job_id);
        if (found == jobs_.end() || found->second.status == JobStatus::Complete ||
            found->second.status == JobStatus::Failed ||
            found->second.status == JobStatus::Cancelled) {
            return false;
        }
        found->second.cancellation.request_stop();
        if (found->second.status == JobStatus::Queued)
            found->second.status = JobStatus::Cancelled;
        snapshot = found->second;
    }
    condition_.notify_all();
    repository_.record_job_state(snapshot.game_id, std::string(name(snapshot.status)));
    notify(snapshot);
    return true;
}

std::optional<AnalysisJob> JobManager::get(std::uint64_t job_id) const {
    std::lock_guard lock(mutex_);
    const auto found = jobs_.find(job_id);
    if (found == jobs_.end())
        return std::nullopt;
    return found->second;
}

std::vector<AnalysisJob> JobManager::list() const {
    std::lock_guard lock(mutex_);
    std::vector<AnalysisJob> result;
    result.reserve(jobs_.size());
    for (const auto& [_, job] : jobs_)
        result.push_back(job);
    return result;
}

void JobManager::pause() {
    {
        std::lock_guard lock(mutex_);
        paused_ = true;
    }
    repository_.set_background_paused(true);
}

void JobManager::resume() {
    {
        std::lock_guard lock(mutex_);
        paused_ = false;
    }
    repository_.set_background_paused(false);
    condition_.notify_all();
}

bool JobManager::paused() const {
    std::lock_guard lock(mutex_);
    return paused_;
}

std::size_t JobManager::queued_count_unlocked() const {
    std::size_t count = 0;
    for (const auto& queue : queues_)
        count += queue.size();
    return count;
}

std::size_t JobManager::queued_count() const {
    std::lock_guard lock(mutex_);
    return queued_count_unlocked();
}

bool JobManager::runnable_unlocked() const {
    if (!queues_[0].empty() || !queues_[1].empty())
        return true;
    const std::size_t historical_limit = options_.workers == 1 ? 1 : options_.workers - 1;
    return !queues_[2].empty() && active_by_priority_[2] < historical_limit;
}

void JobManager::enqueue_unlocked(Task task) {
    queues_[static_cast<std::size_t>(task.priority)].push_back(std::move(task));
}

JobManager::Task JobManager::take_unlocked() {
    for (std::size_t priority = 0; priority < queues_.size(); ++priority) {
        auto& queue = queues_[priority];
        if (!queue.empty()) {
            const std::size_t historical_limit = options_.workers == 1 ? 1 : options_.workers - 1;
            if (priority == 2 && active_by_priority_[2] >= historical_limit)
                continue;
            Task task = queue.front();
            queue.pop_front();
            ++active_by_priority_[priority];
            return task;
        }
    }
    throw Error(ErrorCode::Corruption, "job queue wakeup had no task");
}

void JobManager::finish_task(engine::AnalysisPriority priority) {
    {
        std::lock_guard lock(mutex_);
        --active_by_priority_[static_cast<std::size_t>(priority)];
    }
    condition_.notify_all();
}

void JobManager::set_observer(JobObserver observer) {
    std::lock_guard lock(mutex_);
    observer_ = std::move(observer);
}

void JobManager::notify(const AnalysisJob& job) {
    JobObserver observer;
    {
        std::lock_guard lock(mutex_);
        observer = observer_;
    }
    if (observer)
        observer(job);
}

void JobManager::work(CancellationToken stop_token) {
    while (!stop_token.stop_requested()) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [&] {
                return stop_token.stop_requested() || (!paused_ && runnable_unlocked());
            });
            if (stop_token.stop_requested())
                return;
            task = take_unlocked();
            auto found = jobs_.find(task.job_id);
            if (found == jobs_.end() || found->second.status == JobStatus::Cancelled) {
                --active_by_priority_[static_cast<std::size_t>(task.priority)];
                condition_.notify_all();
                continue;
            }
            found->second.status = JobStatus::Running;
        }
        const std::uint64_t id = task.job_id;
        if (const auto job = get(id))
            repository_.record_job_state(job->game_id, "running");
        if (const auto job = get(id))
            notify(*job);

        try {
            const auto job = get(id);
            if (!job)
                throw Error(ErrorCode::Corruption, "active analysis job disappeared");
            const auto stored = repository_.get(job->game_id);
            if (!stored)
                throw Error(ErrorCode::NotFound, "game disappeared before analysis");
            if (stored->analysis) {
                std::lock_guard lock(mutex_);
                jobs_.at(id).status = JobStatus::Complete;
                jobs_.at(id).progress = analysis::Progress{analysis::AnalysisStage::Complete, 1, 1,
                                                           "Loaded from storage"};
            } else if (!task.deep) {
                const analysis::GameAnalysis shallow = analyzer_.analyze_shallow(
                    stored->imported.game,
                    [this, id](const analysis::Progress& progress) {
                        AnalysisJob snapshot;
                        {
                            std::lock_guard lock(mutex_);
                            auto& job = jobs_.at(id);
                            job.progress = progress;
                            snapshot = job;
                        }
                        notify(snapshot);
                    },
                    job->cancellation.get_token(), task.priority);
                repository_.save_shallow_analysis(shallow);
                AnalysisJob snapshot;
                {
                    std::lock_guard lock(mutex_);
                    auto& queued = jobs_.at(id);
                    queued.status = JobStatus::Queued;
                    queued.progress = analysis::Progress{analysis::AnalysisStage::DeepAnalysis,
                                                         0, 1, "Deep analysis queued"};
                    enqueue_unlocked(Task{id, true, task.priority, 0});
                    snapshot = queued;
                }
                repository_.record_job_state(snapshot.game_id, "queued");
                notify(snapshot);
                condition_.notify_all();
                finish_task(task.priority);
                continue;
            } else {
                analysis::GameAnalysis shallow =
                    stored->shallow_analysis
                        ? *stored->shallow_analysis
                        : analyzer_.analyze_shallow(stored->imported.game, {},
                                                    job->cancellation.get_token(), task.priority);
                const analysis::GameAnalysis result = analyzer_.analyze_deep(
                    stored->imported.game, std::move(shallow),
                    [this, id](const analysis::Progress& progress) {
                        AnalysisJob snapshot;
                        {
                            std::lock_guard lock(mutex_);
                            auto& current = jobs_.at(id);
                            current.progress = progress;
                            snapshot = current;
                        }
                        notify(snapshot);
                    },
                    job->cancellation.get_token(), task.priority);
                repository_.save_analysis(result);
                std::lock_guard lock(mutex_);
                jobs_.at(id).status = JobStatus::Complete;
                jobs_.at(id).progress = analysis::Progress{analysis::AnalysisStage::Complete, 1, 1,
                                                           "Analysis complete"};
            }
        } catch (const Error& error) {
            bool retry = false;
            {
                std::lock_guard lock(mutex_);
                auto& job = jobs_.at(id);
                if (job.cancellation.stop_requested()) {
                    job.status = JobStatus::Cancelled;
                    job.error.clear();
                } else if ((error.code() == ErrorCode::EngineError ||
                            error.code() == ErrorCode::Timeout) &&
                           task.attempts < options_.retry_limit) {
                    job.status = JobStatus::Queued;
                    job.error.clear();
                    job.progress.message = "Retrying after engine failure";
                    ++task.attempts;
                    enqueue_unlocked(task);
                    retry = true;
                } else {
                    job.status = JobStatus::Failed;
                    job.error = error.what();
                    log(LogLevel::Error, "jobs", "analysis job failed: " + job.error);
                }
            }
            if (retry)
                condition_.notify_all();
        } catch (const std::exception& error) {
            std::lock_guard lock(mutex_);
            auto& job = jobs_.at(id);
            job.status = JobStatus::Failed;
            job.error = error.what();
            log(LogLevel::Error, "jobs", "analysis job failed: " + job.error);
        }
        finish_task(task.priority);
        if (const auto job = get(id)) {
            repository_.record_job_state(job->game_id, std::string(name(job->status)));
            notify(*job);
        }
    }
}

std::string_view name(JobStatus status) {
    switch (status) {
    case JobStatus::Queued:
        return "queued";
    case JobStatus::Running:
        return "running";
    case JobStatus::Complete:
        return "complete";
    case JobStatus::Failed:
        return "failed";
    case JobStatus::Cancelled:
        return "cancelled";
    }
    return "unknown";
}

json::Value to_json(const AnalysisJob& job) {
    return json::Value::Object{
        {"id", static_cast<double>(job.id)},
        {"game_id", job.game_id},
        {"status", std::string(name(job.status))},
        {"progress",
         json::Value::Object{
             {"stage", std::string(analysis::name(job.progress.stage))},
             {"complete", job.progress.complete},
             {"total", job.progress.total},
             {"message", job.progress.message},
         }},
        {"error", job.error},
    };
}

} // namespace pct::app
