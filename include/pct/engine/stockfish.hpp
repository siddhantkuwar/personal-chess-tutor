#pragma once

#include "pct/common/cancellation.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pct::engine {

enum class AnalysisPriority { Interactive, CurrentGame, Historical };

struct PrincipalVariation {
    int multipv{1};
    int depth{0};
    std::optional<int> centipawns;
    std::optional<int> mate;
    std::uint64_t nodes{0};
    std::uint64_t time_ms{0};
    std::vector<std::string> moves;
};

struct AnalysisRequest {
    std::string fen;
    int depth{14};
    std::chrono::milliseconds move_time{0};
    int multipv{1};
    std::chrono::milliseconds timeout{30000};
    AnalysisPriority priority{AnalysisPriority::CurrentGame};
};

struct AnalysisResult {
    std::vector<PrincipalVariation> lines;
    std::string best_move;
    std::string ponder_move;
};

struct StockfishOptions {
    std::string executable{"stockfish"};
    int hash_mb{128};
    int threads{1};
};

class AnalysisEngine {
  public:
    virtual ~AnalysisEngine() = default;
    [[nodiscard]] virtual AnalysisResult analyze(const AnalysisRequest& request,
                                                 CancellationToken stop_token = {}) = 0;
};

class Stockfish : public AnalysisEngine {
  public:
    explicit Stockfish(StockfishOptions options = {});
    ~Stockfish();

    Stockfish(const Stockfish&) = delete;
    Stockfish& operator=(const Stockfish&) = delete;
    Stockfish(Stockfish&&) = delete;
    Stockfish& operator=(Stockfish&&) = delete;

    void start();
    void stop() noexcept;
    void restart();
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] AnalysisResult analyze(const AnalysisRequest& request,
                                         CancellationToken stop_token = {}) override;

    [[nodiscard]] static std::optional<PrincipalVariation> parse_info_line(std::string_view line);

  private:
    StockfishOptions options_;
    int input_fd_{-1};
    int output_fd_{-1};
    int process_id_{-1};
    std::string read_buffer_;

    void send(std::string_view command);
    [[nodiscard]] std::string read_line(std::chrono::steady_clock::time_point deadline,
                                        CancellationToken stop_token = {});
    void wait_for(std::string_view token, std::chrono::milliseconds timeout);
};

} // namespace pct::engine
