#include "pct/service/http_server.hpp"

#include "pct/common/error.hpp"
#include "pct/common/json.hpp"
#include "pct/common/log.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <thread>

namespace pct::service {
namespace {

constexpr std::size_t max_header_size = 64 * 1024;
constexpr std::size_t max_body_size = 10 * 1024 * 1024;

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::vector<std::string> path_parts(std::string_view path) {
    const std::size_t query = path.find('?');
    path = path.substr(0, query);
    std::vector<std::string> result;
    std::size_t offset = 0;
    while (offset < path.size()) {
        while (offset < path.size() && path[offset] == '/')
            ++offset;
        if (offset >= path.size())
            break;
        const std::size_t end = path.find('/', offset);
        result.emplace_back(path.substr(offset, end - offset));
        if (end == std::string_view::npos)
            break;
        offset = end + 1;
    }
    return result;
}

Response json_response(int status, json::Value value) {
    return Response{
        status, {{"Content-Type", "application/json; charset=utf-8"}}, json::dump(value)};
}

Response error_response(int status, std::string message) {
    return json_response(status, json::Value::Object{{"error", std::move(message)}});
}

int status_for(ErrorCode code) {
    switch (code) {
    case ErrorCode::InvalidArgument:
    case ErrorCode::ParseError:
    case ErrorCode::IllegalMove:
    case ErrorCode::Unsupported:
        return 400;
    case ErrorCode::NotFound:
        return 404;
    case ErrorCode::NetworkError:
        return 502;
    case ErrorCode::Timeout:
        return 504;
    case ErrorCode::EngineError:
    case ErrorCode::IoError:
    case ErrorCode::Corruption:
        return 500;
    }
    return 500;
}

std::uint64_t parse_id(std::string_view value) {
    std::uint64_t result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw Error(ErrorCode::InvalidArgument, "identifier must be numeric");
    }
    return result;
}

std::string reason_phrase(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 202:
        return "Accepted";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 413:
        return "Content Too Large";
    case 500:
        return "Internal Server Error";
    case 502:
        return "Bad Gateway";
    case 504:
        return "Gateway Timeout";
    default:
        return "Error";
    }
}

bool send_all(int fd, const char* data, std::size_t size) {
    std::size_t offset = 0;
    while (offset < size) {
        const ssize_t count = send(fd, data + offset, size - offset, MSG_NOSIGNAL);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (count == 0)
            return false;
        offset += static_cast<std::size_t>(count);
    }
    return true;
}

std::optional<Request> read_request(int fd) {
    std::string data;
    std::array<char, 8192> buffer{};
    std::size_t header_end = std::string::npos;
    while ((header_end = data.find("\r\n\r\n")) == std::string::npos) {
        const ssize_t count = recv(fd, buffer.data(), buffer.size(), 0);
        if (count <= 0)
            return std::nullopt;
        data.append(buffer.data(), static_cast<std::size_t>(count));
        if (data.size() > max_header_size)
            throw Error(ErrorCode::InvalidArgument, "HTTP headers are too large");
    }
    std::istringstream head(data.substr(0, header_end));
    Request request;
    std::string version;
    if (!(head >> request.method >> request.path >> version) || !version.starts_with("HTTP/1.")) {
        throw Error(ErrorCode::InvalidArgument, "invalid HTTP request line");
    }
    std::string line;
    std::getline(head, line);
    while (std::getline(head, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        std::string key = lowercase(line.substr(0, colon));
        std::size_t start = colon + 1;
        while (start < line.size() && line[start] == ' ')
            ++start;
        request.headers.insert_or_assign(std::move(key), line.substr(start));
    }
    std::size_t content_length = 0;
    if (const auto found = request.headers.find("content-length"); found != request.headers.end()) {
        content_length = static_cast<std::size_t>(parse_id(found->second));
        if (content_length > max_body_size)
            throw Error(ErrorCode::InvalidArgument, "request body is too large");
    }
    request.body = data.substr(header_end + 4);
    while (request.body.size() < content_length) {
        const ssize_t count = recv(fd, buffer.data(), buffer.size(), 0);
        if (count <= 0)
            throw Error(ErrorCode::InvalidArgument, "request body ended early");
        request.body.append(buffer.data(), static_cast<std::size_t>(count));
    }
    if (request.body.size() > content_length)
        request.body.resize(content_length);
    return request;
}

std::array<std::uint32_t, 5> sha1(std::string_view input) {
    std::vector<std::uint8_t> data(input.begin(), input.end());
    const std::uint64_t bit_length = static_cast<std::uint64_t>(data.size()) * 8U;
    data.push_back(0x80U);
    while (data.size() % 64 != 56)
        data.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8) {
        data.push_back(
            static_cast<std::uint8_t>((bit_length >> static_cast<unsigned>(shift)) & 0xffU));
    }
    std::array<std::uint32_t, 5> hash{0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U,
                                      0xc3d2e1f0U};
    for (std::size_t chunk = 0; chunk < data.size(); chunk += 64) {
        std::array<std::uint32_t, 80> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            const std::size_t offset = chunk + index * 4;
            words[index] = (static_cast<std::uint32_t>(data[offset]) << 24U) |
                           (static_cast<std::uint32_t>(data[offset + 1]) << 16U) |
                           (static_cast<std::uint32_t>(data[offset + 2]) << 8U) |
                           static_cast<std::uint32_t>(data[offset + 3]);
        }
        for (std::size_t index = 16; index < 80; ++index) {
            words[index] = std::rotl(
                words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
        }
        auto [a, b, c, d, e] = hash;
        for (std::size_t index = 0; index < 80; ++index) {
            std::uint32_t function = 0;
            std::uint32_t constant = 0;
            if (index < 20) {
                function = (b & c) | ((~b) & d);
                constant = 0x5a827999U;
            } else if (index < 40) {
                function = b ^ c ^ d;
                constant = 0x6ed9eba1U;
            } else if (index < 60) {
                function = (b & c) | (b & d) | (c & d);
                constant = 0x8f1bbcdcU;
            } else {
                function = b ^ c ^ d;
                constant = 0xca62c1d6U;
            }
            const std::uint32_t temp = std::rotl(a, 5) + function + e + constant + words[index];
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = temp;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
    }
    return hash;
}

std::string websocket_accept(std::string_view key) {
    const auto hash = sha1(std::string(key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    std::array<std::uint8_t, 20> bytes{};
    for (std::size_t index = 0; index < hash.size(); ++index) {
        bytes[index * 4] = static_cast<std::uint8_t>(hash[index] >> 24U);
        bytes[index * 4 + 1] = static_cast<std::uint8_t>(hash[index] >> 16U);
        bytes[index * 4 + 2] = static_cast<std::uint8_t>(hash[index] >> 8U);
        bytes[index * 4 + 3] = static_cast<std::uint8_t>(hash[index]);
    }
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    for (std::size_t index = 0; index < bytes.size(); index += 3) {
        const std::uint32_t value =
            (static_cast<std::uint32_t>(bytes[index]) << 16U) |
            (static_cast<std::uint32_t>(index + 1 < bytes.size() ? bytes[index + 1] : 0) << 8U) |
            static_cast<std::uint32_t>(index + 2 < bytes.size() ? bytes[index + 2] : 0);
        output.push_back(alphabet[(value >> 18U) & 63U]);
        output.push_back(alphabet[(value >> 12U) & 63U]);
        output.push_back(index + 1 < bytes.size() ? alphabet[(value >> 6U) & 63U] : '=');
        output.push_back(index + 2 < bytes.size() ? alphabet[value & 63U] : '=');
    }
    return output;
}

std::string websocket_frame(std::string_view message) {
    std::string frame;
    frame.push_back(static_cast<char>(0x81));
    if (message.size() < 126) {
        frame.push_back(static_cast<char>(message.size()));
    } else if (message.size() <= 65535) {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((message.size() >> 8U) & 0xffU));
        frame.push_back(static_cast<char>(message.size() & 0xffU));
    } else {
        frame.push_back(static_cast<char>(127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(
                static_cast<char>((message.size() >> static_cast<unsigned>(shift)) & 0xffU));
        }
    }
    frame.append(message);
    return frame;
}

std::string mime_type(const std::filesystem::path& path) {
    const std::string extension = lowercase(path.extension().string());
    if (extension == ".html")
        return "text/html; charset=utf-8";
    if (extension == ".js")
        return "text/javascript; charset=utf-8";
    if (extension == ".css")
        return "text/css; charset=utf-8";
    if (extension == ".json")
        return "application/json; charset=utf-8";
    if (extension == ".svg")
        return "image/svg+xml";
    if (extension == ".png")
        return "image/png";
    return "application/octet-stream";
}

} // namespace

Response Api::handle(const Request& request) {
    try {
        const auto parts = path_parts(request.path);
        if (request.method == "GET" && parts == std::vector<std::string>{"api", "health"}) {
            return json_response(200, json::Value::Object{
                                          {"status", "ok"},
                                          {"version", "0.1.0"},
                                          {"local_only", true},
                                          {"games", repository_.size()},
                                      });
        }
        if (parts == std::vector<std::string>{"api", "games"} && request.method == "GET") {
            json::Value::Array games;
            for (const auto& game : repository_.list())
                games.push_back(to_json(game));
            return json_response(200, json::Value::Object{{"games", std::move(games)}});
        }
        if (parts == std::vector<std::string>{"api", "mistakes"} && request.method == "GET") {
            json::Value::Array mistakes;
            for (const auto& game : repository_.list()) {
                if (!game.analysis)
                    continue;
                const json::Value analysis = app::to_json(*game.analysis);
                for (const auto& mistake : analysis.at("mistakes").as_array()) {
                    mistakes.push_back(mistake);
                }
            }
            return json_response(200, json::Value::Object{{"mistakes", std::move(mistakes)}});
        }
        if (parts == std::vector<std::string>{"api", "settings"} && request.method == "GET") {
            return json_response(200, json::Value::Object{
                                          {"bind_address", "127.0.0.1"},
                                          {"shallow_depth", 10},
                                          {"deep_depth", 18},
                                          {"top_mistakes", 3},
                                          {"storage", "append_only_event_log"},
                                      });
        }
        if (parts == std::vector<std::string>{"api", "import"} && request.method == "POST") {
            const json::Value body = json::parse(request.body);
            import::ImportedGame imported;
            if (body.as_object().contains("url")) {
                imported = importer_.from_url(body.at("url").as_string());
            } else if (body.as_object().contains("pgn")) {
                imported = importer_.from_pgn(body.at("pgn").as_string());
            } else {
                throw Error(ErrorCode::InvalidArgument, "request requires url or pgn");
            }
            const app::AddResult added = repository_.add(imported);
            const app::AnalysisJob job = jobs_.start(imported.game.identity);
            return json_response(added == app::AddResult::Added ? 202 : 200,
                                 json::Value::Object{
                                     {"duplicate", added == app::AddResult::Duplicate},
                                     {"game_id", imported.game.identity},
                                     {"job", app::to_json(job)},
                                 });
        }
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "games") {
            const auto game = repository_.get(parts[2]);
            if (!game)
                throw Error(ErrorCode::NotFound, "game does not exist");
            if (parts.size() == 3 && request.method == "GET") {
                return json_response(200, to_json(*game, true));
            }
            if (parts.size() == 4 && parts[3] == "analysis" && request.method == "POST") {
                return json_response(202, app::to_json(jobs_.start(parts[2])));
            }
            if (parts.size() == 4 && parts[3] == "analysis" && request.method == "GET") {
                if (!game->analysis)
                    return json_response(202, json::Value::Object{{"status", "pending"}});
                return json_response(200, app::to_json(*game->analysis));
            }
            if (parts.size() == 5 && parts[3] == "moves" && request.method == "GET") {
                const std::size_t ply = static_cast<std::size_t>(parse_id(parts[4]));
                if (ply >= game->imported.game.plies.size()) {
                    throw Error(ErrorCode::NotFound, "move does not exist");
                }
                const auto& move = game->imported.game.plies[ply];
                return json_response(200, json::Value::Object{
                                              {"ply", ply},
                                              {"san", move.san},
                                              {"uci", chess::uci(move.move)},
                                              {"fen_before", move.fen_before},
                                              {"fen_after", move.fen_after},
                                          });
            }
        }
        if (parts == std::vector<std::string>{"api", "jobs"} && request.method == "GET") {
            json::Value::Array jobs;
            for (const auto& job : jobs_.list())
                jobs.push_back(app::to_json(job));
            return json_response(200, json::Value::Object{{"jobs", std::move(jobs)}});
        }
        if (parts.size() == 3 && parts[0] == "api" && parts[1] == "jobs") {
            const std::uint64_t id = parse_id(parts[2]);
            if (request.method == "GET") {
                const auto job = jobs_.get(id);
                if (!job)
                    throw Error(ErrorCode::NotFound, "job does not exist");
                return json_response(200, app::to_json(*job));
            }
            if (request.method == "DELETE") {
                if (!jobs_.cancel(id))
                    throw Error(ErrorCode::NotFound, "active job does not exist");
                return json_response(200, app::to_json(*jobs_.get(id)));
            }
        }
        return error_response(404, "route does not exist");
    } catch (const Error& error) {
        return error_response(status_for(error.code()), error.what());
    } catch (const std::exception& error) {
        log(LogLevel::Error, "api", error.what());
        return error_response(500, "internal server error");
    }
}

HttpServer::HttpServer(Api& api, app::JobManager& jobs, ServerOptions options)
    : api_(api), jobs_(jobs), options_(std::move(options)) {
    jobs_.set_observer([this](const app::AnalysisJob& job) {
        broadcast(
            json::dump(json::Value::Object{{"type", "job_update"}, {"job", app::to_json(job)}}));
    });
}

HttpServer::~HttpServer() {
    jobs_.set_observer({});
    stop();
}

void HttpServer::run() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0)
        throw Error(ErrorCode::IoError, "failed to create HTTP socket");
    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(options_.port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        const std::string message = std::strerror(errno);
        stop();
        throw Error(ErrorCode::IoError,
                    "failed to bind 127.0.0.1:" + std::to_string(options_.port) + ": " + message);
    }
    if (listen(listen_fd_, 32) != 0) {
        stop();
        throw Error(ErrorCode::IoError, "failed to listen on HTTP socket");
    }
    log(LogLevel::Info, "http", "listening on http://127.0.0.1:" + std::to_string(options_.port));
    while (!stopped_) {
        const int client = accept(listen_fd_, nullptr, nullptr);
        if (client < 0) {
            if (stopped_ || errno == EBADF || errno == EINVAL)
                break;
            if (errno == EINTR)
                continue;
            log(LogLevel::Warning, "http", std::string("accept failed: ") + std::strerror(errno));
            continue;
        }
        std::thread([this, client] { handle_client(client); }).detach();
    }
}

void HttpServer::stop() noexcept {
    stopped_ = true;
    if (listen_fd_ >= 0) {
        shutdown(listen_fd_, SHUT_RDWR);
        close(listen_fd_);
        listen_fd_ = -1;
    }
    std::lock_guard lock(clients_mutex_);
    for (const int client : websocket_clients_) {
        shutdown(client, SHUT_RDWR);
        close(client);
    }
    websocket_clients_.clear();
}

void HttpServer::handle_client(int client_fd) {
    try {
        const auto request = read_request(client_fd);
        if (!request) {
            close(client_fd);
            return;
        }
        const auto upgrade = request->headers.find("upgrade");
        if (request->path == "/ws" && upgrade != request->headers.end() &&
            lowercase(upgrade->second) == "websocket") {
            handle_websocket(client_fd, *request);
            return;
        }
        Response response =
            request->path.starts_with("/api/") ? api_.handle(*request) : static_file(request->path);
        response.headers.insert_or_assign("Content-Length", std::to_string(response.body.size()));
        response.headers.insert_or_assign("Connection", "close");
        response.headers.insert_or_assign("X-Content-Type-Options", "nosniff");
        response.headers.insert_or_assign(
            "Content-Security-Policy", "default-src 'self'; connect-src 'self' ws://127.0.0.1:*");
        std::ostringstream head;
        head << "HTTP/1.1 " << response.status << ' ' << reason_phrase(response.status) << "\r\n";
        for (const auto& [key, value] : response.headers)
            head << key << ": " << value << "\r\n";
        head << "\r\n";
        const std::string header = head.str();
        send_all(client_fd, header.data(), header.size());
        send_all(client_fd, response.body.data(), response.body.size());
    } catch (const std::exception& error) {
        const Response response = error_response(400, error.what());
        const std::string head =
            "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\nContent-Length: " +
            std::to_string(response.body.size()) + "\r\nConnection: close\r\n\r\n";
        send_all(client_fd, head.data(), head.size());
        send_all(client_fd, response.body.data(), response.body.size());
    }
    close(client_fd);
}

void HttpServer::handle_websocket(int client_fd, const Request& request) {
    const auto key = request.headers.find("sec-websocket-key");
    if (key == request.headers.end()) {
        close(client_fd);
        return;
    }
    const std::string accept = websocket_accept(key->second);
    const std::string response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                 "Connection: Upgrade\r\nSec-WebSocket-Accept: " +
                                 accept + "\r\n\r\n";
    if (!send_all(client_fd, response.data(), response.size())) {
        close(client_fd);
        return;
    }
    {
        std::lock_guard lock(clients_mutex_);
        websocket_clients_.push_back(client_fd);
    }
    json::Value::Array jobs;
    for (const auto& job : jobs_.list())
        jobs.push_back(app::to_json(job));
    const std::string snapshot = websocket_frame(
        json::dump(json::Value::Object{{"type", "jobs_snapshot"}, {"jobs", std::move(jobs)}}));
    if (!send_all(client_fd, snapshot.data(), snapshot.size())) {
        std::lock_guard lock(clients_mutex_);
        std::erase(websocket_clients_, client_fd);
        close(client_fd);
        return;
    }
    std::array<char, 1024> buffer{};
    while (!stopped_) {
        pollfd descriptor{client_fd, POLLIN, 0};
        const int ready = poll(&descriptor, 1, 1000);
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
            break;
        if (ready == 0)
            continue;
        const ssize_t count = recv(client_fd, buffer.data(), buffer.size(), 0);
        if (count <= 0)
            break;
        if ((static_cast<unsigned char>(buffer[0]) & 0x0fU) == 0x08U)
            break;
    }
    {
        std::lock_guard lock(clients_mutex_);
        std::erase(websocket_clients_, client_fd);
    }
    close(client_fd);
}

void HttpServer::broadcast(std::string_view message) {
    const std::string frame = websocket_frame(message);
    std::lock_guard lock(clients_mutex_);
    std::erase_if(websocket_clients_,
                  [&](int client) { return !send_all(client, frame.data(), frame.size()); });
}

Response HttpServer::static_file(std::string_view request_path) const {
    const std::size_t query = request_path.find('?');
    request_path = request_path.substr(0, query);
    if (request_path.find("..") != std::string_view::npos ||
        request_path.find('\\') != std::string_view::npos) {
        return error_response(400, "invalid static-file path");
    }
    const std::string relative =
        request_path == "/" ? "index.html" : std::string(request_path.substr(1));
    const std::filesystem::path path = options_.web_root / relative;
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        return error_response(404, "file does not exist");
    const auto length = input.tellg();
    std::string body(static_cast<std::size_t>(length), '\0');
    input.seekg(0);
    if (!body.empty())
        input.read(body.data(), length);
    return Response{200, {{"Content-Type", mime_type(path)}}, std::move(body)};
}

} // namespace pct::service
