#include <any>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "adef.h"
#include "Configuration.h"
#include "Individual.h"
#include "Parameters.h"
#include "Population.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Repository.h"
#include "cm/cf/func/Function.h"
#include "cm/cp/MultipleControlParameter.h"
#include "cm/cu/AverageFitnessControlUpdate.h"
#include "cm/cu/CurrentFitnessControlUpdate.h"
#include "cm/cu/GenerationControlUpdate.h"
#include "cm/cu/MaxFitnessControlUpdate.h"
#include "cm/cu/MinFitnessControlUpdate.h"
#include "cm/cu/SdeFControlUpdate.h"

namespace {

bool require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        return false;
    }
    return true;
}

bool approx_equal(double lhs, double rhs, double tolerance = 1e-12)
{
    return std::abs(lhs - rhs) <= tolerance;
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

    void set_population(std::shared_ptr<adef::Population> pop)
    {
        population_ = std::move(pop);
    }

    void set_random(std::shared_ptr<adef::Random> rng)
    {
        random_ = std::move(rng);
    }
};

class SpyFunction : public adef::RealFunction
{
public:
    SpyFunction(bool accept_records,
                std::string accepted_name = "*",
                unsigned int num_params = 1) :
        accept_records_(accept_records),
        accepted_name_(std::move(accepted_name)),
        num_params_(num_params),
        record_calls_(0)
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    double generate() override
    {
        return 0.0;
    }

    bool record(const std::vector<std::any>& params,
                const std::string& name = "") override
    {
        ++record_calls_;
        if (!accept_records_) {
            return false;
        }
        if (accepted_name_ != "*" && name != accepted_name_) {
            return false;
        }
        last_name_ = name;
        last_params_ = params;
        return true;
    }

    bool record(const std::vector<std::any>& params,
                std::shared_ptr<const adef::Individual>,
                std::shared_ptr<const adef::Individual>,
                const std::string& name = "") override
    {
        return record(params, name);
    }

    void update() override
    {
    }

    unsigned int number_of_parameters() const override
    {
        return num_params_;
    }

    const std::string& last_name() const { return last_name_; }
    const std::vector<std::any>& last_params() const { return last_params_; }
    int record_calls() const { return record_calls_; }

    double last_double(std::size_t index = 0) const
    {
        return std::any_cast<double>(last_params_.at(index));
    }

    int last_int(std::size_t index = 0) const
    {
        return std::any_cast<int>(last_params_.at(index));
    }

    std::vector<double> last_doubles() const
    {
        std::vector<double> result;
        result.reserve(last_params_.size());
        for (const auto& item : last_params_) {
            result.push_back(std::any_cast<double>(item));
        }
        return result;
    }

private:
    bool accept_records_;
    std::string accepted_name_;
    unsigned int num_params_;
    int record_calls_;
    std::string last_name_;
    std::vector<std::any> last_params_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<SpyFunction>(*this);
    }
};

std::shared_ptr<adef::Population> make_population_with_fitness(
    const std::vector<double>& fitnesses,
    const adef::PrototypeManager& pm,
    const std::string& tag)
{
    auto cfg = load_json_config(
        "{\"classname\":\"Population\",\"population_size\":" +
            std::to_string(fitnesses.size()) +
            ",\"Individual\":{\"classname\":\"Individual\"}}",
        "test_cu_pop_" + tag + ".json");
    auto pop = adef::make_and_setup_type<adef::Population>(cfg, pm);
    for (std::size_t idx = 0; idx < fitnesses.size(); ++idx) {
        pop->at(idx)->set_fitness_value(fitnesses[idx]);
    }
    return pop;
}

std::shared_ptr<adef::RealMultipleControlParameter> make_multiple_parameter(
    const std::vector<double>& values,
    const adef::PrototypeManager& pm,
    const std::string& tag)
{
    auto cfg = load_json_config(
        "{\"classname\":\"RealMultipleControlParameter\",\"number_of_objects\":" +
            std::to_string(values.size()) + "}",
        "test_cu_param_" + tag + ".json");
    auto param = adef::make_and_setup_type<adef::RealMultipleControlParameter>(cfg, pm);
    for (std::size_t idx = 0; idx < values.size(); ++idx) {
        param->save(values[idx], idx);
    }
    return param;
}

// 1. AverageFitnessControlUpdate unit tests
bool test_average_fitness_control_update(const adef::PrototypeManager& pm)
{
    adef::AverageFitnessControlUpdate updater;
    auto repos = std::make_shared<TestRepository>();

    struct Case {
        std::string name;
        std::vector<double> fitnesses;
        double expected_mean;
    };

    const std::vector<Case> cases = {
        {"positive", {2.0, 4.0, 6.0, 8.0}, 5.0},
        {"negative", {-10.0, -20.0, -30.0}, -20.0},
        {"mixed", {-5.0, 0.0, 2.5, 10.5}, 2.0},
        {"single", {7.25}, 7.25},
        {"empty", {}, 0.0}
    };

    for (const auto& tc : cases) {
        repos->set_population(make_population_with_fitness(tc.fitnesses, pm, "avg_" + tc.name));
        auto spy = std::make_shared<SpyFunction>(true, "average");
        updater.update(repos, nullptr, spy);

        if (!require(spy->last_name() == "average" &&
                         spy->last_params().size() == 1 &&
                         approx_equal(spy->last_double(), tc.expected_mean),
                     "AverageFitnessControlUpdate failed for case \"" + tc.name + "\"")) {
            return false;
        }
    }

    // Rejecting target function throws std::runtime_error
    repos->set_population(make_population_with_fitness({1.0, 2.0}, pm, "avg_reject"));
    auto rejecting_fn = std::make_shared<SpyFunction>(true, "other_name");
    bool threw_runtime = false;
    try {
        updater.update(repos, nullptr, rejecting_fn);
    }
    catch (const std::runtime_error&) {
        threw_runtime = true;
    }
    if (!require(threw_runtime,
                 "Expected std::runtime_error when target function rejects \"average\"")) {
        return false;
    }

    return true;
}

// 2. MaxFitnessControlUpdate unit tests
bool test_max_fitness_control_update(const adef::PrototypeManager& pm)
{
    adef::MaxFitnessControlUpdate updater;
    auto repos = std::make_shared<TestRepository>();

    struct Case {
        std::string name;
        std::vector<double> fitnesses;
        double expected_max;
    };

    const std::vector<Case> cases = {
        {"positive", {1.5, 9.25, 3.0, 4.75}, 9.25},
        {"negative", {-12.0, -3.5, -7.25}, -3.5},
        {"mixed", {-4.0, 0.0, 6.5, -1.0}, 6.5},
        {"single", {-42.5}, -42.5},
        {"duplicate_max", {3.0, 8.5, 2.0, 8.5, 1.0}, 8.5}
    };

    for (const auto& tc : cases) {
        repos->set_population(make_population_with_fitness(tc.fitnesses, pm, "max_" + tc.name));
        auto spy = std::make_shared<SpyFunction>(true, "max");
        updater.update(repos, nullptr, spy);

        if (!require(spy->last_name() == "max" &&
                         spy->last_params().size() == 1 &&
                         approx_equal(spy->last_double(), tc.expected_max),
                     "MaxFitnessControlUpdate failed for case \"" + tc.name + "\"")) {
            return false;
        }
    }

    // Rejecting target function throws std::runtime_error
    repos->set_population(make_population_with_fitness({1.0, 5.0}, pm, "max_reject"));
    auto rejecting_fn = std::make_shared<SpyFunction>(true, "min");
    bool threw_runtime = false;
    try {
        updater.update(repos, nullptr, rejecting_fn);
    }
    catch (const std::runtime_error&) {
        threw_runtime = true;
    }
    if (!require(threw_runtime,
                 "Expected std::runtime_error when target function rejects \"max\"")) {
        return false;
    }

    // Locks down current empty-population behavior: std::out_of_range via Population::at(0)
    repos->set_population(make_population_with_fitness({}, pm, "max_empty"));
    auto spy = std::make_shared<SpyFunction>(true, "max");
    bool threw_out_of_range = false;
    try {
        updater.update(repos, nullptr, spy);
    }
    catch (const std::out_of_range&) {
        threw_out_of_range = true;
    }
    if (!require(threw_out_of_range,
                 "Expected std::out_of_range on empty population in MaxFitnessControlUpdate")) {
        return false;
    }

    return true;
}

// 3. MinFitnessControlUpdate unit tests
bool test_min_fitness_control_update(const adef::PrototypeManager& pm)
{
    adef::MinFitnessControlUpdate updater;
    auto repos = std::make_shared<TestRepository>();

    struct Case {
        std::string name;
        std::vector<double> fitnesses;
        double expected_min;
    };

    const std::vector<Case> cases = {
        {"positive", {4.5, 1.25, 9.0, 2.75}, 1.25},
        {"negative", {-2.0, -15.5, -7.25}, -15.5},
        {"mixed", {4.0, 0.0, -6.5, 1.0}, -6.5},
        {"single", {42.5}, 42.5},
        {"duplicate_min", {3.0, -8.5, 2.0, -8.5, 1.0}, -8.5}
    };

    for (const auto& tc : cases) {
        repos->set_population(make_population_with_fitness(tc.fitnesses, pm, "min_" + tc.name));
        auto spy = std::make_shared<SpyFunction>(true, "min");
        updater.update(repos, nullptr, spy);

        if (!require(spy->last_name() == "min" &&
                         spy->last_params().size() == 1 &&
                         approx_equal(spy->last_double(), tc.expected_min),
                     "MinFitnessControlUpdate failed for case \"" + tc.name + "\"")) {
            return false;
        }
    }

    // Rejecting target function throws std::runtime_error
    repos->set_population(make_population_with_fitness({1.0, 5.0}, pm, "min_reject"));
    auto rejecting_fn = std::make_shared<SpyFunction>(true, "max");
    bool threw_runtime = false;
    try {
        updater.update(repos, nullptr, rejecting_fn);
    }
    catch (const std::runtime_error&) {
        threw_runtime = true;
    }
    if (!require(threw_runtime,
                 "Expected std::runtime_error when target function rejects \"min\"")) {
        return false;
    }

    // Locks down current empty-population behavior: std::out_of_range via Population::at(0)
    repos->set_population(make_population_with_fitness({}, pm, "min_empty"));
    auto spy = std::make_shared<SpyFunction>(true, "min");
    bool threw_out_of_range = false;
    try {
        updater.update(repos, nullptr, spy);
    }
    catch (const std::out_of_range&) {
        threw_out_of_range = true;
    }
    if (!require(threw_out_of_range,
                 "Expected std::out_of_range on empty population in MinFitnessControlUpdate")) {
        return false;
    }

    return true;
}

// 4. CurrentFitnessControlUpdate unit tests
bool test_current_fitness_control_update(const adef::PrototypeManager& pm)
{
    adef::CurrentFitnessControlUpdate updater;
    const std::vector<double> fitnesses = {11.5, -2.25, 0.0, 99.75};

    // Normal per-individual recording
    {
        auto repos = std::make_shared<TestRepository>();
        repos->set_population(make_population_with_fitness(fitnesses, pm, "cur_normal"));
        auto spy = std::make_shared<SpyFunction>(true, "current");

        for (std::size_t idx = 0; idx < fitnesses.size(); ++idx) {
            repos->parameters()->store("target_index", idx);
            updater.update(repos, nullptr, spy);
            if (!require(spy->last_name() == "current" &&
                             spy->last_params().size() == 1 &&
                             approx_equal(spy->last_double(), fitnesses[idx]),
                         "CurrentFitnessControlUpdate mismatched fitness at index " +
                             std::to_string(idx))) {
                return false;
            }
        }
    }

    // Rejecting target function throws std::runtime_error
    {
        auto repos = std::make_shared<TestRepository>();
        repos->set_population(make_population_with_fitness(fitnesses, pm, "cur_reject"));
        repos->parameters()->store("target_index", std::size_t{1});
        auto rejecting_fn = std::make_shared<SpyFunction>(true, "average");

        bool threw_runtime = false;
        try {
            updater.update(repos, nullptr, rejecting_fn);
        }
        catch (const std::runtime_error&) {
            threw_runtime = true;
        }
        if (!require(threw_runtime,
                     "Expected std::runtime_error when target function rejects \"current\"")) {
            return false;
        }
    }

    // Missing "target_index" throws std::out_of_range
    {
        auto repos = std::make_shared<TestRepository>();
        repos->set_population(make_population_with_fitness(fitnesses, pm, "cur_missing"));
        auto spy = std::make_shared<SpyFunction>(true, "current");

        bool threw_missing = false;
        try {
            updater.update(repos, nullptr, spy);
        }
        catch (const std::out_of_range&) {
            threw_missing = true;
        }
        if (!require(threw_missing,
                     "Expected std::out_of_range when \"target_index\" is missing")) {
            return false;
        }
    }

    // Out-of-bounds "target_index" throws std::out_of_range
    {
        auto repos = std::make_shared<TestRepository>();
        repos->set_population(make_population_with_fitness(fitnesses, pm, "cur_oob"));
        auto spy = std::make_shared<SpyFunction>(true, "current");

        for (std::size_t bad_idx : {fitnesses.size(), fitnesses.size() + 10}) {
            repos->parameters()->store("target_index", bad_idx);
            bool threw_oob = false;
            try {
                updater.update(repos, nullptr, spy);
            }
            catch (const std::out_of_range&) {
                threw_oob = true;
            }
            if (!require(threw_oob,
                         "Expected std::out_of_range for out-of-bounds target_index " +
                             std::to_string(bad_idx))) {
                return false;
            }
        }
    }

    // Empty population with target_index == 0 throws std::out_of_range
    {
        auto repos = std::make_shared<TestRepository>();
        repos->set_population(make_population_with_fitness({}, pm, "cur_empty"));
        repos->parameters()->store("target_index", std::size_t{0});
        auto spy = std::make_shared<SpyFunction>(true, "current");

        bool threw_empty = false;
        try {
            updater.update(repos, nullptr, spy);
        }
        catch (const std::out_of_range&) {
            threw_empty = true;
        }
        if (!require(threw_empty,
                     "Expected std::out_of_range for empty population in CurrentFitnessControlUpdate")) {
            return false;
        }
    }

    return true;
}

// 5. SdeFControlUpdate unit tests
bool test_sdef_control_update(const adef::PrototypeManager& pm)
{
    adef::SdeFControlUpdate updater;
    const std::vector<double> param_values = {101.0, 102.0, 103.0, 104.0, 105.0, 106.0};
    auto param = make_multiple_parameter(param_values, pm, "sdef");

    // Fixed seed: mutually exclusive indices within [0, pop_size), exact match with ControlParameter::load(index)
    {
        auto repos = std::make_shared<TestRepository>();
        repos->set_population(make_population_with_fitness(
            std::vector<double>(param_values.size(), 0.0), pm, "sdef_fixed"));
        repos->set_random(std::make_shared<adef::Random>(1337u));

        adef::Random reference_rng(1337u);
        auto spy = std::make_shared<SpyFunction>(true, "", 3u);

        for (int trial = 0; trial < 25; ++trial) {
            updater.update(repos, param, spy);
            const auto recorded = spy->last_doubles();
            const auto expected_indices =
                reference_rng.sample_exclusive_indices(3u, 0, param_values.size());

            if (!require(recorded.size() == 3u,
                         "Expected SdeFControlUpdate to record number_of_parameters() values")) {
                return false;
            }

            std::set<std::size_t> unique_indices;
            for (std::size_t k = 0; k < expected_indices.size(); ++k) {
                const std::size_t idx = expected_indices[k];
                if (!require(idx < param_values.size() && unique_indices.insert(idx).second,
                             "Expected sampled indices to be in [0, pop_size) and mutually exclusive")) {
                    return false;
                }
                if (!require(approx_equal(recorded[k], param->load(idx)),
                             "Expected recorded value to match ControlParameter::load(index)")) {
                    return false;
                }
            }
        }
    }

    // Same-seed reproducibility across two independent runs
    {
        auto run_sequence = [&](adef::Random::SeedType seed) {
            auto repos = std::make_shared<TestRepository>();
            repos->set_population(make_population_with_fitness(
                std::vector<double>(param_values.size(), 0.0), pm, "sdef_rep"));
            repos->set_random(std::make_shared<adef::Random>(seed));
            auto spy = std::make_shared<SpyFunction>(true, "", 3u);

            std::vector<std::vector<double>> history;
            for (int step = 0; step < 10; ++step) {
                updater.update(repos, param, spy);
                history.push_back(spy->last_doubles());
            }
            return history;
        };

        const auto run_a = run_sequence(777u);
        const auto run_b = run_sequence(777u);
        if (!require(run_a == run_b,
                     "Expected SdeFControlUpdate to be reproducible under the same seed")) {
            return false;
        }
    }

    // Full-population draw (number_of_parameters == pop_size)
    {
        auto repos = std::make_shared<TestRepository>();
        repos->set_population(make_population_with_fitness(
            std::vector<double>(param_values.size(), 0.0), pm, "sdef_full"));
        repos->set_random(std::make_shared<adef::Random>(2026u));
        auto spy = std::make_shared<SpyFunction>(true, "", static_cast<unsigned int>(param_values.size()));

        updater.update(repos, param, spy);
        const auto recorded = spy->last_doubles();
        std::set<double> unique_vals(recorded.begin(), recorded.end());
        std::set<double> expected_vals(param_values.begin(), param_values.end());
        if (!require(recorded.size() == param_values.size() && unique_vals == expected_vals,
                     "Expected full-population SdeFControlUpdate to record a permutation of all parameter values")) {
            return false;
        }
    }

    // Target function rejecting record() throws std::runtime_error
    {
        auto repos = std::make_shared<TestRepository>();
        repos->set_population(make_population_with_fitness(
            std::vector<double>(param_values.size(), 0.0), pm, "sdef_reject"));
        repos->set_random(std::make_shared<adef::Random>(42u));
        auto rejecting_fn = std::make_shared<SpyFunction>(false, "", 3u);

        bool threw_runtime = false;
        try {
            updater.update(repos, param, rejecting_fn);
        }
        catch (const std::runtime_error&) {
            threw_runtime = true;
        }
        if (!require(threw_runtime,
                     "Expected std::runtime_error when target function rejects SdeFControlUpdate::record()")) {
            return false;
        }
    }

    return true;
}

// 6. GenerationControlUpdate supplemental error contract checks
bool test_generation_control_update_errors()
{
    adef::GenerationControlUpdate updater;

    // Normal recording under "generation"
    {
        auto repos = std::make_shared<TestRepository>();
        repos->parameters()->store("generation", 5u);
        auto spy = std::make_shared<SpyFunction>(true, "generation");

        updater.update(repos, nullptr, spy);
        if (!require(spy->last_name() == "generation" &&
                         spy->last_params().size() == 1 &&
                         spy->last_int() == 5,
                     "Expected GenerationControlUpdate to record generation 5")) {
            return false;
        }
    }

    // Rejecting target function throws std::runtime_error
    {
        auto repos = std::make_shared<TestRepository>();
        repos->parameters()->store("generation", 5u);
        auto rejecting_fn = std::make_shared<SpyFunction>(true, "other");

        bool threw_runtime = false;
        try {
            updater.update(repos, nullptr, rejecting_fn);
        }
        catch (const std::runtime_error&) {
            threw_runtime = true;
        }
        if (!require(threw_runtime,
                     "Expected std::runtime_error when target function rejects \"generation\"")) {
            return false;
        }
    }

    // Missing "generation" throws std::out_of_range
    {
        auto repos = std::make_shared<TestRepository>();
        auto spy = std::make_shared<SpyFunction>(true, "generation");

        bool threw_missing = false;
        try {
            updater.update(repos, nullptr, spy);
        }
        catch (const std::out_of_range&) {
            threw_missing = true;
        }
        if (!require(threw_missing,
                     "Expected std::out_of_range when \"generation\" parameter is missing")) {
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);

    if (!test_average_fitness_control_update(pm)) {
        return 1;
    }
    if (!test_max_fitness_control_update(pm)) {
        return 1;
    }
    if (!test_min_fitness_control_update(pm)) {
        return 1;
    }
    if (!test_current_fitness_control_update(pm)) {
        return 1;
    }
    if (!test_sdef_control_update(pm)) {
        return 1;
    }
    if (!test_generation_control_update_errors()) {
        return 1;
    }

    std::cout << "All ControlUpdate unit tests passed." << std::endl;
    return 0;
}
