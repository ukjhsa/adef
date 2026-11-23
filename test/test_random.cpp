#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "adef.h"
#include "Configuration.h"
#include "DEBinomial.h"
#include "DEMutation.h"
#include "DERand.h"
#include "DEVariantY.h"
#include "Evolution.h"
#include "Experiment.h"
#include "ExperimentalStatistics.h"
#include "Individual.h"
#include "Initializer.h"
#include "Parameters.h"
#include "Population.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Repository.h"
#include "Statistics.h"
#include "System.h"
#include "cm/cf/func/NormalDisFunction.h"
#include "cm/cf/func/RandomSelectionFunction.h"
#include "cm/cf/func/RouletteWheelSelectionFunction.h"
#include "cm/cu/SdeFControlUpdate.h"
#include "problem/basic/QuarticFunctionWithNoise.h"
#include "problem/basic/Sphere.h"

namespace {

bool require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        return false;
    }
    return true;
}

adef::Configuration load_json_config(const std::string& json_text,
                                     const std::string& filename)
{
    {
        std::ofstream ofs(filename);
        ofs << json_text;
    }

    adef::Configuration config;
    const bool loaded = config.load_config(filename);
    std::remove(filename.c_str());
    if (!loaded) {
        throw std::runtime_error("Failed to load test configuration: " + filename);
    }
    return config;
}

class SequenceRandom : public adef::Random
{
public:
    SequenceRandom() : values_{100u, 200u, 300u, 400u}, pos_(0), draw_count_(0)
    {
    }

    explicit SequenceRandom(std::vector<result_type> values) :
        values_(std::move(values)),
        pos_(0),
        draw_count_(0)
    {
    }

    SequenceRandom(const SequenceRandom& rhs) :
        adef::Random(rhs),
        values_(rhs.values_),
        pos_(rhs.pos_),
        draw_count_(rhs.draw_count_)
    {
    }

    result_type next() override
    {
        ++draw_count_;
        const result_type val = values_[pos_ % values_.size()];
        ++pos_;
        return val;
    }

    std::size_t draw_count() const noexcept { return draw_count_; }

protected:
    void seed_engine(SeedType seed_value) override
    {
        pos_ = static_cast<std::size_t>(seed_value) % values_.size();
    }

private:
    std::vector<result_type> values_;
    std::size_t pos_;
    std::size_t draw_count_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<SequenceRandom>(*this);
    }
};

class ConstantRandom : public adef::Random
{
public:
    explicit ConstantRandom(result_type constant = 0) : constant_(constant)
    {
    }

    result_type next() override
    {
        return constant_;
    }

protected:
    void seed_engine(SeedType) override
    {
    }

private:
    result_type constant_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<ConstantRandom>(*this);
    }
};

class RecordingExperimentalStatistics : public adef::ExperimentalStatistics
{
public:
    void take_statistics(std::shared_ptr<const adef::Repository> repos) override
    {
        adef::ExperimentalStatistics::take_statistics(repos);
        observed_seeds_.push_back(repos->random()->seed());

        std::vector<double> snapshot;
        auto pop = repos->population();
        for (std::size_t i = 0; i < pop->population_size(); ++i) {
            auto ind = pop->at(i);
            for (std::size_t d = 0; d < ind->dimension_of_variable(); ++d) {
                snapshot.push_back(ind->variables(d));
            }
        }
        final_populations_.push_back(std::move(snapshot));
    }

    void output_statistics_on_run(std::shared_ptr<const adef::Repository>,
                                  const std::string&) const override
    {
    }

    void output_statistics(const std::string&) override
    {
    }

    const std::vector<adef::Random::SeedType>& observed_seeds() const
    {
        return observed_seeds_;
    }

    const std::vector<std::vector<double>>& final_populations() const
    {
        return final_populations_;
    }

private:
    std::vector<adef::Random::SeedType> observed_seeds_;
    std::vector<std::vector<double>> final_populations_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<RecordingExperimentalStatistics>(*this);
    }
};

class RecordingInitializer : public adef::Initializer
{
public:
    explicit RecordingInitializer(
        std::shared_ptr<std::vector<std::vector<double>>> sink = nullptr) :
        sink_(std::move(sink))
    {
    }

    void initialize(std::shared_ptr<adef::Repository> repos) const override
    {
        adef::Initializer::initialize(repos);
        if (sink_) {
            std::vector<double> snapshot;
            auto pop = repos->population();
            for (std::size_t i = 0; i < pop->population_size(); ++i) {
                auto ind = pop->at(i);
                for (std::size_t d = 0; d < ind->dimension_of_variable(); ++d) {
                    snapshot.push_back(ind->variables(d));
                }
            }
            sink_->push_back(std::move(snapshot));
        }
    }

private:
    std::shared_ptr<std::vector<std::vector<double>>> sink_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<RecordingInitializer>(*this);
    }
};

// 1. Instance isolation and faithful clone() state copy.
bool test_isolation_and_clone()
{
    adef::Random r1(42u);
    adef::Random r2(42u);
    adef::Random r3(99u);

    if (!require(r1.seed() == 42u && !r1.is_auto_seeded(),
                 "Expected fixed seed 42 and is_auto_seeded() == false")) {
        return false;
    }

    // Advance r1 by 50 draws and verify r2 is completely unaffected.
    std::vector<adef::Random::result_type> r1_first50;
    for (int i = 0; i < 50; ++i) {
        r1_first50.push_back(r1.next());
    }

    std::vector<adef::Random::result_type> r2_first50;
    for (int i = 0; i < 50; ++i) {
        r2_first50.push_back(r2.next());
    }

    if (!require(r1_first50 == r2_first50,
                 "Expected two Random instances with same seed to produce identical sequences")) {
        return false;
    }

    bool any_diff_seed = false;
    for (int i = 0; i < 10; ++i) {
        if (r3.next() != r1_first50[static_cast<std::size_t>(i)]) {
            any_diff_seed = true;
            break;
        }
    }
    if (!require(any_diff_seed,
                 "Expected different seeds to produce distinct sequences")) {
        return false;
    }

    // Clone r1 AFTER advancing 50 steps: clone must continue from step 51, not replay step 0.
    auto r1_clone = r1.clone();
    if (!require(r1_clone->seed() == r1.seed(),
                 "Expected cloned Random to preserve seed()")) {
        return false;
    }

    std::vector<adef::Random::result_type> r1_next20;
    std::vector<adef::Random::result_type> clone_next20;
    for (int i = 0; i < 20; ++i) {
        r1_next20.push_back(r1.next());
    }
    for (int i = 0; i < 20; ++i) {
        clone_next20.push_back(r1_clone->next());
    }

    if (!require(r1_next20 == clone_next20,
                 "Expected cloned Random to faithfully continue from parent's current state")) {
        return false;
    }
    if (!require(std::vector<adef::Random::result_type>(r1_first50.begin(),
                                                        r1_first50.begin() + 20) !=
                     clone_next20,
                 "Expected cloned Random NOT to reset/replay from initial seed")) {
        return false;
    }

    return true;
}

// 2. Per-run derivation determinism, uniqueness, and dynamic type preservation.
bool test_per_run_derivation()
{
    adef::Random master(12345u);

    std::set<adef::Random::SeedType> derived_seeds;
    std::vector<adef::Random::result_type> first_draws;
    constexpr std::size_t kRuns = 16;

    for (std::size_t run = 0; run < kRuns; ++run) {
        auto child_a = master.derive(run);
        auto child_b = master.derive(run);

        if (!require(child_a->seed() == child_b->seed(),
                     "Expected derive(run) to be deterministic")) {
            return false;
        }
        derived_seeds.insert(child_a->seed());

        const auto draw_a = child_a->next();
        const auto draw_b = child_b->next();
        if (!require(draw_a == draw_b,
                     "Expected identically derived streams to produce identical draws")) {
            return false;
        }
        first_draws.push_back(draw_a);
    }

    if (!require(derived_seeds.size() == kRuns,
                 "Expected all derived run seeds to be distinct")) {
        return false;
    }

    std::set<adef::Random::result_type> unique_draws(first_draws.begin(), first_draws.end());
    if (!require(unique_draws.size() == kRuns,
                 "Expected derived per-run generators to produce distinct sequences")) {
        return false;
    }

    // Dynamic type preservation via derive()/spawn()
    adef::Mt19937_64Random mt64_master(777u);
    auto mt64_child = mt64_master.derive(2);
    if (!require(std::dynamic_pointer_cast<adef::Mt19937_64Random>(mt64_child) != nullptr,
                 "Expected derive() to preserve dynamic type Mt19937_64Random")) {
        return false;
    }

    return true;
}

// 3. Fixed vs. auto seed configuration, validation, and reproducibility via queried seed.
bool test_seed_configuration(const adef::PrototypeManager& pm)
{
    // Fixed seed (including 0 and large uint32 > INT_MAX)
    {
        auto cfg0 = load_json_config(R"({"classname":"Random","seed":0})",
                                     "test_rnd_seed0.json");
        auto r0 = adef::make_and_setup_type<adef::Random>(cfg0, pm);
        if (!require(r0->seed() == 0u && !r0->is_auto_seeded(),
                     "Expected seed 0 to be deterministic")) {
            return false;
        }

        auto cfg_big = load_json_config(R"({"classname":"Random","seed":3000000000})",
                                        "test_rnd_seed_big.json");
        auto r_big = adef::make_and_setup_type<adef::Random>(cfg_big, pm);
        if (!require(r_big->seed() == 3000000000u && !r_big->is_auto_seeded(),
                     "Expected large uint32 seed 3000000000 to load accurately")) {
            return false;
        }
    }

    // Omitted seed, null seed, "auto", and "random" modes
    const std::vector<std::string> auto_configs = {
        R"({"classname":"Random"})",
        R"({"classname":"Random","seed":null})",
        R"({"classname":"Random","seed":"auto"})",
        R"({"classname":"Random","seed":"AUTO"})",
        R"({"classname":"Random","seed":"random"})"
    };
    std::set<adef::Random::SeedType> auto_seeds;
    for (std::size_t i = 0; i < auto_configs.size(); ++i) {
        auto cfg = load_json_config(auto_configs[i],
                                    "test_rnd_auto_" + std::to_string(i) + ".json");
        auto rng = adef::make_and_setup_type<adef::Random>(cfg, pm);
        if (!require(rng->is_auto_seeded(),
                     "Expected auto-seeded mode for config index " + std::to_string(i))) {
            return false;
        }

        const auto effective_seed = rng->seed();
        auto_seeds.insert(effective_seed);

        // Reproducing the exact sequence using the queried effective seed
        adef::Random replay(effective_seed);
        for (int step = 0; step < 10; ++step) {
            if (!require(rng->next() == replay.next(),
                         "Expected queried auto seed to reproduce exact sequence")) {
                return false;
            }
        }
    }
    if (!require(auto_seeds.size() > 1,
                 "Expected consecutive auto-seeded instances to produce varying seeds")) {
        return false;
    }

    // Invalid seed configurations must throw std::invalid_argument
    const std::vector<std::string> invalid_configs = {
        R"({"classname":"Random","seed":-1})",
        R"({"classname":"Random","seed":3.14})",
        R"({"classname":"Random","seed":"invalid_mode"})",
        R"({"classname":"Random","seed":true})",
        R"({"classname":"Random","seed":[]})",
        R"({"classname":"Random","seed":5000000000})"
    };
    for (std::size_t i = 0; i < invalid_configs.size(); ++i) {
        auto cfg = load_json_config(invalid_configs[i],
                                    "test_rnd_invalid_" + std::to_string(i) + ".json");
        bool threw = false;
        try {
            (void)adef::make_and_setup_type<adef::Random>(cfg, pm);
        }
        catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!require(threw,
                     "Expected std::invalid_argument for invalid seed config index " +
                         std::to_string(i))) {
            return false;
        }
    }

    return true;
}

// 4. Polymorphic PRNG engine substitution & <random> distribution interoperability.
bool test_polymorphic_engines_and_test_doubles(const adef::PrototypeManager& pm)
{
    // Built-in alternative engines via PrototypeManager
    auto mt64_cfg = load_json_config(R"({"classname":"Mt19937_64Random","seed":123})",
                                     "test_rnd_mt64.json");
    auto mt64 = adef::make_and_setup_type<adef::Random>(mt64_cfg, pm);

    auto minstd_cfg = load_json_config(R"({"classname":"MinstdRandRandom","seed":123})",
                                       "test_rnd_minstd.json");
    auto minstd = adef::make_and_setup_type<adef::Random>(minstd_cfg, pm);

    auto mt32_cfg = load_json_config(R"({"classname":"Random","seed":123})",
                                     "test_rnd_mt32.json");
    auto mt32 = adef::make_and_setup_type<adef::Random>(mt32_cfg, pm);

    // Verify standard <random> distributions interoperate directly with Random base reference
    std::uniform_real_distribution<double> u_real(0.0, 1.0);
    std::normal_distribution<double> normal(0.0, 1.0);
    std::cauchy_distribution<double> cauchy(0.0, 1.0);

    const double v_mt32 = mt32->generate(u_real);
    const double v_mt64 = mt64->generate(u_real);
    const double v_minstd = minstd->generate(u_real);
    const double direct_urbg = u_real(*mt64);

    if (!require(v_mt32 >= 0.0 && v_mt32 < 1.0 &&
                     v_mt64 >= 0.0 && v_mt64 < 1.0 &&
                     v_minstd >= 0.0 && v_minstd < 1.0 &&
                     direct_urbg >= 0.0 && direct_urbg < 1.0,
                 "Expected <random> uniform_real_distribution values in [0, 1)")) {
        return false;
    }
    if (!require(v_mt32 != v_mt64 && v_mt32 != v_minstd,
                 "Expected distinct PRNG engines with same seed to produce different streams")) {
        return false;
    }
    if (!require(std::isfinite(mt64->generate(normal)) &&
                     std::isfinite(minstd->generate(cauchy)),
                 "Expected normal and cauchy distributions to work with alternative engines")) {
        return false;
    }

    // Temporary (rvalue) distribution passed to generate()
    const int rvalue_val = mt32->generate(std::uniform_int_distribution<int>(5, 10));
    if (!require(rvalue_val >= 5 && rvalue_val <= 10,
                 "Expected rvalue distribution to work with Random::generate()")) {
        return false;
    }

    // Custom deterministic test double substituted through Random base interface
    auto seq_rng = std::make_shared<SequenceRandom>(
        std::vector<adef::Random::result_type>{0x10000000u, 0x80000000u, 0xF0000000u});

    class LocalRepo : public adef::Repository
    {
    public:
        void configure_for_test(std::shared_ptr<adef::Random> r,
                                std::shared_ptr<adef::Problem> p,
                                std::shared_ptr<adef::Population> pop)
        {
            random_ = std::move(r);
            problem_ = std::move(p);
            population_ = std::move(pop);
            parameters_ = std::make_shared<adef::Parameters>();
        }
    };

    auto pop_cfg = load_json_config(
        R"({"classname":"Population","population_size":4,"Individual":{"classname":"Individual"}})",
        "test_rnd_pop.json");
    auto pop = adef::make_and_setup_type<adef::Population>(pop_cfg, pm);

    auto prob_cfg = load_json_config(
        R"({"classname":"QuarticFunctionWithNoise","dimension_of_decision_space":3})",
        "test_rnd_prob.json");
    auto prob = adef::make_and_setup_type<adef::Problem>(prob_cfg, pm);

    auto repo = std::make_shared<LocalRepo>();
    repo->configure_for_test(seq_rng, prob, pop);
    prob->init(repo);
    pop->init(repo);

    // Drive Initializer, QuarticFunctionWithNoise, DEBinomial, RealNormalDisFunction
    adef::Initializer initializer;
    initializer.init(repo);
    initializer.initialize(repo);
    const std::size_t after_init = seq_rng->draw_count();
    if (!require(after_init > 0,
                 "Expected Initializer to draw from injected SequenceRandom")) {
        return false;
    }

    prob->evaluate(pop->at(0));
    if (!require(seq_rng->draw_count() > after_init,
                 "Expected QuarticFunctionWithNoise to draw from injected SequenceRandom")) {
        return false;
    }

    adef::DEBinomial binomial;
    binomial.init(repo);
    const std::size_t before_xo = seq_rng->draw_count();
    auto trial = binomial.crossover(pop->at(0), pop->at(1), 0.5);
    if (!require(trial != nullptr && seq_rng->draw_count() > before_xo,
                 "Expected DEBinomial to draw from injected SequenceRandom")) {
        return false;
    }

    // DEBinomial edge case: 0-dimensional individual should safely return without throwing
    auto empty_ind = std::make_shared<adef::Individual>();
    auto empty_trial = binomial.crossover(empty_ind, empty_ind, 0.5);
    if (!require(empty_trial != nullptr && empty_trial->dimension_of_variable() == 0,
                 "Expected DEBinomial to handle 0-dimensional individuals safely")) {
        return false;
    }

    auto normal_cfg = load_json_config(R"({
        "classname": "RealNormalDisFunction",
        "mean": { "classname": "RealConstantFunction", "object": 0.5 },
        "stddev": { "classname": "RealConstantFunction", "object": 0.1 }
    })", "test_rnd_normal_fn.json");
    auto normal_fn = adef::make_and_setup_type<adef::RealNormalDisFunction>(normal_cfg, pm);
    normal_fn->init(repo);
    normal_fn->update();
    const std::size_t before_fn = seq_rng->draw_count();
    const double fn_val = normal_fn->generate();
    if (!require(std::isfinite(fn_val) && seq_rng->draw_count() > before_fn,
                 "Expected RealNormalDisFunction to draw from injected SequenceRandom")) {
        return false;
    }

    return true;
}

// 5. Unbiased integer sampling and mutually exclusive index sampling.
bool test_unbiased_and_exclusive_sampling()
{
    adef::Random rng(2026u);

    // uniform_int & uniform_index bounds and coverage
    {
        std::set<std::size_t> seen;
        for (int i = 0; i < 500; ++i) {
            const std::size_t idx = rng.uniform_index(7);
            if (!require(idx < 7, "Expected uniform_index(7) in [0, 6]")) {
                return false;
            }
            seen.insert(idx);
        }
        if (!require(seen.size() == 7,
                     "Expected uniform_index(7) to cover all values 0..6")) {
            return false;
        }

        // Single-value range
        for (int i = 0; i < 20; ++i) {
            if (!require(rng.uniform_index(1) == 0 && rng.uniform_int<int>(42, 42) == 42,
                         "Expected singleton ranges to return the sole value")) {
                return false;
            }
        }

        // Range larger than MSVC RAND_MAX (32767)
        bool saw_above_rand_max = false;
        for (int i = 0; i < 200; ++i) {
            const std::size_t big = rng.uniform_index(100000);
            if (big > 32767) {
                saw_above_rand_max = true;
                break;
            }
        }
        if (!require(saw_above_rand_max,
                     "Expected uniform_index(100000) to produce values > 32767")) {
            return false;
        }

        // Invalid bounds throw std::invalid_argument
        bool threw_zero = false;
        try {
            (void)rng.uniform_index(0);
        }
        catch (const std::invalid_argument&) {
            threw_zero = true;
        }
        bool threw_inverted = false;
        try {
            (void)rng.uniform_int<int>(10, 5);
        }
        catch (const std::invalid_argument&) {
            threw_inverted = true;
        }
        if (!require(threw_zero && threw_inverted,
                     "Expected std::invalid_argument on invalid uniform_index / uniform_int range")) {
            return false;
        }
    }

    // sample_exclusive_indices: low fill ratio (rejection path) + sequential exclusion updates
    {
        for (int trial = 0; trial < 100; ++trial) {
            std::vector<std::size_t> excluded{5, 5, 999}; // duplicate + out-of-range
            auto first = rng.sample_exclusive_indices(2, 0, 50, excluded);
            auto second = rng.sample_exclusive_indices(3, 0, 50, excluded);

            if (!require(first.size() == 2 && second.size() == 3,
                         "Expected requested exclusive counts")) {
                return false;
            }
            std::set<std::size_t> combined{5};
            for (std::size_t v : first) {
                if (!require(v < 50 && combined.insert(v).second,
                             "Expected first batch in [0, 50) and mutually exclusive")) {
                    return false;
                }
            }
            for (std::size_t v : second) {
                if (!require(v < 50 && combined.insert(v).second,
                             "Expected second batch in [0, 50) and mutually exclusive with first")) {
                    return false;
                }
            }
        }
    }

    // sample_exclusive_indices: high fill ratio (Fisher-Yates full permutation)
    {
        std::vector<std::size_t> excluded{12, 15, 12, 100};
        auto all_remaining = rng.sample_exclusive_indices(8, 10, 20, excluded);
        if (!require(all_remaining.size() == 8,
                     "Expected exact 8 remaining indices in [10, 20) excluding {12, 15}")) {
            return false;
        }
        std::set<std::size_t> unique_vals(all_remaining.begin(), all_remaining.end());
        if (!require(unique_vals.size() == 8 &&
                         unique_vals.count(12) == 0 &&
                         unique_vals.count(15) == 0 &&
                         *unique_vals.begin() == 10 &&
                         *unique_vals.rbegin() == 19,
                     "Expected full permutation of remaining [10, 20) \\ {12, 15}")) {
            return false;
        }
    }

    // sample_exclusive_indices: large excluded set (> 16)
    {
        std::vector<std::size_t> excluded;
        for (std::size_t i = 0; i < 20; ++i) {
            excluded.push_back(i * 2);
        }
        auto picked = rng.sample_exclusive_indices(5, 0, 100, excluded);
        std::set<std::size_t> unique_vals(picked.begin(), picked.end());
        if (!require(unique_vals.size() == 5,
                     "Expected 5 distinct indices with large excluded list")) {
            return false;
        }
        for (std::size_t v : picked) {
            if (!require(v < 100 && !(v < 40 && v % 2 == 0),
                         "Expected sampled value not in large excluded set")) {
                return false;
            }
        }
    }

    // sample_exclusive_indices: count == 0 returns empty even on empty valid range
    {
        auto empty_res = rng.sample_exclusive_indices(0, 5, 5);
        if (!require(empty_res.empty(),
                     "Expected empty vector when count == 0")) {
            return false;
        }
    }

    // sample_exclusive_indices: degenerate constant mock RNG still terminates cleanly
    {
        ConstantRandom constant_rng(0);
        std::vector<std::size_t> excluded{0};
        auto res = constant_rng.sample_exclusive_indices(3, 0, 50, excluded);
        std::set<std::size_t> unique_vals(res.begin(), res.end());
        if (!require(unique_vals.size() == 3 && unique_vals.count(0) == 0,
                     "Expected fallback Fisher-Yates to terminate cleanly even with constant RNG")) {
            return false;
        }
    }

    // sample_exclusive_indices: validation errors throw std::invalid_argument
    {
        bool threw_inverted = false;
        try {
            (void)rng.sample_exclusive_indices(0, 10, 5);
        }
        catch (const std::invalid_argument&) {
            threw_inverted = true;
        }

        bool threw_empty = false;
        try {
            (void)rng.sample_exclusive_indices(1, 5, 5);
        }
        catch (const std::invalid_argument&) {
            threw_empty = true;
        }

        bool threw_exhausted = false;
        try {
            std::vector<std::size_t> excluded{0, 1};
            (void)rng.sample_exclusive_indices(3, 0, 4, excluded);
        }
        catch (const std::invalid_argument&) {
            threw_exhausted = true;
        }

        if (!require(threw_inverted && threw_empty && threw_exhausted,
                     "Expected std::invalid_argument for inverted, empty, or exhausted range")) {
            return false;
        }
    }

    return true;
}

// 6. Multi-run Experiment isolation, reproducibility, optional "Random", and example/*.json loading.
bool test_experiment_multi_run_and_optional_config()
{
    auto make_experiment_json = [](const std::string& random_block) {
        return std::string(R"({
            "classname": "Experiment",
            )") + random_block + R"(
            "number_of_runs": 3,
            "ExperimentalStatistics": { "classname": "RecordingExperimentalStatistics" },
            "Repository": {
                "classname": "Repository",
                "algorithm_name": "test_de",
                "Evolution": { "classname": "Evolution", "max_generation": 5 },
                "Problem": { "classname": "Sphere", "dimension_of_decision_space": 5 },
                "Statistics": { "classname": "Statistics", "item": [] },
                "Parameters": { "classname": "Parameters" },
                "Initializer": { "classname": "RecordingInitializer" },
                "Evaluator": { "classname": "Evaluator" },
                "Population": {
                    "classname": "Population",
                    "population_size": 10,
                    "Individual": { "classname": "Individual" }
                },
                "Reproduction": { "classname": "DEReproduction" },
                "EnvironmentalSelection": { "classname": "DEEnvironmentalSelection" },
                "Mutation": {
                    "classname": "DEMutation",
                    "DEVariantX": { "classname": "DERand" },
                    "DEVariantY": { "classname": "DEVariantY", "number_of_difference_vector": 1 },
                    "F": {
                        "classname": "RealControlMechanism",
                        "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
                        "ControlParameter": { "classname": "RealSingleControlParameter" },
                        "ControlFunction": {
                            "classname": "SingleControlFunction",
                            "Function": { "classname": "RealConstantFunction", "object": 0.5 }
                        },
                        "ControlSelection": { "classname": "NonInfoControlSelection" },
                        "ControlUpdate": []
                    }
                },
                "Crossover": {
                    "classname": "DECrossover",
                    "DEVariantZ": { "classname": "DEBinomial" },
                    "CR": {
                        "classname": "RealControlMechanism",
                        "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
                        "ControlParameter": { "classname": "RealSingleControlParameter" },
                        "ControlFunction": {
                            "classname": "SingleControlFunction",
                            "Function": { "classname": "RealConstantFunction", "object": 0.5 }
                        },
                        "ControlSelection": { "classname": "NonInfoControlSelection" },
                        "ControlUpdate": []
                    }
                },
                "Repair": { "classname": "ReflectiveRepair" }
            }
        })";
    };

    auto run_once = [&](const std::string& random_block,
                        const std::string& tag,
                        std::vector<std::vector<double>>& initial_pops,
                        std::shared_ptr<adef::Experiment>& out_exp,
                        std::shared_ptr<const RecordingExperimentalStatistics>& out_stats) {
        adef::PrototypeManager pm;
        adef::register_type(pm);
        pm.register_type("RecordingExperimentalStatistics",
                         std::make_shared<RecordingExperimentalStatistics>());
        auto init_sink = std::make_shared<std::vector<std::vector<double>>>();
        pm.register_type("RecordingInitializer",
                         std::make_shared<RecordingInitializer>(init_sink));

        auto cfg = load_json_config(make_experiment_json(random_block),
                                    "test_rnd_exp_" + tag + ".json");
        out_exp = adef::make_and_setup_type<adef::Experiment>(cfg, pm);
        out_exp->run();
        out_stats = std::dynamic_pointer_cast<const RecordingExperimentalStatistics>(
            out_exp->statistics());
        initial_pops = *init_sink;
    };

    // 6a. Same fixed master seed: distinct runs within one experiment, 100% identical across repeats.
    std::vector<std::vector<double>> init_a, init_b;
    std::shared_ptr<adef::Experiment> exp_a, exp_b;
    std::shared_ptr<const RecordingExperimentalStatistics> stats_a, stats_b;

    const std::string fixed_random = R"("Random": { "classname": "Random", "seed": 42 },)";
    run_once(fixed_random, "fixed_a", init_a, exp_a, stats_a);
    run_once(fixed_random, "fixed_b", init_b, exp_b, stats_b);

    if (!require(exp_a->random()->seed() == 42u && exp_a->run_seeds().size() == 3,
                 "Expected master seed 42 and 3 recorded run seeds")) {
        return false;
    }
    if (!require(exp_a->run_seeds() == stats_a->observed_seeds(),
                 "Expected Experiment::run_seeds() to match Repository::random()->seed() per run")) {
        return false;
    }

    // Runs within one experiment must be distinct (fixes multi-run replay bug)
    if (!require(exp_a->run_seeds()[0] != exp_a->run_seeds()[1] &&
                     exp_a->run_seeds()[1] != exp_a->run_seeds()[2] &&
                     exp_a->run_seeds()[0] != exp_a->run_seeds()[2],
                 "Expected all 3 derived run seeds to be distinct")) {
        return false;
    }
    if (!require(init_a.size() == 3 &&
                     init_a[0] != init_a[1] &&
                     init_a[1] != init_a[2] &&
                     init_a[0] != init_a[2],
                 "Expected distinct initial populations across runs within one Experiment")) {
        return false;
    }
    if (!require(stats_a->final_populations()[0] != stats_a->final_populations()[1] &&
                     stats_a->final_populations()[1] != stats_a->final_populations()[2],
                 "Expected distinct evolved populations across runs within one Experiment")) {
        return false;
    }

    // Repeating with the same master seed must reproduce every run identically
    if (!require(exp_a->run_seeds() == exp_b->run_seeds() &&
                     init_a == init_b &&
                     stats_a->final_populations() == stats_b->final_populations() &&
                     stats_a->best_fitness() == stats_b->best_fitness(),
                 "Expected repeated Experiment with same seed to reproduce every run identically")) {
        return false;
    }

    // Standalone reproduction of a single run using its queried effective run seed
    {
        auto standalone_repo = exp_a->repository()->clone();
        auto standalone_rng = std::make_shared<adef::Random>(exp_a->run_seeds()[1]);
        standalone_repo->init(standalone_rng);
        standalone_repo->evolution()->evolve(standalone_repo);

        if (!require(standalone_repo->statistics()->best_fitness_so_far() ==
                         stats_a->best_fitness()[1],
                     "Expected standalone run seeded with run_seeds()[1] to match run 1 exactly")) {
            return false;
        }
    }

    // 6b. Alternative PRNG engine configured in Experiment preserves engine across runs
    {
        std::vector<std::vector<double>> init_64_a, init_64_b;
        std::shared_ptr<adef::Experiment> exp_64_a, exp_64_b;
        std::shared_ptr<const RecordingExperimentalStatistics> stats_64_a, stats_64_b;

        const std::string mt64_random =
            R"("Random": { "classname": "Mt19937_64Random", "seed": 42 },)";
        run_once(mt64_random, "mt64_a", init_64_a, exp_64_a, stats_64_a);
        run_once(mt64_random, "mt64_b", init_64_b, exp_64_b, stats_64_b);

        if (!require(init_64_a == init_64_b &&
                         stats_64_a->best_fitness() == stats_64_b->best_fitness() &&
                         init_64_a[0] != init_a[0],
                     "Expected Mt19937_64Random Experiment to be reproducible and distinct from Mt19937")) {
            return false;
        }
    }

    // 6c. Optional "Random" omitted in Experiment config auto-seeds and is traceable
    {
        std::vector<std::vector<double>> init_opt;
        std::shared_ptr<adef::Experiment> exp_opt;
        std::shared_ptr<const RecordingExperimentalStatistics> stats_opt;

        run_once("", "optional_random", init_opt, exp_opt, stats_opt);
        if (!require(exp_opt->random() != nullptr &&
                         exp_opt->random()->is_auto_seeded() &&
                         exp_opt->run_seeds().size() == 3 &&
                         init_opt[0] != init_opt[1],
                     "Expected Experiment with omitted Random to auto-seed and run distinct runs")) {
            return false;
        }

        // Replaying with the queried auto-generated master seed reproduces all runs identically
        const auto queried_master_seed = exp_opt->random()->seed();
        std::vector<std::vector<double>> init_replay;
        std::shared_ptr<adef::Experiment> exp_replay;
        std::shared_ptr<const RecordingExperimentalStatistics> stats_replay;
        run_once("\"Random\": { \"classname\": \"Random\", \"seed\": " +
                     std::to_string(queried_master_seed) + " },",
                 "optional_replay",
                 init_replay,
                 exp_replay,
                 stats_replay);

        if (!require(init_replay == init_opt &&
                         stats_replay->best_fitness() == stats_opt->best_fitness(),
                     "Expected queried auto-generated Experiment master seed to reproduce all runs")) {
            return false;
        }
    }

    // 6d. Existing example/*.json configurations omitting "Random" load cleanly
#ifdef ADEF_SOURCE_DIR
    {
        adef::PrototypeManager pm;
        adef::register_type(pm);
        const std::vector<std::string> example_files = {
            "example/rand1bin_F0.5CR0.5.json",
            "example/jDE.json",
            "example/SaDE.json",
            "example/SDE.json",
            "example/NSDE.json",
            "example/SaNSDE.json",
            "example/DEPD.json",
            "example/ISADE.json"
        };
        for (const auto& rel_path : example_files) {
            const std::string full_path = std::string(ADEF_SOURCE_DIR) + "/" + rel_path;
            adef::Configuration cfg;
            if (!require(cfg.load_config(full_path),
                         "Expected example config to load: " + rel_path)) {
                return false;
            }
            auto sys = adef::make_and_setup_type<adef::System>("System", cfg, pm);
            if (!require(sys != nullptr,
                         "Expected System setup to succeed without Random config: " + rel_path)) {
                return false;
            }
        }
    }
#endif

    return true;
}

} // namespace

int main()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);

    if (!test_isolation_and_clone()) {
        return 1;
    }
    if (!test_per_run_derivation()) {
        return 1;
    }
    if (!test_seed_configuration(pm)) {
        return 1;
    }
    if (!test_polymorphic_engines_and_test_doubles(pm)) {
        return 1;
    }
    if (!test_unbiased_and_exclusive_sampling()) {
        return 1;
    }
    if (!test_experiment_multi_run_and_optional_config()) {
        return 1;
    }

    std::cout << "All Random redesign tests passed." << std::endl;
    return 0;
}
