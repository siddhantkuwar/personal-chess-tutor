#include "test.hpp"

#include "pct/storage/event_log.hpp"

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
