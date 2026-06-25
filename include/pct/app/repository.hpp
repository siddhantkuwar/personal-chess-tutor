#pragma once

#include "pct/analysis/analyzer.hpp"
#include "pct/common/json.hpp"
#include "pct/import/import_service.hpp"
#include "pct/storage/event_log.hpp"
#include "pct/training/training.hpp"

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace pct::app {

struct StoredGame {
    import::ImportedGame imported;
    std::optional<analysis::GameAnalysis> analysis;
    std::int64_t imported_at_ms{0};
    std::int64_t analyzed_at_ms{0};
    std::optional<analysis::GameAnalysis> shallow_analysis;
};

enum class AddResult { Added, Duplicate };

class Repository {
  public:
    explicit Repository(storage::EventLog& log);

    [[nodiscard]] AddResult add(const import::ImportedGame& imported);
    void save_analysis(const analysis::GameAnalysis& analysis);
    void save_shallow_analysis(const analysis::GameAnalysis& analysis);
    [[nodiscard]] std::optional<StoredGame> get(std::string_view id) const;
    [[nodiscard]] std::vector<StoredGame> list() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::vector<training::Drill> drills(std::int64_t now_ms) const;
    [[nodiscard]] std::optional<training::Drill> drill(std::string_view id) const;
    [[nodiscard]] bool add_validated_drill(training::Drill drill);
    [[nodiscard]] training::DrillAttempt record_attempt(std::string_view drill_id,
                                                        std::string move,
                                                        std::uint64_t response_time_ms,
                                                        int hint_level,
                                                        std::int64_t attempted_at_ms);
    [[nodiscard]] training::Drill advance_hint(std::string_view drill_id,
                                               std::int64_t now_ms);
    [[nodiscard]] training::Drill begin_drill_session(std::string_view drill_id,
                                                      std::int64_t now_ms);
    [[nodiscard]] training::Profile profile() const;
    [[nodiscard]] std::vector<training::Recommendation> recommendations();
    void complete_resource(std::string resource_id, std::int64_t completed_at_ms);
    [[nodiscard]] std::filesystem::path create_snapshot();
    [[nodiscard]] std::size_t compact_storage();
    void record_job_state(std::string game_id, std::string status);
    [[nodiscard]] std::vector<std::string> recoverable_analysis_jobs() const;
    void set_background_paused(bool paused);
    [[nodiscard]] bool background_paused() const;
    [[nodiscard]] json::Value create_batch(std::vector<std::string> game_ids,
                                           std::size_t discovered, std::size_t imported,
                                           std::size_t duplicates, std::size_t failed);
    [[nodiscard]] json::Value batches() const;

  private:
    storage::EventLog& log_;
    mutable std::mutex mutex_;
    std::map<std::string, StoredGame> games_;
    std::map<std::string, training::Drill> drills_;
    std::map<std::string, std::int64_t> resource_completions_;
    std::set<std::string> recommended_resources_;
    std::uint64_t next_attempt_id_{1};
    std::map<std::string, std::string> analysis_job_states_;
    bool background_paused_{false};
    std::map<std::string, json::Value> batches_;
    std::uint64_t next_batch_id_{1};

    void replay();
    void rebuild_indexes() const;
    [[nodiscard]] training::Profile profile_unlocked() const;
};

[[nodiscard]] json::Value to_json(const chess::Game& game);
[[nodiscard]] json::Value to_json(const analysis::GameAnalysis& analysis);
[[nodiscard]] json::Value to_json(const StoredGame& game, bool include_pgn = false);
[[nodiscard]] analysis::GameAnalysis analysis_from_json(const json::Value& value);

} // namespace pct::app
