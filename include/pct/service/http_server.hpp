#pragma once

#include "pct/app/job_manager.hpp"
#include "pct/app/repository.hpp"
#include "pct/import/import_service.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace pct::service {

struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct Response {
    int status{200};
    std::map<std::string, std::string> headers;
    std::string body;
};

class Api {
  public:
    Api(import::ImportService& importer, app::Repository& repository, app::JobManager& jobs)
        : importer_(importer), repository_(repository), jobs_(jobs) {}

    [[nodiscard]] Response handle(const Request& request);

  private:
    import::ImportService& importer_;
    app::Repository& repository_;
    app::JobManager& jobs_;
};

struct ServerOptions {
    std::uint16_t port{8787};
    std::filesystem::path web_root{"web/dist"};
};

class HttpServer {
  public:
    HttpServer(Api& api, app::JobManager& jobs, ServerOptions options = {});
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void run();
    void stop() noexcept;
    void broadcast(std::string_view message);

  private:
    Api& api_;
    app::JobManager& jobs_;
    ServerOptions options_;
    std::atomic<bool> stopped_{false};
    int listen_fd_{-1};
    std::mutex clients_mutex_;
    std::vector<int> websocket_clients_;

    void handle_client(int client_fd);
    void handle_websocket(int client_fd, const Request& request);
    [[nodiscard]] Response static_file(std::string_view path) const;
};

} // namespace pct::service
