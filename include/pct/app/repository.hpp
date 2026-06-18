#pragma once

#include "pct/analysis/analyzer.hpp"
#include "pct/common/json.hpp"
#include "pct/import/import_service.hpp"
#include "pct/storage/event_log.hpp"

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace pct::app {

struct StoredGame {
    import::ImportedGame imported;
    std::optional<analysis::GameAnalysis> analysis;
};

enum class AddResult { Added, Duplicate };

class Repository {
  public:
    explicit Repository(storage::EventLog& log);

    [[nodiscard]] AddResult add(const import::ImportedGame& imported);
    void save_analysis(const analysis::GameAnalysis& analysis);
    [[nodiscard]] std::optional<StoredGame> get(std::string_view id) const;
    [[nodiscard]] std::vector<StoredGame> list() const;
    [[nodiscard]] std::size_t size() const;

  private:
    storage::EventLog& log_;
    mutable std::mutex mutex_;
    std::map<std::string, StoredGame> games_;

    void replay();
    void rebuild_indexes() const;
};

[[nodiscard]] json::Value to_json(const chess::Game& game);
[[nodiscard]] json::Value to_json(const analysis::GameAnalysis& analysis);
[[nodiscard]] json::Value to_json(const StoredGame& game, bool include_pgn = false);
[[nodiscard]] analysis::GameAnalysis analysis_from_json(const json::Value& value);

} // namespace pct::app
