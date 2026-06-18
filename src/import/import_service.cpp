#include "pct/import/import_service.hpp"

#include "pct/common/error.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <sstream>

namespace pct::import {
namespace {

constexpr std::size_t max_download_size = 10 * 1024 * 1024;

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string query_value(std::string_view query, std::string_view key) {
    std::size_t offset = 0;
    while (offset < query.size()) {
        const std::size_t end = query.find('&', offset);
        const std::string_view item = query.substr(offset, end - offset);
        const std::size_t equals = item.find('=');
        if (equals != std::string_view::npos && item.substr(0, equals) == key) {
            return std::string(item.substr(equals + 1));
        }
        if (end == std::string_view::npos)
            break;
        offset = end + 1;
    }
    return {};
}

std::optional<std::string> parse_json_string_at(std::string_view input, std::size_t quote) {
    if (quote >= input.size() || input[quote] != '"')
        return std::nullopt;
    std::string value;
    for (std::size_t index = quote + 1; index < input.size(); ++index) {
        const char character = input[index];
        if (character == '"')
            return value;
        if (character != '\\') {
            value.push_back(character);
            continue;
        }
        if (++index >= input.size())
            return std::nullopt;
        switch (input[index]) {
        case '"':
            value.push_back('"');
            break;
        case '\\':
            value.push_back('\\');
            break;
        case '/':
            value.push_back('/');
            break;
        case 'b':
            value.push_back('\b');
            break;
        case 'f':
            value.push_back('\f');
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        case 'u':
            if (index + 4 >= input.size())
                return std::nullopt;
            index += 4;
            value.push_back('?');
            break;
        default:
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_json_value(std::string_view input, std::string_view key,
                                           std::size_t start = 0, std::size_t limit = 0) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t end = limit == 0 ? input.size() : std::min(limit, input.size());
    std::size_t position = input.find(needle, start);
    while (position != std::string_view::npos && position < end) {
        const std::size_t colon = input.find(':', position + needle.size());
        if (colon == std::string_view::npos || colon >= end)
            return std::nullopt;
        const std::size_t quote = input.find('"', colon + 1);
        if (quote == std::string_view::npos || quote >= end)
            return std::nullopt;
        if (auto value = parse_json_string_at(input, quote))
            return value;
        position = input.find(needle, position + needle.size());
    }
    return std::nullopt;
}

std::string html_unescape(std::string value) {
    const std::pair<std::string_view, std::string_view> entities[] = {
        {"&quot;", "\""}, {"&#34;", "\""}, {"&amp;", "&"},
        {"&lt;", "<"},    {"&gt;", ">"},   {"&#39;", "'"},
    };
    for (const auto& [encoded, decoded] : entities) {
        std::size_t offset = 0;
        while ((offset = value.find(encoded, offset)) != std::string::npos) {
            value.replace(offset, encoded.size(), decoded);
            offset += decoded.size();
        }
    }
    return value;
}

std::string archive_pgn_for_game(std::string_view response, std::string_view game_id) {
    std::size_t object_start = 0;
    while ((object_start = response.find('{', object_start)) != std::string_view::npos) {
        const std::size_t object_end = response.find('}', object_start + 1);
        if (object_end == std::string_view::npos)
            break;
        const auto url = find_json_value(response, "url", object_start, object_end);
        if (url && (url->ends_with('/' + std::string(game_id)) ||
                    url->find("/" + std::string(game_id) + "/") != std::string::npos)) {
            if (const auto pgn = find_json_value(response, "pgn", object_start, object_end)) {
                return *pgn;
            }
        }
        object_start = object_end + 1;
    }
    throw Error(ErrorCode::NotFound, "game was not found in the Chess.com monthly archive");
}

std::string url_encode_component(std::string_view input) {
    std::ostringstream result;
    constexpr char hex[] = "0123456789ABCDEF";
    for (const char raw_character : input) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (std::isalnum(character) != 0 || character == '-' || character == '_' ||
            character == '.') {
            result << static_cast<char>(character);
        } else {
            result << '%' << hex[character >> 4U] << hex[character & 0x0FU];
        }
    }
    return result.str();
}

std::size_t write_callback(char* data, std::size_t size, std::size_t count, void* user_data) {
    const std::size_t bytes = size * count;
    auto& output = *static_cast<std::string*>(user_data);
    if (output.size() + bytes > max_download_size)
        return 0;
    output.append(data, bytes);
    return bytes;
}

} // namespace

ImportService::ImportService(HttpGet get) : get_(get ? std::move(get) : curl_get) {}

ChessComUrl ImportService::parse_chesscom_url(std::string_view input) {
    if (input.size() > 2048) {
        throw Error(ErrorCode::InvalidArgument, "Chess.com URL is too long");
    }
    constexpr std::string_view scheme = "https://";
    if (!input.starts_with(scheme)) {
        throw Error(ErrorCode::InvalidArgument, "Chess.com game URL must use HTTPS");
    }
    const std::size_t path_start = input.find('/', scheme.size());
    const std::string host = lowercase(std::string(input.substr(
        scheme.size(), path_start == std::string_view::npos ? input.size() - scheme.size()
                                                            : path_start - scheme.size())));
    if (host != "chess.com" && host != "www.chess.com") {
        throw Error(ErrorCode::InvalidArgument, "URL host must be chess.com");
    }
    if (path_start == std::string_view::npos) {
        throw Error(ErrorCode::InvalidArgument, "Chess.com URL has no game path");
    }
    const std::size_t query_start = input.find('?', path_start);
    const std::string_view path = input.substr(path_start, query_start - path_start);
    std::size_t game = path.find("/game/");
    if (game == std::string_view::npos)
        game = path.find("/analysis/game/");
    if (game == std::string_view::npos) {
        throw Error(ErrorCode::InvalidArgument, "URL is not a Chess.com game URL");
    }
    const std::size_t id_slash = path.find('/', game + 6);
    if (id_slash == std::string_view::npos || id_slash + 1 >= path.size()) {
        throw Error(ErrorCode::InvalidArgument, "Chess.com game URL has no game identifier");
    }
    const std::size_t id_end = path.find('/', id_slash + 1);
    const std::string id(path.substr(id_slash + 1, id_end - id_slash - 1));
    if (id.empty() || !std::all_of(id.begin(), id.end(),
                                   [](unsigned char value) { return std::isdigit(value) != 0; })) {
        throw Error(ErrorCode::InvalidArgument, "Chess.com game identifier must be numeric");
    }
    const std::string_view query =
        query_start == std::string_view::npos ? std::string_view{} : input.substr(query_start + 1);
    return ChessComUrl{"https://www.chess.com" + std::string(path), id,
                       query_value(query, "player"), query_value(query, "year"),
                       query_value(query, "month")};
}

std::string ImportService::extract_pgn(std::string_view response, std::string_view target_game_id) {
    if (!target_game_id.empty()) {
        try {
            return archive_pgn_for_game(response, target_game_id);
        } catch (const Error&) {
        }
    }
    if (const auto pgn = find_json_value(response, "pgn")) {
        if (pgn->find("[Event ") != std::string::npos)
            return *pgn;
    }
    std::string decoded = html_unescape(std::string(response));
    const std::size_t start = decoded.find("[Event ");
    if (start != std::string::npos) {
        const std::size_t script_end = decoded.find("</script>", start);
        const std::size_t textarea_end = decoded.find("</textarea>", start);
        const std::size_t end =
            std::min(script_end == std::string::npos ? decoded.size() : script_end,
                     textarea_end == std::string::npos ? decoded.size() : textarea_end);
        return decoded.substr(start, end - start);
    }
    throw Error(ErrorCode::ParseError, "the public game page did not contain an extractable PGN");
}

ImportedGame ImportService::from_url(std::string_view url) const {
    const ChessComUrl parsed = parse_chesscom_url(url);
    if (!parsed.player.empty() && parsed.year.size() == 4 && parsed.month.size() == 2) {
        const std::string endpoint = "https://api.chess.com/pub/player/" +
                                     url_encode_component(parsed.player) + "/games/" + parsed.year +
                                     "/" + parsed.month;
        try {
            const std::string pgn = extract_pgn(get_(endpoint), parsed.game_id);
            return ImportedGame{chess::parse_pgn(pgn), parsed.canonical, pgn,
                                ImportMethod::PublicApi};
        } catch (const Error&) {
        }
    }
    const std::string pgn = extract_pgn(get_(parsed.canonical));
    return ImportedGame{chess::parse_pgn(pgn), parsed.canonical, pgn, ImportMethod::PublicPage};
}

ImportedGame ImportService::from_pgn(std::string_view pgn, std::string_view source_url) const {
    if (pgn.size() > max_download_size) {
        throw Error(ErrorCode::InvalidArgument, "PGN exceeds the 10 MiB import limit");
    }
    return ImportedGame{chess::parse_pgn(pgn), std::string(source_url), std::string(pgn),
                        ImportMethod::ManualPgn};
}

std::string curl_get(const std::string& url) {
    static const int initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) {
        throw Error(ErrorCode::NetworkError, "failed to initialize libcurl");
    }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl)
        throw Error(ErrorCode::NetworkError, "failed to create HTTP client");
    std::string response;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, 15000L);
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "personal-chess-tutor/0.1 local-app");
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    const CURLcode result = curl_easy_perform(curl.get());
    if (result != CURLE_OK) {
        throw Error(ErrorCode::NetworkError,
                    std::string("Chess.com request failed: ") + curl_easy_strerror(result));
    }
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) {
        throw Error(ErrorCode::NetworkError, "Chess.com returned HTTP " + std::to_string(status));
    }
    return response;
}

} // namespace pct::import
