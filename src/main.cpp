#include "pct/analysis/analyzer.hpp"
#include "pct/app/job_manager.hpp"
#include "pct/app/repository.hpp"
#include "pct/common/log.hpp"
#include "pct/engine/stockfish.hpp"
#include "pct/engine/pool.hpp"
#include "pct/import/import_service.hpp"
#include "pct/service/http_server.hpp"
#include "pct/storage/event_log.hpp"

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

struct Options {
    std::filesystem::path data_dir{"data"};
    std::filesystem::path web_root{"web/dist"};
    std::string stockfish{"stockfish"};
    std::uint16_t port{8787};
    std::size_t workers{std::min<std::size_t>(2, std::max(1U, std::thread::hardware_concurrency()))};
    std::size_t max_pending{256};
    std::size_t retry_limit{1};
    std::filesystem::path tactical_corpus{"resources/tactical-corpus.json"};
    bool tactical_corpus_enabled{true};
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
        } else if (argument == "--workers") {
            options.workers = std::stoul(value());
            if (options.workers == 0 || options.workers > 16)
                throw std::runtime_error("workers must be between 1 and 16");
        } else if (argument == "--max-pending") {
            options.max_pending = std::stoul(value());
            if (options.max_pending == 0 || options.max_pending > 10000)
                throw std::runtime_error("max-pending must be between 1 and 10000");
        } else if (argument == "--retry-limit") {
            options.retry_limit = std::stoul(value());
            if (options.retry_limit > 10)
                throw std::runtime_error("retry-limit must be at most 10");
        } else if (argument == "--tactical-corpus") {
            options.tactical_corpus = value();
        } else if (argument == "--no-tactical-corpus") {
            options.tactical_corpus_enabled = false;
        } else if (argument == "--help") {
            std::cout << "usage: personal-chess-tutor [--data-dir path] [--web-root path] "
                         "[--stockfish path] [--port number] [--workers 1-16] "
                         "[--max-pending count] [--retry-limit count] "
                         "[--tactical-corpus path | --no-tactical-corpus]\n";
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
        pct::engine::EnginePool engines(
            [&](std::size_t) {
                return std::make_unique<pct::engine::Stockfish>(
                    pct::engine::StockfishOptions{options.stockfish, 128, 1});
            },
            pct::engine::EnginePoolOptions{options.workers, options.max_pending,
                                           options.retry_limit});
        pct::analysis::AnalysisCache cache;
        pct::analysis::Analyzer analyzer(engines, cache);
        pct::app::JobManager jobs(
            repository, analyzer,
            pct::app::JobManagerOptions{options.workers, options.max_pending,
                                        options.retry_limit});
        std::unique_ptr<pct::training::AdvancedDrillGenerator> advanced_drills;
        if (options.tactical_corpus_enabled && std::filesystem::exists(options.tactical_corpus)) {
            advanced_drills = std::make_unique<pct::training::AdvancedDrillGenerator>(
                pct::training::TacticalCorpus::load(options.tactical_corpus),
                [&] {
                    return std::make_unique<pct::engine::Stockfish>(
                        pct::engine::StockfishOptions{options.stockfish, 64, 1});
                });
        }
        pct::service::Api api(importer, repository, jobs, [&] {
            const auto stats = engines.stats();
            const auto count = [](std::uint64_t value) {
                return static_cast<std::size_t>(value);
            };
            return pct::json::Value::Object{
                {"engine_workers", engines.worker_count()},
                {"engine_submitted", count(stats.submitted)},
                {"engine_completed", count(stats.completed)},
                {"engine_failed", count(stats.failed)},
                {"engine_retried", count(stats.retried)},
                {"engine_rejected", count(stats.rejected)},
                {"engine_active", count(stats.active)},
                {"queued_interactive", count(stats.queued_interactive)},
                {"queued_current_game", count(stats.queued_current_game)},
                {"queued_historical", count(stats.queued_historical)},
                {"maximum_queue_latency_ms", count(stats.maximum_queue_latency_ms)},
            };
        }, [&] {
            if (!advanced_drills)
                return std::vector<pct::training::Drill>{};
            return advanced_drills->generate(repository.profile(), repository.drills(0), 5);
        });
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
