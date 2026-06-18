#include "pct/storage/event_log.hpp"

#include "pct/common/error.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>

namespace pct::storage {
namespace {

constexpr std::uint32_t magic = 0x45544350U; // PCTE in little-endian byte order.
constexpr std::uint16_t current_schema = 1;
constexpr std::size_t header_size = 32;
constexpr std::size_t checksum_size = 4;
constexpr std::size_t minimum_record_size = header_size + checksum_size;
constexpr std::size_t maximum_record_size = 16 * 1024 * 1024;

template <typename T> void append_little_endian(std::vector<std::byte>& output, T value) {
    using Unsigned = std::make_unsigned_t<T>;
    Unsigned encoded = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        output.push_back(static_cast<std::byte>((encoded >> (index * 8U)) & 0xffU));
    }
}

template <typename T> T read_little_endian(const std::byte* data) {
    using Unsigned = std::make_unsigned_t<T>;
    Unsigned value = 0;
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        value |= static_cast<Unsigned>(std::to_integer<unsigned>(data[index])) << (index * 8U);
    }
    return static_cast<T>(value);
}

std::vector<std::byte> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        return {};
    const auto length = input.tellg();
    if (length < 0)
        throw Error(ErrorCode::IoError, "failed to determine event-log size");
    std::vector<std::byte> data(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!data.empty())
        input.read(reinterpret_cast<char*>(data.data()), length);
    if (!input && !data.empty())
        throw Error(ErrorCode::IoError, "failed to read event log");
    return data;
}

std::size_t find_magic(const std::vector<std::byte>& data, std::size_t offset) {
    const std::array<std::byte, 4> bytes{
        static_cast<std::byte>(magic & 0xffU),
        static_cast<std::byte>((magic >> 8U) & 0xffU),
        static_cast<std::byte>((magic >> 16U) & 0xffU),
        static_cast<std::byte>((magic >> 24U) & 0xffU),
    };
    for (std::size_t index = offset; index + bytes.size() <= data.size(); ++index) {
        if (std::equal(bytes.begin(), bytes.end(),
                       data.begin() + static_cast<std::ptrdiff_t>(index))) {
            return index;
        }
    }
    return data.size();
}

} // namespace

EventLog::EventLog(std::filesystem::path path) : path_(std::move(path)) {
    if (path_.has_parent_path())
        std::filesystem::create_directories(path_.parent_path());
    if (!std::filesystem::exists(path_)) {
        std::ofstream create(path_, std::ios::binary);
        if (!create)
            throw Error(ErrorCode::IoError, "failed to create event log");
    }
    const ReplayResult state = replay_unlocked();
    for (const Event& event : state.events)
        next_id_ = std::max(next_id_, event.id + 1);
}

std::uint32_t EventLog::checksum(const std::byte* data, std::size_t size) {
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= std::to_integer<std::uint8_t>(data[index]);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                static_cast<std::uint32_t>(-static_cast<std::int32_t>(crc & 1U));
            crc = (crc >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

std::vector<std::byte> EventLog::serialize(const Event& event) {
    if (event.payload.size() > maximum_record_size - minimum_record_size) {
        throw Error(ErrorCode::InvalidArgument, "event payload exceeds the record limit");
    }
    const auto record_size = static_cast<std::uint32_t>(minimum_record_size + event.payload.size());
    std::vector<std::byte> output;
    output.reserve(record_size);
    append_little_endian(output, magic);
    append_little_endian(output, event.schema_version);
    append_little_endian(output, static_cast<std::uint16_t>(event.type));
    append_little_endian(output, record_size);
    append_little_endian(output, event.id);
    append_little_endian(output, event.timestamp_ms);
    append_little_endian(output, static_cast<std::uint32_t>(event.payload.size()));
    for (const char character : event.payload)
        output.push_back(static_cast<std::byte>(character));
    append_little_endian(output, checksum(output.data(), output.size()));
    return output;
}

Event EventLog::append(EventType type, std::string payload,
                       std::chrono::system_clock::time_point timestamp) {
    std::lock_guard lock(mutex_);
    Event event{
        current_schema, type, next_id_,
        std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count(),
        std::move(payload)};
    const std::vector<std::byte> record = serialize(event);
    const int descriptor = open(path_.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0600);
    if (descriptor < 0) {
        throw Error(ErrorCode::IoError,
                    std::string("failed to open event log: ") + std::strerror(errno));
    }
    std::size_t offset = 0;
    while (offset < record.size()) {
        const ssize_t written = write(descriptor, record.data() + offset, record.size() - offset);
        if (written < 0) {
            const int error = errno;
            close(descriptor);
            throw Error(ErrorCode::IoError,
                        std::string("failed to append event: ") + std::strerror(error));
        }
        offset += static_cast<std::size_t>(written);
    }
    if (fsync(descriptor) != 0) {
        const int error = errno;
        close(descriptor);
        throw Error(ErrorCode::IoError,
                    std::string("failed to sync event log: ") + std::strerror(error));
    }
    close(descriptor);
    ++next_id_;
    return event;
}

ReplayResult EventLog::replay() const {
    std::lock_guard lock(mutex_);
    return replay_unlocked();
}

ReplayResult EventLog::replay_unlocked() const {
    const std::vector<std::byte> data = read_file(path_);
    ReplayResult result;
    std::size_t offset = 0;
    bool prefix_intact = true;
    while (offset < data.size()) {
        if (data.size() - offset < minimum_record_size) {
            result.truncated_tail = true;
            break;
        }
        if (read_little_endian<std::uint32_t>(data.data() + offset) != magic) {
            result.corruptions.push_back(Corruption{offset, "record magic does not match"});
            prefix_intact = false;
            offset = find_magic(data, offset + 1);
            continue;
        }
        const std::uint16_t schema = read_little_endian<std::uint16_t>(data.data() + offset + 4);
        const auto type = read_little_endian<std::uint16_t>(data.data() + offset + 6);
        const std::uint32_t record_size =
            read_little_endian<std::uint32_t>(data.data() + offset + 8);
        const std::uint32_t payload_size =
            read_little_endian<std::uint32_t>(data.data() + offset + 28);
        if (record_size < minimum_record_size || record_size > maximum_record_size ||
            payload_size != record_size - minimum_record_size) {
            result.corruptions.push_back(Corruption{offset, "record length is invalid"});
            prefix_intact = false;
            offset = find_magic(data, offset + 1);
            continue;
        }
        if (data.size() - offset < record_size) {
            result.truncated_tail = true;
            break;
        }
        const std::uint32_t expected =
            read_little_endian<std::uint32_t>(data.data() + offset + record_size - checksum_size);
        const std::uint32_t actual = checksum(data.data() + offset, record_size - checksum_size);
        if (actual != expected) {
            result.corruptions.push_back(Corruption{offset, "record checksum does not match"});
            prefix_intact = false;
            offset += record_size;
            continue;
        }
        if (schema != current_schema) {
            result.corruptions.push_back(Corruption{offset, "record schema is unsupported"});
            prefix_intact = false;
            offset += record_size;
            continue;
        }
        Event event;
        event.schema_version = schema;
        event.type = static_cast<EventType>(type);
        event.id = read_little_endian<std::uint64_t>(data.data() + offset + 12);
        event.timestamp_ms = read_little_endian<std::int64_t>(data.data() + offset + 20);
        event.payload.assign(reinterpret_cast<const char*>(data.data() + offset + header_size),
                             payload_size);
        result.events.push_back(std::move(event));
        offset += record_size;
        if (prefix_intact)
            result.valid_prefix_bytes = offset;
    }
    return result;
}

bool EventLog::recover_trailing_record() {
    std::lock_guard lock(mutex_);
    const ReplayResult result = replay_unlocked();
    if (!result.truncated_tail || !result.corruptions.empty())
        return false;
    std::filesystem::resize_file(path_, result.valid_prefix_bytes);
    return true;
}

std::string_view name(EventType type) {
    switch (type) {
    case EventType::GameImported:
        return "GameImported";
    case EventType::GameParsed:
        return "GameParsed";
    case EventType::PositionAnalyzed:
        return "PositionAnalyzed";
    case EventType::MistakeDetected:
        return "MistakeDetected";
    case EventType::MistakeClassified:
        return "MistakeClassified";
    case EventType::ExplanationCreated:
        return "ExplanationCreated";
    case EventType::AnalysisCompleted:
        return "AnalysisCompleted";
    }
    return "Unknown";
}

} // namespace pct::storage
