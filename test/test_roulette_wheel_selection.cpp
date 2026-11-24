#include <any>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "adef.h"
#include "Configuration.h"
#include "Individual.h"
#include "Parameters.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Repository.h"
#include "cm/cf/func/ConstantFunction.h"
#include "cm/cf/func/Function.h"
#include "cm/cf/func/NormalDisFunction.h"
#include "cm/cf/func/CauchyDisFunction.h"
#include "cm/cf/func/RouletteWheelSelectionFunction.h"

namespace {

bool require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        return false;
    }
    return true;
}

class TempFileGuard
{
public:
    explicit TempFileGuard(std::string filename) :
        filename_(std::move(filename))
    {
    }

    ~TempFileGuard()
    {
        std::remove(filename_.c_str());
    }

    TempFileGuard(const TempFileGuard&) = delete;
    TempFileGuard& operator=(const TempFileGuard&) = delete;

private:
    std::string filename_;
};

adef::Configuration load_json_config(const std::string& json_text,
                                     const std::string& filename)
{
    TempFileGuard guard(filename);
    {
        std::ofstream ofs(filename);
        ofs << json_text;
    }

    adef::Configuration config;
    if (!config.load_config(filename)) {
        throw std::runtime_error("Failed to load test configuration: " + filename);
    }
    return config;
}

class TestRepository : public adef::Repository
{
public:
    TestRepository()
    {
        parameters_ = std::make_shared<adef::Parameters>();
    }

    void set_random(std::shared_ptr<adef::Random> rng)
    {
        random_ = std::move(rng);
    }
};

// Deterministic word-sequence Random that falls back to mt19937 after a finite
// number of cycles so rejection-based standard distributions can never hang.
class RepeatingWordSequenceRandom : public adef::Random
{
public:
    RepeatingWordSequenceRandom(std::vector<result_type> pattern,
                                std::size_t max_pattern_cycles,
                                SeedType fallback_seed = 42u) :
        adef::Random(fallback_seed),
        pattern_(std::move(pattern)),
        remaining_cycles_(max_pattern_cycles),
        pos_(0),
        fallback_draws_(0)
    {
    }

    RepeatingWordSequenceRandom(const RepeatingWordSequenceRandom& rhs) :
        adef::Random(rhs),
        pattern_(rhs.pattern_),
        remaining_cycles_(rhs.remaining_cycles_),
        pos_(rhs.pos_),
        fallback_draws_(rhs.fallback_draws_)
    {
    }

    result_type next() override
    {
        if (!pattern_.empty() && remaining_cycles_ > 0) {
            const result_type val = pattern_[pos_++];
            if (pos_ == pattern_.size()) {
                pos_ = 0;
                --remaining_cycles_;
            }
            return val;
        }
        ++fallback_draws_;
        return adef::Random::next();
    }

    std::size_t fallback_draws() const noexcept { return fallback_draws_; }

protected:
    void seed_engine(SeedType seed_value) override
    {
        adef::Random::seed_engine(seed_value);
        pos_ = 0;
    }

private:
    std::vector<result_type> pattern_;
    std::size_t remaining_cycles_;
    std::size_t pos_;
    std::size_t fallback_draws_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<RepeatingWordSequenceRandom>(*this);
    }
};

std::shared_ptr<adef::Individual> make_individual(double fitness)
{
    auto ind = std::make_shared<adef::Individual>();
    ind->set_fitness_value(fitness);
    return ind;
}

std::shared_ptr<TestRepository> make_repos(adef::Random::SeedType seed)
{
    auto repos = std::make_shared<TestRepository>();
    repos->set_random(std::make_shared<adef::Random>(seed));
    return repos;
}

template<typename FunctionType>
bool expect_empty_generate_throws(std::shared_ptr<FunctionType> fn,
                                  const std::string& label)
{
    auto repos = make_repos(101u);
    fn->init(repos);

    bool threw = false;
    try {
        (void)fn->generate();
    }
    catch (const std::logic_error&) {
        threw = true;
    }
    return require(threw, label + ": expected std::logic_error on empty valued_objects_");
}

// ============================================================================
// 1. Empty-container guard across all four RouletteWheelSelectionFunction aliases
// ============================================================================
bool test_empty_container_throws(const adef::PrototypeManager& pm)
{
    // Default-constructed instances (no setup)
    if (!expect_empty_generate_throws(
            std::make_shared<adef::IntegerRouletteWheelSelectionFunction>(),
            "default IntegerRouletteWheelSelectionFunction")) {
        return false;
    }
    if (!expect_empty_generate_throws(
            std::make_shared<adef::RealRouletteWheelSelectionFunction>(),
            "default RealRouletteWheelSelectionFunction")) {
        return false;
    }
    if (!expect_empty_generate_throws(
            std::make_shared<adef::IntegerControlRouletteWheelSelectionFunction>(),
            "default IntegerControlRouletteWheelSelectionFunction")) {
        return false;
    }
    if (!expect_empty_generate_throws(
            std::make_shared<adef::RealControlRouletteWheelSelectionFunction>(),
            "default RealControlRouletteWheelSelectionFunction")) {
        return false;
    }

    // Explicitly configured with empty "object": []
    const std::vector<std::string> classnames = {
        "IntegerRouletteWheelSelectionFunction",
        "RealRouletteWheelSelectionFunction",
        "IntegerControlRouletteWheelSelectionFunction",
        "RealControlRouletteWheelSelectionFunction"
    };
    for (const auto& cls : classnames) {
        auto cfg = load_json_config(
            R"({"classname":")" + cls +
                R"(","scoring_function":{"classname":"SuccessScoringFunction"},"score_size":10,"object":[])" +
                "}",
            "test_rws_empty_" + cls + ".json");
        auto base = adef::make_and_setup_type<adef::BaseFunction>(cfg, pm);
        if (cls == "IntegerRouletteWheelSelectionFunction") {
            auto fn = std::dynamic_pointer_cast<adef::IntegerRouletteWheelSelectionFunction>(base);
            if (!require(fn != nullptr, "failed cast for " + cls) ||
                !expect_empty_generate_throws(fn, "configured empty " + cls)) {
                return false;
            }
        }
        else if (cls == "RealRouletteWheelSelectionFunction") {
            auto fn = std::dynamic_pointer_cast<adef::RealRouletteWheelSelectionFunction>(base);
            if (!require(fn != nullptr, "failed cast for " + cls) ||
                !expect_empty_generate_throws(fn, "configured empty " + cls)) {
                return false;
            }
        }
        else if (cls == "IntegerControlRouletteWheelSelectionFunction") {
            auto fn = std::dynamic_pointer_cast<adef::IntegerControlRouletteWheelSelectionFunction>(base);
            if (!require(fn != nullptr, "failed cast for " + cls) ||
                !expect_empty_generate_throws(fn, "configured empty " + cls)) {
                return false;
            }
        }
        else {
            auto fn = std::dynamic_pointer_cast<adef::RealControlRouletteWheelSelectionFunction>(base);
            if (!require(fn != nullptr, "failed cast for " + cls) ||
                !expect_empty_generate_throws(fn, "configured empty " + cls)) {
                return false;
            }
        }
    }

    return true;
}

// ============================================================================
// 2. Zero-score and non-positive-score uniform fallback (k=1, k=2, k=4)
// ============================================================================
bool test_zero_score_uniform_fallback(const adef::PrototypeManager& pm)
{
    // k = 1
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 10,
            "object": [7]
        })", "test_rws_zero_k1.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(11u));
        for (int i = 0; i < 50; ++i) {
            if (!require(fn->generate() == 7, "k=1 zero-score should always return sole candidate")) {
                return false;
            }
        }
    }

    // k = 2: initial zero scores, repeated batches without score mutation, and post-double-update reset
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 10,
            "object": [0, 1]
        })", "test_rws_zero_k2.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(2026u));

        // Run two consecutive batches to verify generate() does not mutate scores into a biased state.
        // N=2000, p=0.5 => mean=1000, stddev~=22.36; [850, 1150] is +/-6.7 sigma.
        constexpr int kDraws = 2000;
        for (int batch = 0; batch < 2; ++batch) {
            std::vector<int> counts(2, 0);
            for (int i = 0; i < kDraws; ++i) {
                const int picked = fn->generate();
                if (!require(picked == 0 || picked == 1, "k=2 zero-score returned unexpected candidate")) {
                    return false;
                }
                ++counts[static_cast<std::size_t>(picked)];
            }
            for (std::size_t idx = 0; idx < 2; ++idx) {
                if (!require(counts[idx] >= 850 && counts[idx] <= 1150,
                             "k=2 zero-score batch " + std::to_string(batch) +
                                 " candidate " + std::to_string(idx) +
                                 " count out of uniform range: " + std::to_string(counts[idx]))) {
                    return false;
                }
            }
        }

        // Train candidate 1 to score 1.0, then reset scores to 0.0 via two consecutive update() calls.
        auto parent = make_individual(10.0);
        auto better = make_individual(5.0);
        fn->record({ 1 }, parent, better);
        fn->update();
        fn->update();

        std::vector<int> post_reset_counts(2, 0);
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked == 0 || picked == 1, "k=2 post-reset returned unexpected candidate")) {
                return false;
            }
            ++post_reset_counts[static_cast<std::size_t>(picked)];
        }
        for (std::size_t idx = 0; idx < 2; ++idx) {
            if (!require(post_reset_counts[idx] >= 850 && post_reset_counts[idx] <= 1150,
                         "k=2 post-reset candidate " + std::to_string(idx) +
                             " count out of uniform range: " + std::to_string(post_reset_counts[idx]))) {
                return false;
            }
        }
    }

    // k = 4: initial zero scores (two consecutive batches) and post-double-update reset
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 10,
            "object": [0, 1, 2, 3]
        })", "test_rws_zero_k4.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(3030u));

        // N=4000, p=0.25 => mean=1000, stddev~=27.39; [820, 1180] is +/-6.57 sigma.
        constexpr int kDraws = 4000;
        for (int batch = 0; batch < 2; ++batch) {
            std::vector<int> counts(4, 0);
            for (int i = 0; i < kDraws; ++i) {
                const int picked = fn->generate();
                if (!require(picked >= 0 && picked < 4, "k=4 zero-score returned unexpected candidate")) {
                    return false;
                }
                ++counts[static_cast<std::size_t>(picked)];
            }
            for (std::size_t idx = 0; idx < 4; ++idx) {
                if (!require(counts[idx] >= 820 && counts[idx] <= 1180,
                             "k=4 zero-score batch " + std::to_string(batch) +
                                 " candidate " + std::to_string(idx) +
                                 " count out of uniform range: " + std::to_string(counts[idx]))) {
                    return false;
                }
            }
        }

        auto parent = make_individual(10.0);
        auto better = make_individual(5.0);
        fn->record({ 3 }, parent, better);
        fn->update();
        fn->update();

        std::vector<int> post_reset_counts(4, 0);
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked >= 0 && picked < 4, "k=4 post-reset returned unexpected candidate")) {
                return false;
            }
            ++post_reset_counts[static_cast<std::size_t>(picked)];
        }
        for (std::size_t idx = 0; idx < 4; ++idx) {
            if (!require(post_reset_counts[idx] >= 820 && post_reset_counts[idx] <= 1180,
                         "k=4 post-reset candidate " + std::to_string(idx) +
                             " count out of uniform range: " + std::to_string(post_reset_counts[idx]))) {
                return false;
            }
        }
    }

    // All-negative scores via ImprovedPercentageScoringFunction should also fall back to uniform selection
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "ImprovedPercentageScoringFunction" },
            "score_size": 10,
            "object": [0, 1, 2]
        })", "test_rws_all_negative.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(4040u));

        // parent - offspring < 0 for every candidate
        fn->record({ 0 }, make_individual(1.0), make_individual(5.0)); // -4.0
        fn->record({ 1 }, make_individual(1.0), make_individual(2.0)); // -1.0
        fn->record({ 2 }, make_individual(1.0), make_individual(1.0)); //  0.0
        fn->update();

        // N=3000, p=1/3 => mean=1000, stddev~=25.82; [830, 1170] is +/-6.58 sigma.
        constexpr int kDraws = 3000;
        std::vector<int> counts(3, 0);
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked >= 0 && picked < 3, "all-negative returned unexpected candidate")) {
                return false;
            }
            ++counts[static_cast<std::size_t>(picked)];
        }
        for (std::size_t idx = 0; idx < 3; ++idx) {
            if (!require(counts[idx] >= 830 && counts[idx] <= 1170,
                         "all-negative candidate " + std::to_string(idx) +
                             " count out of uniform range: " + std::to_string(counts[idx]))) {
                return false;
            }
        }
    }

    return true;
}

// ============================================================================
// 3. Weighted proportional selection without index-0 bias
// ============================================================================
bool test_weighted_proportional_selection(const adef::PrototypeManager& pm)
{
    // Unequal positive scores via SuccessScoringFunction: averages [0.25, 0.50, 0.75] => ratios 1:2:3
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 20,
            "object": [0, 1, 2]
        })", "test_rws_weighted_success.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(5050u));

        auto parent = make_individual(10.0);
        auto win = make_individual(5.0);
        auto loss = make_individual(15.0);

        // Candidate 0: 1/4 = 0.25
        fn->record({ 0 }, parent, win);
        fn->record({ 0 }, parent, loss);
        fn->record({ 0 }, parent, loss);
        fn->record({ 0 }, parent, loss);

        // Candidate 1: 2/4 = 0.50
        fn->record({ 1 }, parent, win);
        fn->record({ 1 }, parent, win);
        fn->record({ 1 }, parent, loss);
        fn->record({ 1 }, parent, loss);

        // Candidate 2: 3/4 = 0.75
        fn->record({ 2 }, parent, win);
        fn->record({ 2 }, parent, win);
        fn->record({ 2 }, parent, win);
        fn->record({ 2 }, parent, loss);

        fn->update();

        // N=6000 => expected [1000, 2000, 3000]; max stddev~=38.7; tolerance +/-240 (>=6.2 sigma).
        constexpr int kDraws = 6000;
        std::vector<int> counts(3, 0);
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked >= 0 && picked < 3, "weighted success returned unexpected candidate")) {
                return false;
            }
            ++counts[static_cast<std::size_t>(picked)];
        }
        if (!require(counts[0] >= 760 && counts[0] <= 1240 &&
                         counts[1] >= 1760 && counts[1] <= 2240 &&
                         counts[2] >= 2760 && counts[2] <= 3240,
                     "weighted success counts not proportional to 1:2:3 (" +
                         std::to_string(counts[0]) + ", " +
                         std::to_string(counts[1]) + ", " +
                         std::to_string(counts[2]) + ")")) {
            return false;
        }
    }

    // Small index-0 share [1.0, 9.0] via ImprovedPercentageScoringFunction => 10% vs 90%
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "ImprovedPercentageScoringFunction" },
            "score_size": 10,
            "object": [0, 1]
        })", "test_rws_weighted_1_9.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(6060u));

        fn->record({ 0 }, make_individual(1.0), make_individual(0.0));
        fn->record({ 1 }, make_individual(9.0), make_individual(0.0));
        fn->update();

        // N=5000, p0=0.1 => mean=500, stddev~=21.21; [360, 640] is +/-6.6 sigma.
        constexpr int kDraws = 5000;
        int count0 = 0;
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked == 0 || picked == 1, "weighted 1:9 returned unexpected candidate")) {
                return false;
            }
            if (picked == 0) {
                ++count0;
            }
        }
        if (!require(count0 >= 360 && count0 <= 640,
                     "weighted 1:9 index-0 count out of range: " + std::to_string(count0))) {
            return false;
        }
    }

    // Equal positive scores [1.0, 1.0, 1.0] => 1/3 each (no index-0 bias)
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 10,
            "object": [0, 1, 2]
        })", "test_rws_equal_positive.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(7070u));

        auto parent = make_individual(10.0);
        auto win = make_individual(5.0);
        fn->record({ 0 }, parent, win);
        fn->record({ 1 }, parent, win);
        fn->record({ 2 }, parent, win);
        fn->update();

        constexpr int kDraws = 3000;
        std::vector<int> counts(3, 0);
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked >= 0 && picked < 3, "equal positive returned unexpected candidate")) {
                return false;
            }
            ++counts[static_cast<std::size_t>(picked)];
        }
        for (std::size_t idx = 0; idx < 3; ++idx) {
            if (!require(counts[idx] >= 830 && counts[idx] <= 1170,
                         "equal positive candidate " + std::to_string(idx) +
                             " count out of range: " + std::to_string(counts[idx]))) {
                return false;
            }
        }
    }

    return true;
}

// ============================================================================
// 4. Mixed zero / negative and positive scores
// ============================================================================
bool test_mixed_zero_and_positive_scores(const adef::PrototypeManager& pm)
{
    // [0.0, 1.0] => index 0 never selected
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 10,
            "object": [0, 1]
        })", "test_rws_mixed_0_1.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(8080u));

        fn->record({ 0 }, make_individual(10.0), make_individual(15.0)); // 0
        fn->record({ 1 }, make_individual(10.0), make_individual(5.0));  // 1
        fn->update();

        for (int i = 0; i < 1000; ++i) {
            if (!require(fn->generate() == 1, "mixed [0, 1] must never select zero-score index 0")) {
                return false;
            }
        }
    }

    // [1.0, 0.0] => trailing zero-score candidate never selected
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 10,
            "object": [0, 1]
        })", "test_rws_mixed_1_0.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(8081u));

        fn->record({ 0 }, make_individual(10.0), make_individual(5.0));  // 1
        fn->record({ 1 }, make_individual(10.0), make_individual(15.0)); // 0
        fn->update();

        for (int i = 0; i < 1000; ++i) {
            if (!require(fn->generate() == 0, "mixed [1, 0] must never select trailing zero-score index 1")) {
                return false;
            }
        }
    }

    // Interleaved [0.0, 1.0, 0.0, 3.0, 0.0] => only indices 1 (25%) and 3 (75%)
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "ImprovedPercentageScoringFunction" },
            "score_size": 10,
            "object": [0, 1, 2, 3, 4]
        })", "test_rws_mixed_interleaved.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(8082u));

        fn->record({ 1 }, make_individual(1.0), make_individual(0.0));
        fn->record({ 3 }, make_individual(3.0), make_individual(0.0));
        fn->update();

        // N=4000 => expected [0, 1000, 0, 3000, 0]; stddev~=27.39; +/-180 is +/-6.57 sigma.
        constexpr int kDraws = 4000;
        std::vector<int> counts(5, 0);
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked >= 0 && picked < 5, "mixed interleaved returned unexpected candidate")) {
                return false;
            }
            ++counts[static_cast<std::size_t>(picked)];
        }
        if (!require(counts[0] == 0 && counts[2] == 0 && counts[4] == 0,
                     "mixed interleaved selected zero-score candidate")) {
            return false;
        }
        if (!require(counts[1] >= 820 && counts[1] <= 1180 &&
                         counts[3] >= 2820 && counts[3] <= 3180,
                     "mixed interleaved counts out of 25%/75% range (" +
                         std::to_string(counts[1]) + ", " + std::to_string(counts[3]) + ")")) {
            return false;
        }
    }

    // Mixed negative and positive scores [-5.0, 1.0, 3.0] => index 0 never selected; 1:3 ratio
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "ImprovedPercentageScoringFunction" },
            "score_size": 10,
            "object": [0, 1, 2]
        })", "test_rws_mixed_neg_pos.json");
        auto fn = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(8083u));

        fn->record({ 0 }, make_individual(0.0), make_individual(5.0)); // -5.0
        fn->record({ 1 }, make_individual(1.0), make_individual(0.0)); // +1.0
        fn->record({ 2 }, make_individual(3.0), make_individual(0.0)); // +3.0
        fn->update();

        constexpr int kDraws = 4000;
        std::vector<int> counts(3, 0);
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked >= 0 && picked < 3, "mixed neg/pos returned unexpected candidate")) {
                return false;
            }
            ++counts[static_cast<std::size_t>(picked)];
        }
        if (!require(counts[0] == 0, "mixed neg/pos must never select negative-score candidate")) {
            return false;
        }
        if (!require(counts[1] >= 820 && counts[1] <= 1180 &&
                         counts[2] >= 2820 && counts[2] <= 3180,
                     "mixed neg/pos positive counts out of 25%/75% range")) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 5. Small positive weights (below epsilon), extreme scales, and boundary safety
// ============================================================================
bool test_small_weights_and_boundary_safety(const adef::PrototypeManager& pm)
{
    auto make_imp_rws = [&](const std::string& objects_json, const std::string& tag) {
        auto cfg = load_json_config(
            R"({"classname":"IntegerRouletteWheelSelectionFunction",)"
            R"("scoring_function":{"classname":"ImprovedPercentageScoringFunction"},)"
            R"("score_size":10,"object":)" + objects_json + "}",
            "test_rws_" + tag + ".json");
        return adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);
    };

    // 5a. Equal tiny positive scores well below double::epsilon() (1e-20 and 1e-300)
    const std::vector<double> tiny_scales = { 1e-20, 1e-300 };
    for (std::size_t s = 0; s < tiny_scales.size(); ++s) {
        const double scale = tiny_scales[s];
        auto fn = make_imp_rws("[0, 1]", "tiny_eq_" + std::to_string(s));
        fn->init(make_repos(9000u + static_cast<adef::Random::SeedType>(s)));

        fn->record({ 0 }, make_individual(scale), make_individual(0.0));
        fn->record({ 1 }, make_individual(scale), make_individual(0.0));
        fn->update();

        constexpr int kDraws = 2000;
        int count0 = 0;
        for (int i = 0; i < kDraws; ++i) {
            if (fn->generate() == 0) {
                ++count0;
            }
        }
        if (!require(count0 >= 850 && count0 <= 1150,
                     "equal tiny weights (scale index " + std::to_string(s) +
                         ") exhibited bias: count0=" + std::to_string(count0))) {
            return false;
        }
    }

    // 5b. Unequal tiny positive scores well below double::epsilon() ([1*scale, 3*scale]) => 25% / 75%
    for (std::size_t s = 0; s < tiny_scales.size(); ++s) {
        const double scale = tiny_scales[s];
        auto fn = make_imp_rws("[0, 1]", "tiny_uneq_" + std::to_string(s));
        fn->init(make_repos(9100u + static_cast<adef::Random::SeedType>(s)));

        fn->record({ 0 }, make_individual(scale), make_individual(0.0));
        fn->record({ 1 }, make_individual(3.0 * scale), make_individual(0.0));
        fn->update();

        constexpr int kDraws = 4000;
        int count0 = 0;
        for (int i = 0; i < kDraws; ++i) {
            if (fn->generate() == 0) {
                ++count0;
            }
        }
        if (!require(count0 >= 820 && count0 <= 1180,
                     "unequal tiny weights 1:3 (scale index " + std::to_string(s) +
                         ") count0 out of 25% range: " + std::to_string(count0))) {
            return false;
        }
    }

    // 5c. Mixed zero and tiny positive scores: [0.0, 1e-20, 3e-20, 0.0]
    {
        auto fn = make_imp_rws("[0, 1, 2, 3]", "tiny_mixed");
        fn->init(make_repos(9200u));

        fn->record({ 1 }, make_individual(1e-20), make_individual(0.0));
        fn->record({ 2 }, make_individual(3e-20), make_individual(0.0));
        fn->update();

        constexpr int kDraws = 4000;
        std::vector<int> counts(4, 0);
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked >= 0 && picked < 4, "tiny mixed returned unexpected candidate")) {
                return false;
            }
            ++counts[static_cast<std::size_t>(picked)];
        }
        if (!require(counts[0] == 0 && counts[3] == 0,
                     "tiny mixed must never select zero-score candidates")) {
            return false;
        }
        if (!require(counts[1] >= 820 && counts[1] <= 1180 &&
                         counts[2] >= 2820 && counts[2] <= 3180,
                     "tiny mixed positive counts out of 25%/75% range")) {
            return false;
        }
    }

    // 5d. Huge positive scores whose raw sum would overflow double ([1e308, 1e308])
    {
        auto fn = make_imp_rws("[0, 1]", "huge_weights");
        fn->init(make_repos(9300u));

        fn->record({ 0 }, make_individual(1e308), make_individual(0.0));
        fn->record({ 1 }, make_individual(1e308), make_individual(0.0));
        fn->update();

        constexpr int kDraws = 2000;
        int count0 = 0;
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked == 0 || picked == 1, "huge weights returned unexpected candidate")) {
                return false;
            }
            if (picked == 0) {
                ++count0;
            }
        }
        if (!require(count0 >= 850 && count0 <= 1150,
                     "huge weights count0 out of uniform range: " + std::to_string(count0))) {
            return false;
        }
    }

    // 5e. Infinite positive scores do not produce NaN or throw
    {
        const double inf = std::numeric_limits<double>::infinity();
        auto fn = make_imp_rws("[0, 1, 2]", "inf_weights");
        fn->init(make_repos(9350u));

        fn->record({ 0 }, make_individual(1.0), make_individual(0.0));
        fn->record({ 1 }, make_individual(inf), make_individual(0.0));
        fn->record({ 2 }, make_individual(inf), make_individual(0.0));
        fn->update();

        constexpr int kDraws = 2000;
        std::vector<int> counts(3, 0);
        for (int i = 0; i < kDraws; ++i) {
            const int picked = fn->generate();
            if (!require(picked >= 0 && picked < 3, "inf weights returned unexpected candidate")) {
                return false;
            }
            ++counts[static_cast<std::size_t>(picked)];
        }
        if (!require(counts[0] == 0 &&
                         counts[1] >= 850 && counts[1] <= 1150 &&
                         counts[2] >= 850 && counts[2] <= 1150,
                     "inf weights should select only +inf candidates uniformly")) {
            return false;
        }
    }

    // 5f. Lower (0u) and upper (max()) RNG word boundary tests on [0.0, 1.0, 2.0, 0.0] and [1.0, 2.0, 3.0]
    {
        auto fn = make_imp_rws("[0, 1, 2, 3]", "boundary_mixed");
        fn->record({ 1 }, make_individual(1.0), make_individual(0.0));
        fn->record({ 2 }, make_individual(2.0), make_individual(0.0));
        fn->update();

        // Lower boundary: RNG emits 0u words => must pick index 1 (never zero-weight index 0)
        auto low_rng = std::make_shared<RepeatingWordSequenceRandom>(
            std::vector<adef::Random::result_type>{ 0u, 0u }, 8u, 123u);
        auto low_repos = std::make_shared<TestRepository>();
        low_repos->set_random(low_rng);
        fn->init(low_repos);

        const int low_pick = fn->generate();
        if (!require(low_pick == 1 || low_pick == 2,
                     "lower boundary must only pick positive-weight candidates")) {
            return false;
        }
        if (low_rng->fallback_draws() == 0) {
            if (!require(low_pick == 1,
                         "lower boundary (rnd == 0) should select first positive-weight candidate (index 1)")) {
                return false;
            }
        }

        // Upper boundary: RNG emits max() words => must not throw and must never pick trailing zero-weight index 3
        auto high_rng = std::make_shared<RepeatingWordSequenceRandom>(
            std::vector<adef::Random::result_type>{ adef::Random::max(), adef::Random::max() }, 8u, 456u);
        auto high_repos = std::make_shared<TestRepository>();
        high_repos->set_random(high_rng);
        fn->init(high_repos);

        const int high_pick = fn->generate();
        if (!require(high_pick == 1 || high_pick == 2,
                     "upper boundary must not pick trailing zero-weight candidate")) {
            return false;
        }
        if (high_rng->fallback_draws() == 0) {
            if (!require(high_pick == 2,
                         "upper boundary (max word) should select last positive-weight candidate (index 2)")) {
                return false;
            }
        }
    }

    {
        auto fn = make_imp_rws("[0, 1, 2]", "boundary_all_pos");
        fn->record({ 0 }, make_individual(0.1), make_individual(0.0));
        fn->record({ 1 }, make_individual(0.2), make_individual(0.0));
        fn->record({ 2 }, make_individual(0.3), make_individual(0.0));
        fn->update();

        auto high_rng = std::make_shared<RepeatingWordSequenceRandom>(
            std::vector<adef::Random::result_type>{ adef::Random::max(), adef::Random::max() }, 8u, 789u);
        auto high_repos = std::make_shared<TestRepository>();
        high_repos->set_random(high_rng);
        fn->init(high_repos);

        for (int i = 0; i < 4; ++i) {
            const int picked = fn->generate();
            if (!require(picked >= 0 && picked < 3,
                         "upper boundary on all-positive weights must return a valid candidate without throwing")) {
                return false;
            }
        }
    }

    return true;
}

// ============================================================================
// 6. All four type aliases and SaNSDE-like RealControlRouletteWheelSelectionFunction
// ============================================================================
bool test_all_aliases_and_sansde_control_function(const adef::PrototypeManager& pm)
{
    // RealRouletteWheelSelectionFunction
    {
        auto cfg = load_json_config(R"({
            "classname": "RealRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 10,
            "object": [0.25, 0.75]
        })", "test_rws_alias_real.json");
        auto fn = adef::make_and_setup_type<adef::RealRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(1001u));

        int count_first = 0;
        constexpr int kDraws = 2000;
        for (int i = 0; i < kDraws; ++i) {
            const double val = fn->generate();
            if (!require(val == 0.25 || val == 0.75, "RealRouletteWheelSelectionFunction returned unexpected value")) {
                return false;
            }
            if (val == 0.25) {
                ++count_first;
            }
        }
        if (!require(count_first >= 850 && count_first <= 1150,
                     "RealRouletteWheelSelectionFunction zero-score selection biased")) {
            return false;
        }
    }

    // IntegerControlRouletteWheelSelectionFunction
    {
        auto cfg = load_json_config(R"({
            "classname": "IntegerControlRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 10,
            "object": [
                { "classname": "IntegerConstantFunction", "object": 11 },
                { "classname": "IntegerConstantFunction", "object": 22 }
            ]
        })", "test_rws_alias_int_ctrl.json");
        auto fn = adef::make_and_setup_type<adef::IntegerControlRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(1002u));

        int count_11 = 0;
        constexpr int kDraws = 2000;
        for (int i = 0; i < kDraws; ++i) {
            auto picked = fn->generate();
            if (!require(picked != nullptr, "IntegerControlRouletteWheelSelectionFunction returned null")) {
                return false;
            }
            const int v = picked->generate();
            if (!require(v == 11 || v == 22, "IntegerControlRouletteWheelSelectionFunction returned unexpected inner value")) {
                return false;
            }
            if (v == 11) {
                ++count_11;
            }
        }
        if (!require(count_11 >= 850 && count_11 <= 1150,
                     "IntegerControlRouletteWheelSelectionFunction zero-score selection biased")) {
            return false;
        }
    }

    // RealControlRouletteWheelSelectionFunction matching example/SaNSDE.json (Normal vs Cauchy)
    {
        auto cfg = load_json_config(R"({
            "classname": "RealControlRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 50,
            "object": [
                {
                    "classname": "RealNormalDisFunction",
                    "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                    "stddev": { "classname": "RealConstantFunction", "object": 0.5 }
                },
                {
                    "classname": "RealCauchyDisFunction",
                    "location": { "classname": "RealConstantFunction", "object": 0.0 },
                    "scale": { "classname": "RealConstantFunction", "object": 1.0 }
                }
            ]
        })", "test_rws_sansde_ctrl.json");
        auto fn = adef::make_and_setup_type<adef::RealControlRouletteWheelSelectionFunction>(cfg, pm);
        fn->init(make_repos(1003u));

        constexpr int kDraws = 2000;
        int normal_count = 0;
        int cauchy_count = 0;
        std::shared_ptr<adef::RealFunction> cauchy_ptr;
        for (int i = 0; i < kDraws; ++i) {
            auto picked = fn->generate();
            if (!require(picked != nullptr, "SaNSDE RouletteWheel returned null function")) {
                return false;
            }
            if (std::dynamic_pointer_cast<adef::RealNormalDisFunction>(picked) != nullptr) {
                ++normal_count;
            }
            else if (std::dynamic_pointer_cast<adef::RealCauchyDisFunction>(picked) != nullptr) {
                ++cauchy_count;
                cauchy_ptr = picked;
            }
            else {
                return require(false, "SaNSDE RouletteWheel returned unexpected function subtype");
            }
        }
        if (!require(normal_count >= 850 && normal_count <= 1150 &&
                         cauchy_count >= 850 && cauchy_count <= 1150 &&
                         cauchy_ptr != nullptr,
                     "SaNSDE initial zero-score selection must sample Normal and Cauchy uniformly (" +
                         std::to_string(normal_count) + " vs " + std::to_string(cauchy_count) + ")")) {
            return false;
        }

        // Record wins for Cauchy only -> after update(), Cauchy is selected 100% of the time
        fn->record({ cauchy_ptr }, make_individual(10.0), make_individual(5.0));
        fn->update();
        for (int i = 0; i < 200; ++i) {
            auto picked = fn->generate();
            if (!require(picked == cauchy_ptr,
                         "SaNSDE RouletteWheel should select Cauchy 100% when only Cauchy has positive score")) {
                return false;
            }
        }

        // Second update() resets scores to [0, 0] -> reverts to uniform Normal/Cauchy selection
        fn->update();
        int reset_cauchy_count = 0;
        for (int i = 0; i < kDraws; ++i) {
            if (fn->generate() == cauchy_ptr) {
                ++reset_cauchy_count;
            }
        }
        if (!require(reset_cauchy_count >= 850 && reset_cauchy_count <= 1150,
                     "SaNSDE RouletteWheel should sample uniformly after repeated update() resets scores")) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 7. Determinism and clone isolation
// ============================================================================
bool test_determinism_and_clone_isolation(const adef::PrototypeManager& pm)
{
    auto cfg = load_json_config(R"({
        "classname": "IntegerRouletteWheelSelectionFunction",
        "scoring_function": { "classname": "ImprovedPercentageScoringFunction" },
        "score_size": 10,
        "object": [0, 1, 2, 3]
    })", "test_rws_determinism.json");

    auto proto = adef::make_and_setup_type<adef::IntegerRouletteWheelSelectionFunction>(cfg, pm);

    // Zero-score determinism
    {
        auto fn_a = proto->clone();
        auto fn_b = proto->clone();
        fn_a->init(make_repos(7777u));
        fn_b->init(make_repos(7777u));

        for (int i = 0; i < 200; ++i) {
            if (!require(fn_a->generate() == fn_b->generate(),
                         "zero-score determinism failed at draw " + std::to_string(i))) {
                return false;
            }
        }
    }

    // Weighted determinism + clone isolation
    {
        auto fn_a = proto->clone();
        auto fn_b = proto->clone();
        fn_a->init(make_repos(8888u));
        fn_b->init(make_repos(8888u));

        for (auto& fn : { fn_a, fn_b }) {
            fn->record({ 0 }, make_individual(1.0), make_individual(0.0));
            fn->record({ 1 }, make_individual(2.0), make_individual(0.0));
            fn->record({ 2 }, make_individual(3.0), make_individual(0.0));
            fn->record({ 3 }, make_individual(4.0), make_individual(0.0));
            fn->update();
        }

        // Clone fn_a AFTER updating and mutate fn_a further; fn_c must remain identical to fn_b.
        auto fn_c = fn_a->clone();
        fn_c->init(make_repos(8888u));

        // Mutate fn_a so only candidate 0 has positive score
        fn_a->record({ 0 }, make_individual(10.0), make_individual(0.0));
        fn_a->update();

        for (int i = 0; i < 200; ++i) {
            if (!require(fn_a->generate() == 0,
                         "mutated clone fn_a should select candidate 0 exclusively")) {
                return false;
            }
            if (!require(fn_b->generate() == fn_c->generate(),
                         "weighted determinism / clone isolation failed at draw " + std::to_string(i))) {
                return false;
            }
        }
    }

    return true;
}

} // namespace

int main()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);

    if (!test_empty_container_throws(pm)) {
        return 1;
    }
    if (!test_zero_score_uniform_fallback(pm)) {
        return 1;
    }
    if (!test_weighted_proportional_selection(pm)) {
        return 1;
    }
    if (!test_mixed_zero_and_positive_scores(pm)) {
        return 1;
    }
    if (!test_small_weights_and_boundary_safety(pm)) {
        return 1;
    }
    if (!test_all_aliases_and_sansde_control_function(pm)) {
        return 1;
    }
    if (!test_determinism_and_clone_isolation(pm)) {
        return 1;
    }

    std::cout << "All RouletteWheelSelectionFunction unit tests passed." << std::endl;
    return 0;
}
