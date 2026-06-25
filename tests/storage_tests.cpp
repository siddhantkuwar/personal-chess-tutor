#include "test.hpp"

#include "pct/storage/event_log.hpp"
#include "pct/common/error.hpp"

#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace pct::storage;

namespace {

std::filesystem::path temp_path(std::string_view name) {
    return std::filesystem::temp_directory_path() /
           ("pct-" + std::string(name) + "-" + std::to_string(::getpid()) + ".log");
}

struct TempFile {
    std::filesystem::path path;
    ~TempFile() {
        std::filesystem::remove(path);
    }
};

} // namespace

TEST_CASE("event log appends and replays ordered checksummed records") {
    TempFile file{temp_path("replay")};
    EventLog log(file.path);
    const Event first = log.append(EventType::GameImported, "game-one");
    const Event second = log.append(EventType::GameParsed, "parsed-one");
    CHECK_EQ(first.id, 1ULL);
    CHECK_EQ(second.id, 2ULL);
    const ReplayResult replay = log.replay();
    CHECK_EQ(replay.events.size(), 2ULL);
    CHECK_EQ(replay.events[0].payload, "game-one");
    CHECK_EQ(replay.events[1].payload, "parsed-one");
    CHECK(replay.corruptions.empty());
    CHECK(!replay.truncated_tail);
}

TEST_CASE("event log reports corruption and keeps later valid records") {
    TempFile file{temp_path("corruption")};
    EventLog log(file.path);
    const auto first = EventLog::serialize(log.append(EventType::GameImported, "first"));
    static_cast<void>(log.append(EventType::GameParsed, "second"));
    static_cast<void>(log.append(EventType::PositionAnalyzed, "third"));
    std::fstream stream(file.path, std::ios::binary | std::ios::in | std::ios::out);
    stream.seekp(static_cast<std::streamoff>(first.size() + 32));
    stream.put('X');
    stream.close();
    const ReplayResult replay = log.replay();
    CHECK_EQ(replay.events.size(), 2ULL);
    CHECK_EQ(replay.events[0].payload, "first");
    CHECK_EQ(replay.events[1].payload, "third");
    CHECK_EQ(replay.corruptions.size(), 1ULL);
}

TEST_CASE("partial trailing record can be recovered without losing valid events") {
    TempFile file{temp_path("trailing")};
    EventLog log(file.path);
    static_cast<void>(log.append(EventType::GameImported, "complete"));
    const std::uint64_t valid_size = std::filesystem::file_size(file.path);
    const Event partial{1, EventType::GameParsed, 2, 123, "partial"};
    const auto bytes = EventLog::serialize(partial);
    std::ofstream stream(file.path, std::ios::binary | std::ios::app);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size() / 2));
    stream.close();
    CHECK(log.replay().truncated_tail);
    CHECK(log.recover_trailing_record());
    CHECK_EQ(std::filesystem::file_size(file.path), valid_size);
    const ReplayResult replay = log.replay();
    CHECK_EQ(replay.events.size(), 1ULL);
    CHECK(!replay.truncated_tail);
}

TEST_CASE("event serialization is deterministic") {
    const Event event{1, EventType::MistakeDetected, 42, 123456, "payload"};
    CHECK(EventLog::serialize(event) == EventLog::serialize(event));
}

TEST_CASE("schema one records migrate during atomic validated compaction") {
    TempFile file{temp_path("compact")};
    const Event legacy{1, EventType::GameImported, 1, 123, "legacy"};
    const auto bytes = EventLog::serialize(legacy);
    {
        std::ofstream output(file.path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    EventLog log(file.path);
    CHECK_EQ(log.replay().events.front().schema_version, 1);
    CHECK_EQ(log.compact(), 3ULL);
    const auto replay = log.replay();
    CHECK_EQ(replay.events.size(), 3ULL);
    CHECK_EQ(replay.events.front().schema_version, current_schema_version);
    CHECK(replay.events[1].type == EventType::SchemaMigrated);
    CHECK(replay.events.back().type == EventType::LogCompacted);
}

TEST_CASE("compaction fault hooks retain a valid old or new log at every stage") {
    for (const auto failure : {CompactionStage::TemporaryWritten, CompactionStage::Validated,
                               CompactionStage::BeforeReplace, CompactionStage::Replaced}) {
        TempFile file{temp_path("compact-fault-" + std::to_string(static_cast<int>(failure)))};
        EventLog log(file.path);
        static_cast<void>(log.append(EventType::GameImported, "game"));
        CHECK_THROWS(log.compact([failure](CompactionStage stage) {
            if (stage == failure)
                throw std::runtime_error("injected interruption");
        }));
        EventLog reopened(file.path);
        const auto replay = reopened.replay();
        CHECK(replay.corruptions.empty());
        CHECK(!replay.truncated_tail);
        CHECK(!replay.events.empty());
    }
}

TEST_CASE("compaction removes superseded projection state while preserving latest value") {
    TempFile file{temp_path("compact-superseded")};
    EventLog log(file.path);
    static_cast<void>(log.append(EventType::AnalysisJobStateChanged,
                                 "{\"game_id\":\"g\",\"status\":\"queued\"}"));
    static_cast<void>(log.append(EventType::AnalysisJobStateChanged,
                                 "{\"game_id\":\"g\",\"status\":\"running\"}"));
    static_cast<void>(log.append(EventType::AnalysisJobStateChanged,
                                 "{\"game_id\":\"g\",\"status\":\"complete\"}"));
    CHECK_EQ(log.compact(), 2ULL);
    const auto replay = log.replay();
    CHECK_EQ(replay.events.size(), 2ULL);
    CHECK(replay.events.front().payload.find("complete") != std::string::npos);
    CHECK(replay.events.back().type == EventType::LogCompacted);
}

TEST_CASE("compaction drops shallow projections superseded by completed analysis") {
    TempFile file{temp_path("compact-shallow")};
    EventLog log(file.path);
    static_cast<void>(log.append(
        EventType::ShallowAnalysisCompleted,
        "{\"game_id\":\"g\",\"analysis\":{}}"));
    static_cast<void>(log.append(
        EventType::AnalysisCompleted,
        "{\"game_id\":\"g\",\"analysis\":{}}"));
    CHECK_EQ(log.compact(), 2ULL);
    const auto replay = log.replay();
    CHECK_EQ(replay.events.size(), 2ULL);
    CHECK(replay.events.front().type == EventType::AnalysisCompleted);
    CHECK(replay.events.back().type == EventType::LogCompacted);
}

TEST_CASE("append fault injection preserves acknowledged records and recovers partial tails") {
    const auto path = std::filesystem::temp_directory_path() /
                      ("pct-append-fault-" + std::to_string(::getpid()) + ".log");
    for (const auto stage : {AppendStage::BeforeWrite, AppendStage::AfterPartialWrite,
                             AppendStage::BeforeSync}) {
        std::filesystem::remove(path);
        {
            EventLog log(path);
            static_cast<void>(log.append(EventType::GameImported, "acknowledged"));
            log.set_append_fault_hook([&](AppendStage current) {
                if (current == stage)
                    throw pct::Error(pct::ErrorCode::IoError, "injected append interruption");
            });
            CHECK_THROWS(log.append(EventType::GameParsed, "interrupted"));
        }
        EventLog replayed(path);
        const auto before_recovery = replayed.replay();
        CHECK_EQ(before_recovery.events.front().payload, "acknowledged");
        const std::size_t expected_events = stage == AppendStage::BeforeSync ? 2 : 1;
        CHECK_EQ(before_recovery.events.size(), expected_events);
        if (stage == AppendStage::AfterPartialWrite)
            CHECK(replayed.recover_trailing_record());
        const auto recovered = replayed.replay();
        CHECK_EQ(recovered.events.size(), expected_events);
        CHECK(!recovered.truncated_tail);
    }
    std::filesystem::remove(path);
}

TEST_CASE("append interrupted after sync remains a valid durable event") {
    const auto path = std::filesystem::temp_directory_path() /
                      ("pct-after-sync-" + std::to_string(::getpid()) + ".log");
    std::filesystem::remove(path);
    {
        EventLog log(path);
        log.set_append_fault_hook([](AppendStage stage) {
            if (stage == AppendStage::AfterSync)
                throw pct::Error(pct::ErrorCode::IoError, "injected process exit after sync");
        });
        CHECK_THROWS(log.append(EventType::GameImported, "durable"));
    }
    EventLog replayed(path);
    const auto result = replayed.replay();
    CHECK_EQ(result.events.size(), 1ULL);
    CHECK_EQ(result.events.front().payload, "durable");
    CHECK(result.corruptions.empty());
    CHECK(!result.truncated_tail);
    std::filesystem::remove(path);
}
