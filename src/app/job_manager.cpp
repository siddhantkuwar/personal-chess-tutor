#include "pct/app/job_manager.hpp"

#include "pct/common/error.hpp"
#include "pct/common/log.hpp"

#include <algorithm>

namespace pct::app {

JobManager::JobManager(Repository& repository, analysis::Analyzer& analyzer)
    : repository_(repository), analyzer_(analyzer),
      worker_([this](std::stop_token token) { work(token); }) {}

JobManager::~JobManager() {
    worker_.request_stop();
    condition_.notify_all();
}

AnalysisJob JobManager::start(std::string game_id) {
    if (!repository_.get(game_id))
        throw Error(ErrorCode::NotFound, "game does not exist");
    AnalysisJob result;
    {
        std::lock_guard lock(mutex_);
        for (const auto& [_, job] : jobs_) {
            if (job.game_id == game_id &&
                (job.status == JobStatus::Queued || job.status == JobStatus::Running ||
                 job.status == JobStatus::Complete)) {
                return job;
            }
        }
        result.id = next_id_++;
        result.game_id = std::move(game_id);
        result.status = JobStatus::Queued;
        result.progress = analysis::Progress{analysis::AnalysisStage::Parsing, 0, 1, "Queued"};
        jobs_.emplace(result.id, result);
        queue_.push_back(result.id);
    }
    condition_.notify_one();
    notify(result);
    return result;
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

void JobManager::work(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::uint64_t id = 0;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, stop_token, [&] { return !queue_.empty(); });
            if (stop_token.stop_requested())
                return;
            id = queue_.front();
            queue_.pop_front();
            auto found = jobs_.find(id);
            if (found == jobs_.end() || found->second.status == JobStatus::Cancelled)
                continue;
            found->second.status = JobStatus::Running;
        }
        if (const auto job = get(id))
            notify(*job);

        try {
            const auto job = get(id);
            if (!job)
                continue;
            const auto stored = repository_.get(job->game_id);
            if (!stored)
                throw Error(ErrorCode::NotFound, "game disappeared before analysis");
            if (stored->analysis) {
                std::lock_guard lock(mutex_);
                jobs_.at(id).status = JobStatus::Complete;
                jobs_.at(id).progress = analysis::Progress{analysis::AnalysisStage::Complete, 1, 1,
                                                           "Loaded from storage"};
            } else {
                const analysis::GameAnalysis result = analyzer_.analyze(
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
                    job->cancellation.get_token());
                repository_.save_analysis(result);
                std::lock_guard lock(mutex_);
                jobs_.at(id).status = JobStatus::Complete;
                jobs_.at(id).progress = analysis::Progress{analysis::AnalysisStage::Complete, 1, 1,
                                                           "Analysis complete"};
            }
        } catch (const Error& error) {
            std::lock_guard lock(mutex_);
            auto& job = jobs_.at(id);
            if (job.cancellation.stop_requested()) {
                job.status = JobStatus::Cancelled;
                job.error.clear();
            } else {
                job.status = JobStatus::Failed;
                job.error = error.what();
                log(LogLevel::Error, "jobs", "analysis job failed: " + job.error);
            }
        } catch (const std::exception& error) {
            std::lock_guard lock(mutex_);
            auto& job = jobs_.at(id);
            job.status = JobStatus::Failed;
            job.error = error.what();
            log(LogLevel::Error, "jobs", "analysis job failed: " + job.error);
        }
        if (const auto job = get(id))
            notify(*job);
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
