#include "test.hpp"

#include "pct/app/repository.hpp"
#include "pct/storage/event_log.hpp"
#include "pct/training/training.hpp"

#include <filesystem>
#include <memory>
#include <unistd.h>

using namespace pct;

namespace {

class CorpusVerifier final : public engine::AnalysisEngine {
  public:
    explicit CorpusVerifier(bool ambiguous = false) : ambiguous_(ambiguous) {}

    engine::AnalysisResult analyze(const engine::AnalysisRequest& request,
                                   CancellationToken) override {
        const std::string solution = "e1a5";
        const int alternative = ambiguous_ ? 280 : 100;
        return {{{1, request.depth, 300, std::nullopt, 1, 1, {solution}},
                 {2, request.depth, alternative, std::nullopt, 1, 1, {"e1e2"}}},
                solution,
                {}};
    }

  private:
    bool ambiguous_{false};
};

training::Profile recurring_profile() {
    training::Profile profile;
    profile.latest_rating = 1000;
    training::Weakness weakness;
    weakness.category = "Discovered attack";
    weakness.occurrences = 3;
    profile.weaknesses.push_back(weakness);
    return profile;
}

} // namespace

TEST_CASE("tactical corpus records provenance and presents the solution position") {
    const auto corpus = training::TacticalCorpus::load(PCT_TACTICAL_CORPUS_PATH);
    CHECK_EQ(corpus.manifest().license, "Creative Commons CC0");
    CHECK(!corpus.manifest().download_url.empty());
    const auto matches = corpus.match(recurring_profile(), 5);
    CHECK_EQ(matches.size(), 1ULL);
    CHECK_EQ(matches.front().solution, "e1a5");
    chess::Board board = chess::Board::from_fen(matches.front().fen);
    CHECK(board.find_legal_move(chess::parse_square("e1"), chess::parse_square("a5")).has_value());
}

TEST_CASE("advanced corpus drills require two stable independent validations") {
    const auto corpus = training::TacticalCorpus::load(PCT_TACTICAL_CORPUS_PATH);
    training::AdvancedDrillGenerator accepted(
        corpus, [] { return std::make_unique<CorpusVerifier>(); });
    const auto generated = accepted.generate(recurring_profile(), {}, 3);
    CHECK_EQ(generated.size(), 1ULL);
    CHECK_EQ(generated.front().source_type, "public_corpus");
    CHECK_EQ(generated.front().validation_evidence.size(), 4ULL);

    training::AdvancedDrillGenerator ambiguous(
        corpus, [] { return std::make_unique<CorpusVerifier>(true); });
    CHECK(ambiguous.generate(recurring_profile(), {}, 3).empty());
}

TEST_CASE("validated corpus drill provenance survives event replay") {
    const auto path = std::filesystem::temp_directory_path() /
                      ("pct-corpus-" + std::to_string(::getpid()) + ".log");
    std::filesystem::remove(path);
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        training::AdvancedDrillGenerator generator(
            training::TacticalCorpus::load(PCT_TACTICAL_CORPUS_PATH),
            [] { return std::make_unique<CorpusVerifier>(); });
        auto generated = generator.generate(recurring_profile(), {}, 1);
        CHECK_EQ(generated.size(), 1ULL);
        if (generated.empty())
            return;
        auto drill = generated.front();
        CHECK(repository.add_validated_drill(std::move(drill)));
    }
    {
        storage::EventLog log(path);
        app::Repository repository(log);
        const auto drills = repository.drills(0);
        CHECK_EQ(drills.size(), 1ULL);
        CHECK_EQ(drills.front().source_type, "public_corpus");
        CHECK(!drills.front().provenance.empty());
        CHECK_EQ(drills.front().validation_evidence.size(), 4ULL);
    }
    std::filesystem::remove(path);
}
