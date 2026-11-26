#include <any>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "json.hpp"

#include "adef.h"
#include "Configuration.h"
#include "DECrossover.h"
#include "DEEnvironmentalSelection.h"
#include "DEMutation.h"
#include "Experiment.h"
#include "ExperimentalStatistics.h"
#include "Individual.h"
#include "Parameters.h"
#include "Population.h"
#include "Problem.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Repository.h"
#include "Statistics.h"
#include "System.h"
#include "cm/ControlMechanism.h"
#include "cm/IndirectControlMechanism.h"
#include "cm/cf/func/ConstantFunction.h"
#include "cm/cf/func/Function.h"
#include "cm/cf/func/LearningPeriodFunction.h"
#include "cm/cf/func/RouletteWheelSelectionFunction.h"
#include "cm/cp/MultipleControlParameter.h"
#include "cm/cp/SingleControlParameter.h"
#include "cm/cs/AllOffspringControlSelection.h"
#include "cm/cs/BetterOffspringControlSelection.h"

#ifndef ADEF_SOURCE_DIR
#define ADEF_SOURCE_DIR "."
#endif

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

    void clear_parameters()
    {
        parameters_.reset();
    }

    void set_population(std::shared_ptr<adef::Population> pop)
    {
        population_ = std::move(pop);
    }

    void set_offspring(std::shared_ptr<adef::Population> offspring)
    {
        offspring_ = std::move(offspring);
    }

    void set_random(std::shared_ptr<adef::Random> rng)
    {
        random_ = std::move(rng);
    }

    void set_problem(std::shared_ptr<adef::Problem> prob)
    {
        problem_ = std::move(prob);
    }

    void set_statistics(std::shared_ptr<adef::Statistics> stats)
    {
        statistics_ = std::move(stats);
    }
};

class CapturingStatistics : public adef::Statistics
{
public:
    void output_statistics(const std::string&) const override
    {
    }

    bool has_item(Item item) const
    {
        return statistics_all_individuals_.find(item) !=
               statistics_all_individuals_.end();
    }

    const GenerationList<AllIndividualsValueList>& recorded(Item item) const
    {
        return statistics_all_individuals_.at(item);
    }

private:
    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<CapturingStatistics>(*this);
    }
};

struct RunSnapshot {
    adef::Statistics::GenerationList<adef::Statistics::AllIndividualsValueList> f_history;
    adef::Statistics::GenerationList<adef::Statistics::AllIndividualsValueList> cr_history;
    std::vector<double> final_fitness;
    std::vector<double> best_fitness;
};

class CapturingExperimentalStatistics : public adef::ExperimentalStatistics
{
public:
    void take_statistics(std::shared_ptr<const adef::Repository> repos) override
    {
        adef::ExperimentalStatistics::take_statistics(repos);

        auto stats = std::dynamic_pointer_cast<const CapturingStatistics>(repos->statistics());
        if (!stats) {
            throw std::runtime_error("Expected CapturingStatistics in Repository");
        }

        RunSnapshot snap;
        if (stats->has_item(adef::Statistics::Item::f)) {
            snap.f_history = stats->recorded(adef::Statistics::Item::f);
        }
        if (stats->has_item(adef::Statistics::Item::cr)) {
            snap.cr_history = stats->recorded(adef::Statistics::Item::cr);
        }
        snap.best_fitness = stats->best_fitness();

        auto pop = repos->population();
        snap.final_fitness.reserve(pop->population_size());
        for (std::size_t idx = 0; idx < pop->population_size(); ++idx) {
            snap.final_fitness.push_back(pop->at(idx)->fitness());
        }
        runs_.push_back(std::move(snap));
    }

    void output_statistics_on_run(std::shared_ptr<const adef::Repository>,
                                  const std::string&) const override
    {
    }

    void output_statistics(const std::string&) override
    {
    }

    const std::vector<RunSnapshot>& runs() const { return runs_; }

private:
    std::vector<RunSnapshot> runs_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<CapturingExperimentalStatistics>(*this);
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
        "test_cs_pop_" + tag + ".json");
    auto pop = adef::make_and_setup_type<adef::Population>(cfg, pm);
    for (std::size_t idx = 0; idx < fitnesses.size(); ++idx) {
        pop->at(idx)->set_fitness_value(fitnesses[idx]);
    }
    return pop;
}

template<typename T>
struct RecordedOutcome {
    T value;
    double parent_fitness;
    double offspring_fitness;
    std::string name;
};

template<typename T>
class OutcomeSpyFunction : public adef::Function<T>
{
public:
    using Object = typename adef::Function<T>::Object;

    explicit OutcomeSpyFunction(bool accept_records = true,
                                std::string accepted_name = "object") :
        accept_records_(accept_records),
        accepted_name_(std::move(accepted_name))
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    Object generate() override
    {
        return Object{};
    }

    bool record(const std::vector<std::any>&,
                const std::string& = "") override
    {
        return false;
    }

    bool record(const std::vector<std::any>& params,
                std::shared_ptr<const adef::Individual> parent,
                std::shared_ptr<const adef::Individual> offspring,
                const std::string& name = "") override
    {
        if (!accept_records_ || name != accepted_name_) {
            return false;
        }
        outcomes_.push_back({
            std::any_cast<Object>(params.front()),
            parent->fitness(),
            offspring->fitness(),
            name
        });
        return true;
    }

    void update() override
    {
    }

    unsigned int number_of_parameters() const override
    {
        return 1;
    }

    const std::vector<RecordedOutcome<Object>>& outcomes() const
    {
        return outcomes_;
    }

private:
    bool accept_records_;
    std::string accepted_name_;
    std::vector<RecordedOutcome<Object>> outcomes_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<OutcomeSpyFunction>(*this);
    }
};

template<typename T>
bool verify_selection_alias_outcomes(const std::string& all_classname,
                                     const std::string& better_classname,
                                     const std::vector<T>& param_values,
                                     const adef::PrototypeManager& pm,
                                     const std::string& tag)
{
    auto all_sel = pm.make_type<adef::ControlSelection>(all_classname);
    auto better_sel = pm.make_type<adef::ControlSelection>(better_classname);
    if (!require(all_sel != nullptr && better_sel != nullptr,
                 tag + ": failed to instantiate ControlSelection from PrototypeManager")) {
        return false;
    }

    // Clone isolation check on ControlSelection itself
    auto cloned_all_sel = all_sel->clone();
    if (!require(cloned_all_sel != nullptr && cloned_all_sel.get() != all_sel.get(),
                 tag + ": clone() must return a distinct ControlSelection instance")) {
        return false;
    }

    auto param = std::make_shared<adef::MultipleControlParameter<T>>();
    auto param_cfg = load_json_config(
        "{\"number_of_objects\":" + std::to_string(param_values.size()) + "}",
        "test_cs_param_" + tag + ".json");
    param->setup(param_cfg, pm);
    for (std::size_t idx = 0; idx < param_values.size(); ++idx) {
        param->save(param_values[idx], idx);
    }

    // Index 0: strict improvement (5 < 10)
    // Index 1: tie (10 == 10)
    // Index 2: worse offspring (15 > 10)
    auto repos = std::make_shared<TestRepository>();
    repos->set_population(make_population_with_fitness({ 10.0, 10.0, 10.0 }, pm, tag + "_p"));
    repos->set_offspring(make_population_with_fitness({ 5.0, 10.0, 15.0 }, pm, tag + "_o"));

    auto spy_all = std::make_shared<OutcomeSpyFunction<T>>();
    auto spy_better = std::make_shared<OutcomeSpyFunction<T>>();

    for (std::size_t idx = 0; idx < 3; ++idx) {
        repos->parameters()->store("target_index", idx);
        cloned_all_sel->select(repos, param, spy_all);
        better_sel->select(repos, param, spy_better);
    }

    // AllOffspringControlSelection must record ALL 3 outcomes (win, tie, loss)
    if (!require(spy_all->outcomes().size() == 3,
                 tag + ": AllOffspringControlSelection must record improved, tied, and worse outcomes")) {
        return false;
    }
    const std::vector<double> expected_offspring_fit = { 5.0, 10.0, 15.0 };
    for (std::size_t idx = 0; idx < 3; ++idx) {
        const auto& rec = spy_all->outcomes()[idx];
        if (!require(rec.value == param_values[idx] &&
                         approx_equal(rec.parent_fitness, 10.0) &&
                         approx_equal(rec.offspring_fitness, expected_offspring_fit[idx]) &&
                         rec.name == "object",
                     tag + ": AllOffspringControlSelection recorded unexpected payload at index " +
                         std::to_string(idx))) {
            return false;
        }
    }

    // BetterOffspringControlSelection must still record ONLY index 0 (strict improvement)
    if (!require(spy_better->outcomes().size() == 1 &&
                     spy_better->outcomes()[0].value == param_values[0] &&
                     approx_equal(spy_better->outcomes()[0].parent_fitness, 10.0) &&
                     approx_equal(spy_better->outcomes()[0].offspring_fitness, 5.0) &&
                     spy_better->outcomes()[0].name == "object",
                 tag + ": BetterOffspringControlSelection must record strictly improved outcomes only")) {
        return false;
    }

    return true;
}

// ============================================================================
// 1. Unit tests for AllOffspringControlSelection (all 4 aliases + error contracts)
// ============================================================================
bool test_all_offspring_control_selection_unit(const adef::PrototypeManager& pm)
{
    if (!verify_selection_alias_outcomes<int>(
            "IntegerAllOffspringControlSelection",
            "IntegerBetterOffspringControlSelection",
            { 11, 22, 33 },
            pm,
            "int")) {
        return false;
    }
    if (!verify_selection_alias_outcomes<double>(
            "RealAllOffspringControlSelection",
            "RealBetterOffspringControlSelection",
            { 0.25, 0.50, 0.75 },
            pm,
            "real")) {
        return false;
    }

    auto int_fn0 = std::make_shared<adef::IntegerConstantFunction>();
    auto int_fn1 = std::make_shared<adef::IntegerConstantFunction>();
    auto int_fn2 = std::make_shared<adef::IntegerConstantFunction>();
    if (!verify_selection_alias_outcomes<std::shared_ptr<adef::IntegerFunction>>(
            "IntegerControlAllOffspringControlSelection",
            "IntegerControlBetterOffspringControlSelection",
            { int_fn0, int_fn1, int_fn2 },
            pm,
            "int_ctrl")) {
        return false;
    }

    auto real_fn0 = std::make_shared<adef::RealConstantFunction>();
    auto real_fn1 = std::make_shared<adef::RealConstantFunction>();
    auto real_fn2 = std::make_shared<adef::RealConstantFunction>();
    if (!verify_selection_alias_outcomes<std::shared_ptr<adef::RealFunction>>(
            "RealControlAllOffspringControlSelection",
            "RealControlBetterOffspringControlSelection",
            { real_fn0, real_fn1, real_fn2 },
            pm,
            "real_ctrl")) {
        return false;
    }

    // Error contracts on AllOffspringControlSelection
    adef::RealAllOffspringControlSelection sel;
    auto real_param = std::make_shared<adef::RealSingleControlParameter>();
    real_param->save(0.5, 0);
    auto spy = std::make_shared<OutcomeSpyFunction<double>>();

    // Null Repository
    {
        bool threw = false;
        try {
            sel.select(nullptr, real_param, spy);
        }
        catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!require(threw, "Expected std::invalid_argument on null Repository")) {
            return false;
        }
    }

    // Incomplete Repository (missing population / offspring / parameters)
    {
        auto incomplete = std::make_shared<TestRepository>();
        incomplete->set_population(make_population_with_fitness({ 1.0 }, pm, "err_p"));
        bool threw = false;
        try {
            sel.select(incomplete, real_param, spy);
        }
        catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!require(threw, "Expected std::invalid_argument on Repository missing offspring")) {
            return false;
        }

        auto no_params = std::make_shared<TestRepository>();
        no_params->clear_parameters();
        no_params->set_population(make_population_with_fitness({ 1.0 }, pm, "err_np_p"));
        no_params->set_offspring(make_population_with_fitness({ 2.0 }, pm, "err_np_o"));
        bool threw_no_params = false;
        try {
            sel.select(no_params, real_param, spy);
        }
        catch (const std::invalid_argument&) {
            threw_no_params = true;
        }
        if (!require(threw_no_params, "Expected std::invalid_argument on Repository missing parameters")) {
            return false;
        }
    }

    auto valid_repos = std::make_shared<TestRepository>();
    valid_repos->set_population(make_population_with_fitness({ 10.0 }, pm, "err_valid_p"));
    valid_repos->set_offspring(make_population_with_fitness({ 20.0 }, pm, "err_valid_o"));
    valid_repos->parameters()->store("target_index", std::size_t{ 0 });

    // Incompatible ControlParameter type
    {
        auto wrong_param = std::make_shared<adef::IntegerSingleControlParameter>();
        wrong_param->save(1, 0);
        bool threw = false;
        try {
            sel.select(valid_repos, wrong_param, spy);
        }
        catch (const std::runtime_error&) {
            threw = true;
        }
        if (!require(threw, "Expected std::runtime_error on incompatible ControlParameter")) {
            return false;
        }
    }

    // Null BaseFunction
    {
        bool threw = false;
        try {
            sel.select(valid_repos, real_param, nullptr);
        }
        catch (const std::runtime_error&) {
            threw = true;
        }
        if (!require(threw, "Expected std::runtime_error on null BaseFunction")) {
            return false;
        }
    }

    // Rejecting target BaseFunction (record returns false)
    {
        auto rejecting = std::make_shared<OutcomeSpyFunction<double>>(false);
        bool threw = false;
        try {
            sel.select(valid_repos, real_param, rejecting);
        }
        catch (const std::runtime_error&) {
            threw = true;
        }
        if (!require(threw, "Expected std::runtime_error when target Function rejects \"object\"")) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 2. Empirical success-rate recording into SuccessScoringFunction +
//    RouletteWheelSelectionFunction (including asymmetric trial counts)
// ============================================================================
bool test_success_rate_recording_and_relative_weighting(const adef::PrototypeManager& pm)
{
    auto build_rws_cm_lp2 = [&](const std::string& selection_classname,
                                unsigned int score_size,
                                const std::string& tag) {
        auto cfg = load_json_config(
            R"({
                "classname": "RealControlControlMechanism",
                "ControlRange": { "classname": "RealControlControlRange" },
                "ControlParameter": { "classname": "RealControlSingleControlParameter" },
                "ControlFunction": {
                    "classname": "SingleControlFunction",
                    "Function": {
                        "classname": "RealControlLearningPeriodFunction",
                        "learning_period": 2,
                        "object": {
                            "classname": "RealControlRouletteWheelSelectionFunction",
                            "scoring_function": { "classname": "SuccessScoringFunction" },
                            "score_size": )" + std::to_string(score_size) + R"(,
                            "object": [
                                { "classname": "RealConstantFunction", "object": 0.25 },
                                { "classname": "RealConstantFunction", "object": 0.75 }
                            ]
                        }
                    }
                },
                "ControlSelection": { "classname": ")" + selection_classname + R"(" },
                "ControlUpdate": [ { "classname": "GenerationControlUpdate" } ]
            })",
            "test_cs_rws_lp2_" + tag + ".json");
        return adef::make_and_setup_type<adef::RealControlControlMechanism>(cfg, pm);
    };

    auto drive_outcomes_and_sample = [&](const std::string& selection_classname,
                                         int cand0_wins,
                                         int cand0_ties,
                                         int cand0_losses,
                                         int cand1_wins,
                                         int cand1_losses,
                                         int draws,
                                         adef::Random::SeedType seed,
                                         const std::string& tag) -> int {
        auto cm = build_rws_cm_lp2(selection_classname, 100u, tag);
        auto repos = std::make_shared<TestRepository>();
        repos->set_random(std::make_shared<adef::Random>(seed));
        repos->set_population(make_population_with_fitness({ 10.0 }, pm, tag + "_p"));
        repos->set_offspring(make_population_with_fitness({ 10.0 }, pm, tag + "_o"));
        cm->init(repos);

        repos->parameters()->store("generation", 1u);
        repos->parameters()->store("target_index", std::size_t{ 0 });

        int rem0_win = cand0_wins;
        int rem0_tie = cand0_ties;
        int rem0_loss = cand0_losses;
        int rem1_win = cand1_wins;
        int rem1_loss = cand1_losses;

        while (rem0_win > 0 || rem0_tie > 0 || rem0_loss > 0 || rem1_win > 0 || rem1_loss > 0) {
            // gen = 1 does not hit learning_period = 2, so update_generation() resets already_generated only
            cm->update_generation(repos);
            auto picked = cm->generate(repos);
            const double val = picked->generate();
            if (approx_equal(val, 0.25)) {
                if (rem0_win > 0) {
                    repos->offspring()->at(0)->set_fitness_value(5.0);
                    cm->select(repos);
                    --rem0_win;
                }
                else if (rem0_tie > 0) {
                    repos->offspring()->at(0)->set_fitness_value(10.0);
                    cm->select(repos);
                    --rem0_tie;
                }
                else if (rem0_loss > 0) {
                    repos->offspring()->at(0)->set_fitness_value(15.0);
                    cm->select(repos);
                    --rem0_loss;
                }
            }
            else if (approx_equal(val, 0.75)) {
                if (rem1_win > 0) {
                    repos->offspring()->at(0)->set_fitness_value(5.0);
                    cm->select(repos);
                    --rem1_win;
                }
                else if (rem1_loss > 0) {
                    repos->offspring()->at(0)->set_fitness_value(15.0);
                    cm->select(repos);
                    --rem1_loss;
                }
            }
            else {
                throw std::runtime_error("Unexpected candidate value drawn");
            }
        }

        // Hit learning_period = 2 once to compute scores from all recorded outcomes
        repos->parameters()->store("generation", 2u);
        cm->update_generation(repos);

        // Switch to gen = 3 (non-multiple of 2) so update_generation() resets already_generated
        // without re-updating or clearing the learned scores
        repos->parameters()->store("generation", 3u);
        int cand0_count = 0;
        for (int i = 0; i < draws; ++i) {
            cm->update_generation(repos);
            auto picked = cm->generate(repos);
            if (approx_equal(picked->generate(), 0.25)) {
                ++cand0_count;
            }
        }
        return cand0_count;
    };

    // 2a. Equal trial counts: cand0 has 1 win + 1 tie + 2 losses (r0 = 0.25),
    //     cand1 has 3 wins + 1 loss (r1 = 0.75).
    //     - AllOffspringControlSelection => P(cand0) = 0.25 / (0.25 + 0.75) = 0.25.
    //     - BetterOffspringControlSelection => scores [1.0, 1.0] => collapses to 0.50.
    {
        constexpr int kDraws = 6000;
        const int all_cand0 = drive_outcomes_and_sample(
            "RealControlAllOffspringControlSelection",
            1, 1, 2, 3, 1, kDraws, 2026u, "eq_all");
        const int better_cand0 = drive_outcomes_and_sample(
            "RealControlBetterOffspringControlSelection",
            1, 1, 2, 3, 1, kDraws, 2026u, "eq_better");

        // AllOffspring: N=6000, p=0.25 => mean=1500, stddev~=33.54; [1290, 1710] is +/-6.26 sigma.
        if (!require(all_cand0 >= 1290 && all_cand0 <= 1710,
                     "AllOffspringControlSelection expected ~25% for cand0 (1/4 vs 3/4), got " +
                         std::to_string(all_cand0) + "/" + std::to_string(kDraws))) {
            return false;
        }
        // BetterOffspring (backward compatibility): N=6000, p=0.50 => mean=3000, stddev~=38.73; [2750, 3250] is +/-6.45 sigma.
        if (!require(better_cand0 >= 2750 && better_cand0 <= 3250,
                     "BetterOffspringControlSelection expected legacy 50/50 collapse when both candidates win, got " +
                         std::to_string(better_cand0) + "/" + std::to_string(kDraws))) {
            return false;
        }
    }

    // 2b. Asymmetric trial counts (SaNSDE relative success-rate formula):
    //     cand0: ns0=2, nf0=6 (8 trials -> r0 = 0.25)
    //     cand1: ns1=3, nf1=1 (4 trials -> r1 = 0.75)
    //     fp = r0 / (r0 + r1) = 0.25 (NOT raw-win ratio 2/5 = 0.40 and NOT 0.50).
    {
        constexpr int kDraws = 8000;
        const int asym_cand0 = drive_outcomes_and_sample(
            "RealControlAllOffspringControlSelection",
            2, 2, 4, 3, 1, kDraws, 4096u, "asym_all");

        // N=8000, p=0.25 => mean=2000, stddev~=38.73; [1750, 2250] is +/-6.45 sigma
        // (and >24 sigma away from raw-win 3200 and collapsed 4000).
        if (!require(asym_cand0 >= 1750 && asym_cand0 <= 2250,
                     "Asymmetric trial counts expected fp=0.25 (2000/8000), got " +
                         std::to_string(asym_cand0) + "/" + std::to_string(kDraws))) {
            return false;
        }
    }

    // 2c. All-failure learning period: both candidates record only failures => scores [0, 0] => uniform 50/50 fallback.
    {
        constexpr int kDraws = 4000;
        const int all_fail_cand0 = drive_outcomes_and_sample(
            "RealControlAllOffspringControlSelection",
            0, 2, 2, 0, 4, kDraws, 5050u, "all_fail");

        // N=4000, p=0.50 => mean=2000, stddev~=31.62; [1790, 2210] is +/-6.64 sigma.
        if (!require(all_fail_cand0 >= 1790 && all_fail_cand0 <= 2210,
                     "All-failure period expected uniform 50/50 fallback, got " +
                         std::to_string(all_fail_cand0) + "/" + std::to_string(kDraws))) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 3. End-to-end SaNSDE adaptive weighting across learning periods,
//    same-seed determinism, and clone() isolation
// ============================================================================
bool test_sansde_end_to_end_adaptive_weighting_and_isolation(const adef::PrototypeManager& pm)
{
    // 3a. Controlled DEMutation + DEEnvironmentalSelection run across 3 learning periods
    //     demonstrating non-degenerate adaptive weighting (~20%/80%) and reversal (~80%/20%).
    {
        adef::PrototypeManager local_pm;
        adef::register_type(local_pm);
        local_pm.register_type("CapturingStatistics", std::make_shared<CapturingStatistics>());

        constexpr std::size_t kPop = 50;
        constexpr unsigned int kLP = 20;

        auto prob_cfg = load_json_config(
            R"({"classname":"Sphere","dimension_of_decision_space":4})",
            "test_cs_e2e_prob.json");
        auto prob = adef::make_and_setup_type<adef::Problem>(prob_cfg, local_pm);

        auto pop_cfg = load_json_config(
            R"({"classname":"Population","population_size":50,"Individual":{"classname":"Individual"}})",
            "test_cs_e2e_pop.json");
        auto pop = adef::make_and_setup_type<adef::Population>(pop_cfg, local_pm);

        auto stats_cfg = load_json_config(
            R"({"classname":"CapturingStatistics","item":["f"]})",
            "test_cs_e2e_stats.json");
        auto stats = adef::make_and_setup_type<CapturingStatistics>(stats_cfg, local_pm);

        auto mut_cfg = load_json_config(R"({
            "classname": "DEMutation",
            "DEVariantX": { "classname": "DERand" },
            "DEVariantY": { "classname": "DEVariantY", "number_of_difference_vector": 1 },
            "F": {
                "classname": "RealIndirectControlMechanism",
                "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
                "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 50 },
                "ControlSelection": { "classname": "NonInfoControlSelection" },
                "ControlUpdate": [],
                "ControlMechanism": {
                    "classname": "RealControlControlMechanism",
                    "ControlRange": { "classname": "RealControlControlRange" },
                    "ControlParameter": { "classname": "RealControlMultipleControlParameter", "number_of_objects": 50 },
                    "ControlFunction": {
                        "classname": "SingleControlFunction",
                        "Function": {
                            "classname": "RealControlLearningPeriodFunction",
                            "learning_period": 20,
                            "object": {
                                "classname": "RealControlRouletteWheelSelectionFunction",
                                "scoring_function": { "classname": "SuccessScoringFunction" },
                                "score_size": 1000,
                                "object": [
                                    { "classname": "RealConstantFunction", "object": 0.25 },
                                    { "classname": "RealConstantFunction", "object": 0.75 }
                                ]
                            }
                        }
                    },
                    "ControlSelection": { "classname": "RealControlAllOffspringControlSelection" },
                    "ControlUpdate": [ { "classname": "GenerationControlUpdate" } ]
                }
            }
        })", "test_cs_e2e_mut.json");
        auto mutation = adef::make_and_setup_type<adef::DEMutation>(mut_cfg, local_pm);

        auto cr_cfg = load_json_config(R"({
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
        })", "test_cs_e2e_cr.json");
        auto crossover = adef::make_and_setup_type<adef::DECrossover>(cr_cfg, local_pm);
        auto env_sel = std::make_shared<adef::DEEnvironmentalSelection>();

        auto repos = std::make_shared<TestRepository>();
        repos->set_random(std::make_shared<adef::Random>(2026u));
        repos->set_problem(prob);
        repos->set_population(pop);
        repos->set_statistics(stats);
        prob->init(repos);
        pop->init(repos);
        repos->set_offspring(pop->empty_clone());
        mutation->init(repos);
        crossover->init(repos);
        env_sel->init(repos);

        int lp2_cand0_draws = 0;
        int lp3_cand0_draws = 0;
        int cand0_trial_lp1 = 0;
        int cand1_trial_lp1 = 0;
        int cand0_trial_lp2 = 0;
        int cand1_trial_lp2 = 0;

        // Run gens 1..59 (3 learning periods of 20 gens):
        // - LP1 (gens 1..19): cand0 (0.25) succeeds 20% of the time; cand1 (0.75) succeeds 80% of the time.
        // - LP2 (gens 20..39): expect cand0 drawn ~20% of the time; reverse rewards (cand0 80%, cand1 20%).
        // - LP3 (gens 40..59): expect cand0 drawn ~80% of the time.
        for (unsigned int gen = 1; gen < 3 * kLP; ++gen) {
            repos->parameters()->store("generation", gen);
            mutation->mutate(repos);

            const auto& gen_f = stats->recorded(adef::Statistics::Item::f).at(gen - 1);
            for (std::size_t idx = 0; idx < kPop; ++idx) {
                const bool is_cand0 = approx_equal(gen_f[idx], 0.25);
                if (!require(is_cand0 || approx_equal(gen_f[idx], 0.75),
                             "Unexpected F value in SaNSDE e2e adaptive test")) {
                    return false;
                }

                if (gen >= kLP && gen < 2 * kLP && is_cand0) {
                    ++lp2_cand0_draws;
                }
                else if (gen >= 2 * kLP && is_cand0) {
                    ++lp3_cand0_draws;
                }

                repos->population()->at(idx)->set_fitness_value(10.0);
                bool win = false;
                if (gen < kLP) {
                    if (is_cand0) {
                        win = (cand0_trial_lp1++ % 5 == 0); // 20% win rate
                    }
                    else {
                        win = (cand1_trial_lp1++ % 5 != 0); // 80% win rate
                    }
                }
                else if (gen < 2 * kLP) {
                    if (is_cand0) {
                        win = (cand0_trial_lp2++ % 5 != 0); // 80% win rate
                    }
                    else {
                        win = (cand1_trial_lp2++ % 5 == 0); // 20% win rate
                    }
                }
                repos->offspring()->at(idx)->set_fitness_value(win ? 5.0 : 15.0);
            }
            env_sel->select(repos);
        }

        // LP2: 20 gens * 50 = 1000 draws, expected p=0.20 => mean=200, stddev~=12.65;
        // [115, 285] is +/-6.7 sigma (non-zero, non-degenerate, far from 500).
        if (!require(lp2_cand0_draws >= 115 && lp2_cand0_draws <= 285,
                     "SaNSDE e2e LP2 expected ~20% draws for cand0 (200/1000), got " +
                         std::to_string(lp2_cand0_draws) + "/1000")) {
            return false;
        }
        // LP3: 20 gens * 50 = 1000 draws, expected p=0.80 => mean=800, stddev~=12.65;
        // [715, 885] is +/-6.7 sigma (reversed dominance, far from 500 and 1000).
        if (!require(lp3_cand0_draws >= 715 && lp3_cand0_draws <= 885,
                     "SaNSDE e2e LP3 expected ~80% draws for cand0 (800/1000), got " +
                         std::to_string(lp3_cand0_draws) + "/1000")) {
            return false;
        }
    }

    // 3b. Full SaNSDE Experiment (Normal(0.5, 0.3) vs Cauchy(0.0, 1.0)) determinism + multi-run independence
    {
        auto run_sansde_exp = [](adef::Random::SeedType seed, const std::string& tag) {
            adef::PrototypeManager local_pm;
            adef::register_type(local_pm);
            local_pm.register_type("CapturingStatistics", std::make_shared<CapturingStatistics>());
            local_pm.register_type("CapturingExperimentalStatistics",
                                   std::make_shared<CapturingExperimentalStatistics>());

            auto cfg = load_json_config(R"({
                "classname": "Experiment",
                "Random": { "classname": "Random", "seed": )" + std::to_string(seed) + R"( },
                "number_of_runs": 2,
                "ExperimentalStatistics": { "classname": "CapturingExperimentalStatistics" },
                "Repository": {
                    "classname": "Repository",
                    "algorithm_name": "SaNSDE",
                    "Evolution": { "classname": "Evolution", "max_generation": 15 },
                    "Problem": { "classname": "Sphere", "dimension_of_decision_space": 5 },
                    "Statistics": {
                        "classname": "CapturingStatistics",
                        "success_error": 0.0,
                        "item": ["parents", "offspring", "f", "cr"]
                    },
                    "Parameters": { "classname": "Parameters" },
                    "Initializer": { "classname": "Initializer" },
                    "Evaluator": { "classname": "Evaluator" },
                    "Population": {
                        "classname": "Population",
                        "population_size": 12,
                        "Individual": { "classname": "Individual" }
                    },
                    "Reproduction": { "classname": "DEReproduction" },
                    "EnvironmentalSelection": { "classname": "DEEnvironmentalSelection" },
                    "Mutation": {
                        "classname": "DEMutation",
                        "DEVariantX": { "classname": "DERand" },
                        "DEVariantY": { "classname": "DEVariantY", "number_of_difference_vector": 1 },
                        "F": {
                            "classname": "RealIndirectControlMechanism",
                            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
                            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 12 },
                            "ControlSelection": { "classname": "NonInfoControlSelection" },
                            "ControlUpdate": [],
                            "ControlMechanism": {
                                "classname": "RealControlControlMechanism",
                                "ControlRange": { "classname": "RealControlControlRange" },
                                "ControlParameter": { "classname": "RealControlMultipleControlParameter", "number_of_objects": 12 },
                                "ControlFunction": {
                                    "classname": "SingleControlFunction",
                                    "Function": {
                                        "classname": "RealControlLearningPeriodFunction",
                                        "learning_period": 5,
                                        "object": {
                                            "classname": "RealControlRouletteWheelSelectionFunction",
                                            "scoring_function": { "classname": "SuccessScoringFunction" },
                                            "score_size": 50,
                                            "object": [
                                                {
                                                    "classname": "RealNormalDisFunction",
                                                    "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                                                    "stddev": { "classname": "RealConstantFunction", "object": 0.3 }
                                                },
                                                {
                                                    "classname": "RealCauchyDisFunction",
                                                    "location": { "classname": "RealConstantFunction", "object": 0.0 },
                                                    "scale": { "classname": "RealConstantFunction", "object": 1.0 }
                                                }
                                            ]
                                        }
                                    }
                                },
                                "ControlSelection": { "classname": "RealControlAllOffspringControlSelection" },
                                "ControlUpdate": [ { "classname": "GenerationControlUpdate" } ]
                            }
                        }
                    },
                    "Crossover": {
                        "classname": "DECrossover",
                        "DEVariantZ": { "classname": "DEBinomial" },
                        "CR": {
                            "classname": "SadeCrControlMechanism",
                            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
                            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 12 },
                            "ControlFunction": {
                                "classname": "SingleControlFunction",
                                "Function": {
                                    "classname": "RealNormalDisFunction",
                                    "mean": {
                                        "classname": "RealLearningPeriodFunction",
                                        "learning_period": 5,
                                        "object": {
                                            "classname": "WeightedAverageFunction",
                                            "scoring_function": { "classname": "ImprovedPercentageScoringFunction" },
                                            "object_size": 12,
                                            "initial_value": 0.5
                                        }
                                    },
                                    "stddev": { "classname": "RealConstantFunction", "object": 0.1 }
                                }
                            },
                            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
                            "ControlUpdate": [ { "classname": "GenerationControlUpdate" } ]
                        }
                    },
                    "Repair": { "classname": "ReflectiveRepair" }
                }
            })", "test_cs_sansde_exp_" + tag + ".json");

            auto exp = adef::make_and_setup_type<adef::Experiment>(cfg, local_pm);
            exp->run();
            auto exp_stats = std::dynamic_pointer_cast<const CapturingExperimentalStatistics>(
                exp->statistics());
            return exp_stats->runs();
        };

        const auto runs_a = run_sansde_exp(2026u, "a");
        const auto runs_b = run_sansde_exp(2026u, "b");
        if (!require(runs_a.size() == 2 && runs_b.size() == 2,
                     "SaNSDE Experiment expected 2 runs")) {
            return false;
        }
        for (std::size_t r = 0; r < 2; ++r) {
            if (!require(runs_a[r].f_history == runs_b[r].f_history &&
                             runs_a[r].cr_history == runs_b[r].cr_history &&
                             runs_a[r].final_fitness == runs_b[r].final_fitness &&
                             runs_a[r].best_fitness == runs_b[r].best_fitness,
                         "SaNSDE Experiment same-seed determinism failed on run " + std::to_string(r))) {
                return false;
            }
        }
        if (!require(runs_a[0].final_fitness != runs_a[1].final_fitness,
                     "SaNSDE Experiment multi-run independence failed")) {
            return false;
        }
    }

    // 3c. Prototype clone() isolation on RealIndirectControlMechanism with RealControlAllOffspringControlSelection
    {
        auto cfg = load_json_config(R"({
            "classname": "RealIndirectControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 4 },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": [],
            "ControlMechanism": {
                "classname": "RealControlControlMechanism",
                "ControlRange": { "classname": "RealControlControlRange" },
                "ControlParameter": { "classname": "RealControlMultipleControlParameter", "number_of_objects": 4 },
                "ControlFunction": {
                    "classname": "SingleControlFunction",
                    "Function": {
                        "classname": "RealControlLearningPeriodFunction",
                        "learning_period": 2,
                        "object": {
                            "classname": "RealControlRouletteWheelSelectionFunction",
                            "scoring_function": { "classname": "SuccessScoringFunction" },
                            "score_size": 50,
                            "object": [
                                {
                                    "classname": "RealNormalDisFunction",
                                    "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                                    "stddev": { "classname": "RealConstantFunction", "object": 0.3 }
                                },
                                {
                                    "classname": "RealCauchyDisFunction",
                                    "location": { "classname": "RealConstantFunction", "object": 0.0 },
                                    "scale": { "classname": "RealConstantFunction", "object": 1.0 }
                                }
                            ]
                        }
                    }
                },
                "ControlSelection": { "classname": "RealControlAllOffspringControlSelection" },
                "ControlUpdate": [ { "classname": "GenerationControlUpdate" } ]
            }
        })", "test_cs_clone_indirect.json");

        auto proto = adef::make_and_setup_type<adef::RealIndirectControlMechanism>(cfg, pm);

        auto drive_cm = [&](std::shared_ptr<adef::RealIndirectControlMechanism> cm,
                            adef::Random::SeedType seed,
                            const std::vector<double>& offspring_fit,
                            const std::string& tag) {
            auto repos = std::make_shared<TestRepository>();
            repos->set_random(std::make_shared<adef::Random>(seed));
            repos->set_population(make_population_with_fitness({ 10.0, 10.0, 10.0, 10.0 }, pm, tag + "_p"));
            repos->set_offspring(make_population_with_fitness(offspring_fit, pm, tag + "_o"));
            cm->init(repos);

            std::vector<double> generated;
            for (unsigned int gen = 1; gen <= 6; ++gen) {
                repos->parameters()->store("generation", gen);
                cm->update_generation(repos);
                for (std::size_t idx = 0; idx < 4; ++idx) {
                    repos->parameters()->store("target_index", idx);
                    cm->update_individual(repos);
                    generated.push_back(cm->generate(repos));
                }
                for (std::size_t idx = 0; idx < 4; ++idx) {
                    repos->parameters()->store("target_index", idx);
                    cm->select(repos);
                }
            }
            return generated;
        };

        auto ref_clone = proto->clone();
        const auto ref_out = drive_cm(ref_clone, 1234u, { 5.0, 15.0, 5.0, 15.0 }, "iso_ref");

        auto mutator_clone = proto->clone();
        (void)drive_cm(mutator_clone, 9999u, { 20.0, 20.0, 20.0, 20.0 }, "iso_mut");

        auto post_clone = proto->clone();
        const auto post_out = drive_cm(post_clone, 1234u, { 5.0, 15.0, 5.0, 15.0 }, "iso_post");
        const auto proto_out = drive_cm(proto, 1234u, { 5.0, 15.0, 5.0, 15.0 }, "iso_proto");

        if (!require(ref_out == post_out && ref_out == proto_out,
                     "Mutating cloned SaNSDE IndirectControlMechanism leaked state into prototype")) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 4. Verify example/SaNSDE.json, example/fully_exp.json (all 23 SaNSDE entries),
//    and backward compatibility of all other example configurations
// ============================================================================
bool test_example_configs_synchronized_and_valid(const adef::PrototypeManager& pm)
{
    const std::string source_dir = ADEF_SOURCE_DIR;

    auto read_json_file = [](const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open JSON file: " + path);
        }
        nlohmann::json j;
        ifs >> j;
        return j;
    };

    const std::string sansde_path = source_dir + "/example/SaNSDE.json";
    const std::string fully_exp_path = source_dir + "/example/fully_exp.json";

    const auto sansde_json = read_json_file(sansde_path);
    const auto& sansde_repo = sansde_json.at("System").at("Experiment").at(0).at("Repository");
    const auto& sansde_mut = sansde_repo.at("Mutation");
    const auto& sansde_cr = sansde_repo.at("Crossover");

    // Verify F inner ControlMechanism fields in example/SaNSDE.json
    const auto& inner_cm = sansde_mut.at("F").at("ControlMechanism");
    if (!require(inner_cm.at("ControlSelection").at("classname") == "RealControlAllOffspringControlSelection",
                 "example/SaNSDE.json F inner ControlSelection must be RealControlAllOffspringControlSelection")) {
        return false;
    }
    const auto& inner_updates = inner_cm.at("ControlUpdate");
    if (!require(inner_updates.is_array() && inner_updates.size() == 1 &&
                     inner_updates.at(0).at("classname") == "GenerationControlUpdate",
                 "example/SaNSDE.json F inner ControlUpdate must include GenerationControlUpdate")) {
        return false;
    }
    const auto& rws_objects = inner_cm.at("ControlFunction").at("Function").at("object").at("object");
    if (!require(rws_objects.is_array() && rws_objects.size() == 2 &&
                     rws_objects.at(0).at("classname") == "RealNormalDisFunction" &&
                     approx_equal(rws_objects.at(0).at("mean").at("object").get<double>(), 0.5) &&
                     approx_equal(rws_objects.at(0).at("stddev").at("object").get<double>(), 0.3) &&
                     rws_objects.at(1).at("classname") == "RealCauchyDisFunction" &&
                     approx_equal(rws_objects.at(1).at("location").at("object").get<double>(), 0.0) &&
                     approx_equal(rws_objects.at(1).at("scale").at("object").get<double>(), 1.0),
                 "example/SaNSDE.json F distributions must match N(0.5, 0.3) and Cauchy(0.0, 1.0)")) {
        return false;
    }

    // Verify CR in example/SaNSDE.json preserves BetterOffspringControlSelection + GenerationControlUpdate
    const auto& cr_cm = sansde_cr.at("CR");
    if (!require(cr_cm.at("classname") == "SadeCrControlMechanism" &&
                     cr_cm.at("ControlSelection").at("classname") == "RealBetterOffspringControlSelection" &&
                     cr_cm.at("ControlUpdate").is_array() &&
                     cr_cm.at("ControlUpdate").size() == 1 &&
                     cr_cm.at("ControlUpdate").at(0).at("classname") == "GenerationControlUpdate",
                 "example/SaNSDE.json CR configuration must preserve SadeCr + BetterOffspring + GenerationControlUpdate")) {
        return false;
    }

    // Verify all 23 SaNSDE entries in example/fully_exp.json match example/SaNSDE.json
    const auto fully_json = read_json_file(fully_exp_path);
    const auto& experiments = fully_json.at("System").at("Experiment");
    std::size_t sansde_count = 0;
    for (std::size_t idx = 0; idx < experiments.size(); ++idx) {
        const auto& repo = experiments.at(idx).at("Repository");
        if (repo.at("algorithm_name") == "SaNSDE") {
            ++sansde_count;
            if (!require(repo.at("Mutation") == sansde_mut && repo.at("Crossover") == sansde_cr,
                         "example/fully_exp.json SaNSDE entry #" + std::to_string(sansde_count) +
                             " (" + repo.at("Problem").at("classname").get<std::string>() +
                             ") is out of sync with example/SaNSDE.json")) {
                return false;
            }
        }
    }
    if (!require(sansde_count == 23,
                 "Expected exactly 23 SaNSDE entries in example/fully_exp.json, found " +
                     std::to_string(sansde_count))) {
        return false;
    }

    // Verify all standalone example configurations and default config.json load and setup cleanly
    const std::vector<std::string> config_files = {
        source_dir + "/config.json",
        source_dir + "/example/DEPD.json",
        source_dir + "/example/ISADE.json",
        source_dir + "/example/jDE.json",
        source_dir + "/example/NSDE.json",
        source_dir + "/example/SaDE.json",
        source_dir + "/example/SaNSDE.json",
        source_dir + "/example/SDE.json",
        source_dir + "/example/rand1bin_F0.5CR0.5.json"
    };
    for (const auto& path : config_files) {
        adef::Configuration cfg;
        if (!require(cfg.load_config(path), "Failed to load configuration file: " + path)) {
            return false;
        }
        auto sys = adef::make_and_setup_type<adef::System>("System", cfg, pm);
        if (!require(sys != nullptr, "Failed to setup System from configuration file: " + path)) {
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

    if (!test_all_offspring_control_selection_unit(pm)) {
        return 1;
    }
    if (!test_success_rate_recording_and_relative_weighting(pm)) {
        return 1;
    }
    if (!test_sansde_end_to_end_adaptive_weighting_and_isolation(pm)) {
        return 1;
    }
    if (!test_example_configs_synchronized_and_valid(pm)) {
        return 1;
    }

    std::cout << "All ControlSelection and SaNSDE F self-adaptation tests passed." << std::endl;
    return 0;
}
