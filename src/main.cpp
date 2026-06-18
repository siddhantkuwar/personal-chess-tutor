#include "pct/analysis/analyzer.hpp"
#include "pct/app/job_manager.hpp"
#include "pct/app/repository.hpp"
#include "pct/common/log.hpp"
#include "pct/engine/stockfish.hpp"
#include "pct/import/import_service.hpp"
#include "pct/service/http_server.hpp"
#include "pct/storage/event_log.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::filesystem::path data_dir{"data"};
    std::filesystem::path web_root{"web/dist"};
    std::string stockfish{"stockfish"};
    std::uint16_t port{8787};
};

Options parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        const auto value = [&]() -> std::string {
            if (++index >= argc)
                throw std::runtime_error("missing value for " + std::string(argument));
            return argv[index];
        };
        if (argument == "--data-dir")
            options.data_dir = value();
        else if (argument == "--web-root")
            options.web_root = value();
        else if (argument == "--stockfish")
            options.stockfish = value();
        else if (argument == "--port") {
            const unsigned long port = std::stoul(value());
            if (port == 0 || port > 65535)
                throw std::runtime_error("port is outside 1-65535");
            options.port = static_cast<std::uint16_t>(port);
        } else if (argument == "--help") {
            std::cout << "usage: personal-chess-tutor [--data-dir path] [--web-root path] "
                         "[--stockfish path] [--port number]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + std::string(argument));
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        std::filesystem::create_directories(options.data_dir);
        pct::storage::EventLog event_log(options.data_dir / "events.log");
        if (event_log.replay().truncated_tail) {
            if (event_log.recover_trailing_record()) {
                pct::log(pct::LogLevel::Warning, "storage", "recovered a partial trailing record");
            }
        }
        pct::app::Repository repository(event_log);
        pct::import::ImportService importer;
        pct::engine::Stockfish stockfish(pct::engine::StockfishOptions{options.stockfish, 128, 1});
        pct::analysis::AnalysisCache cache;
        pct::analysis::Analyzer analyzer(stockfish, cache);
        pct::app::JobManager jobs(repository, analyzer);
        pct::service::Api api(importer, repository, jobs);
        pct::service::HttpServer server(
            api, jobs, pct::service::ServerOptions{options.port, options.web_root});
        if (!std::filesystem::exists(options.web_root / "index.html")) {
            pct::log(pct::LogLevel::Warning, "http",
                     "frontend build is missing; run npm run build --prefix web");
        }
        server.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }
}
