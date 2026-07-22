#pragma once

#include "pct/app/job_manager.hpp"
#include "pct/import/chesscom_archive_client.hpp"

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

inline constexpr std::size_t ingest_max_pending = 64;
inline constexpr std::size_t ingest_status_history = 100;
inline constexpr std::size_t ingest_archive_chunk_limit = 100;

struct IngestOptions {
    std::size_t max_pending{ingest_max_pending};
    std::size_t max_history{ingest_status_history};
    std::function<std::int64_t()> clock;
};

class IngestConflict final : public Error {
  public:
    explicit IngestConflict(std::string message)
        : Error(ErrorCode::InvalidArgument, std::move(message)) {}
};

struct ImportResolution {
    std::string id;
    std::string status{"queued"};
    std::string supplied_url;
    std::string canonical_url;
    std::string game_id;
    std::string username;
    std::string source;
    std::string imported_game_id;
    std::string code;
    std::string error;
    std::vector<std::string> actions;
    std::int64_t created_at_ms{0};
    std::int64_t updated_at_ms{0};
};

struct IngestSync {
    std::string id;
    std::string status{"queued"};
    std::string username;
    int days{30};
    std::size_t max_months{2};
    std::int64_t cutoff_ms{0};
    std::int64_t upper_bound_ms{0};
    std::string current_month;
    std::size_t months_completed{0};
    std::size_t games_indexed{0};
    std::string code;
    std::string error;
    std::int64_t created_at_ms{0};
    std::int64_t updated_at_ms{0};
};

using IngestObserver = std::function<void(const json::Value&)>;

class IngestManager {
  public:
    IngestManager(import::ImportService& importer, Repository& repository, JobManager& jobs,
                  import::HttpTransport transport = {}, import::RetrySleeper sleeper = {},
                  IngestOptions options = {}, std::string startup_username = {});
    ~IngestManager();

    IngestManager(const IngestManager&) = delete;
    IngestManager& operator=(const IngestManager&) = delete;

    void configure_profile(std::string username,
                           std::vector<std::string> time_controls = {});
    [[nodiscard]] std::optional<ChessComProfile> profile() const;

    [[nodiscard]] ImportResolution resolve(std::string url, std::string username = {});
    [[nodiscard]] std::optional<ImportResolution> resolution(std::string_view id) const;
    [[nodiscard]] bool cancel_resolution(std::string_view id);
    [[nodiscard]] IngestSync start_sync(int days, std::string username = {});
    [[nodiscard]] std::optional<IngestSync> sync(std::string_view id = "current") const;
    [[nodiscard]] bool cancel_sync(std::string_view id = "current");
    [[nodiscard]] json::Value snapshot() const;
    void set_observer(IngestObserver observer);

    [[nodiscard]] static std::size_t max_months_for_days(int days);

  private:
    struct SyncRuntime {
        std::vector<std::string> archives;
        std::size_t next_archive{0};
        bool discovered{false};
        CancellationSource cancellation;
    };

    import::ImportService& importer_;
    Repository& repository_;
    import::ChessComArchiveClient client_;
    IngestOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable interactive_condition_;
    std::condition_variable historical_condition_;
    std::condition_variable observer_condition_;
    std::deque<std::string> interactive_queue_;
    std::deque<std::string> historical_queue_;
    std::map<std::string, ImportResolution> resolutions_;
    std::map<std::string, CancellationSource> resolution_cancellations_;
    std::map<std::string, std::string> active_canonical_resolutions_;
    std::map<std::string, IngestSync> syncs_;
    std::map<std::string, SyncRuntime> sync_runtime_;
    std::deque<std::string> resolution_order_;
    std::deque<std::string> sync_order_;
    std::optional<std::string> current_sync_;
    std::uint64_t next_resolution_id_{1};
    std::uint64_t next_sync_id_{1};
    IngestObserver observer_;
    std::size_t observer_calls_{0};
    std::thread interactive_worker_;
    std::thread historical_worker_;
    bool stopping_{false};

    [[nodiscard]] std::int64_t now_ms() const;
    void interactive_work();
    void historical_work();
    void run_resolution(std::string id);
    void run_sync_step(std::string id);
    void finish_sync(std::string id, std::string status, std::string code = {},
                     std::string error = {});
    void notify(std::string_view event, const json::Value& payload);
    void trim_history_unlocked();
};

[[nodiscard]] json::Value to_json(const ChessComProfile& profile);
[[nodiscard]] json::Value to_json(const ChessComArchiveEntry& entry);
[[nodiscard]] json::Value to_json(const ImportResolution& resolution);
[[nodiscard]] json::Value to_json(const IngestSync& sync);

} // namespace pct::app
