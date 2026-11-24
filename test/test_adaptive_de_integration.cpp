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

#include "adef.h"
#include "Configuration.h"
#include "Experiment.h"
#include "ExperimentalStatistics.h"
#include "Individual.h"
#include "Population.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Repository.h"
#include "Statistics.h"
#include "util/math_tool.h"

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

struct AlgorithmCase {
    std::string name;
    double f_lower;
    double f_upper;
    double cr_lower;
    double cr_upper;
    std::string f_json;
    std::string cr_json;
};

constexpr std::size_t kPopSize = 10;
constexpr unsigned int kMaxGen = 12; // spans 3 learning periods of 4 generations
constexpr unsigned int kNumRuns = 2;

std::string build_experiment_json(const AlgorithmCase& alg,
                                  adef::Random::SeedType seed)
{
    return R"({
        "classname": "Experiment",
        "Random": { "classname": "Random", "seed": )" + std::to_string(seed) + R"( },
        "number_of_runs": )" + std::to_string(kNumRuns) + R"(,
        "ExperimentalStatistics": { "classname": "CapturingExperimentalStatistics" },
        "Repository": {
            "classname": "Repository",
            "algorithm_name": ")" + alg.name + R"(",
            "Evolution": { "classname": "Evolution", "max_generation": )" + std::to_string(kMaxGen) + R"( },
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
                "population_size": )" + std::to_string(kPopSize) + R"(,
                "Individual": { "classname": "Individual" }
            },
            "Reproduction": { "classname": "DEReproduction" },
            "EnvironmentalSelection": { "classname": "DEEnvironmentalSelection" },
            "Mutation": {
                "classname": "DEMutation",
                "DEVariantX": { "classname": "DERand" },
                "DEVariantY": { "classname": "DEVariantY", "number_of_difference_vector": 1 },
                "F": )" + alg.f_json + R"(
            },
            "Crossover": {
                "classname": "DECrossover",
                "DEVariantZ": { "classname": "DEBinomial" },
                "CR": )" + alg.cr_json + R"(
            },
            "Repair": { "classname": "ReflectiveRepair" }
        }
    })";
}

std::vector<RunSnapshot> run_experiment(const AlgorithmCase& alg,
                                        adef::Random::SeedType seed,
                                        const std::string& tag)
{
    adef::PrototypeManager pm;
    adef::register_type(pm);
    pm.register_type("CapturingStatistics", std::make_shared<CapturingStatistics>());
    pm.register_type("CapturingExperimentalStatistics",
                     std::make_shared<CapturingExperimentalStatistics>());

    auto cfg = load_json_config(build_experiment_json(alg, seed),
                                "test_integ_" + alg.name + "_" + tag + ".json");
    auto exp = adef::make_and_setup_type<adef::Experiment>(cfg, pm);
    exp->run();

    auto stats = std::dynamic_pointer_cast<const CapturingExperimentalStatistics>(
        exp->statistics());
    if (!stats) {
        throw std::runtime_error("Missing CapturingExperimentalStatistics after run");
    }
    return stats->runs();
}

bool verify_algorithm_case(const AlgorithmCase& alg)
{
    const auto runs_a = run_experiment(alg, 2026u, "run_a");
    const auto runs_b = run_experiment(alg, 2026u, "run_b");

    if (!require(runs_a.size() == kNumRuns && runs_b.size() == kNumRuns,
                 alg.name + ": unexpected number of recorded runs")) {
        return false;
    }

    for (std::size_t run_idx = 0; run_idx < kNumRuns; ++run_idx) {
        const auto& ra = runs_a[run_idx];
        const auto& rb = runs_b[run_idx];

        if (!require(ra.f_history.size() == kMaxGen &&
                         ra.cr_history.size() == kMaxGen &&
                         ra.final_fitness.size() == kPopSize,
                     alg.name + ": expected full " + std::to_string(kMaxGen) +
                         " generations and " + std::to_string(kPopSize) +
                         " individuals on run " + std::to_string(run_idx))) {
            return false;
        }

        // Verify finiteness and configured bounds on every recorded F and CR
        for (std::size_t gen = 0; gen < kMaxGen; ++gen) {
            if (!require(ra.f_history[gen].size() == kPopSize &&
                             ra.cr_history[gen].size() == kPopSize,
                         alg.name + ": unexpected per-generation population size")) {
                return false;
            }
            for (std::size_t ind = 0; ind < kPopSize; ++ind) {
                const double f_val = ra.f_history[gen][ind];
                const double cr_val = ra.cr_history[gen][ind];

                if (!require(std::isfinite(f_val) &&
                                 adef::is_less_equal(alg.f_lower, f_val) &&
                                 adef::is_less_equal(f_val, alg.f_upper),
                             alg.name + ": F out of bounds or non-finite (" +
                                 std::to_string(f_val) + ")")) {
                    return false;
                }
                if (!require(std::isfinite(cr_val) &&
                                 adef::is_less_equal(alg.cr_lower, cr_val) &&
                                 adef::is_less_equal(cr_val, alg.cr_upper),
                             alg.name + ": CR out of bounds or non-finite (" +
                                 std::to_string(cr_val) + ")")) {
                    return false;
                }
            }
        }

        // Verify finiteness and non-negativity of Sphere final population fitness
        for (std::size_t ind = 0; ind < kPopSize; ++ind) {
            const double fit = ra.final_fitness[ind];
            if (!require(std::isfinite(fit) && fit >= 0.0,
                         alg.name + ": non-finite or negative Sphere fitness")) {
                return false;
            }
        }

        // Same-seed reproducibility
        if (!require(ra.f_history == rb.f_history &&
                         ra.cr_history == rb.cr_history &&
                         ra.final_fitness == rb.final_fitness &&
                         ra.best_fitness == rb.best_fitness,
                     alg.name + ": same-seed reproducibility failed on run " +
                         std::to_string(run_idx))) {
            return false;
        }
    }

    // Multi-run independence within a single Experiment
    if (!require(runs_a[0].final_fitness != runs_a[1].final_fitness,
                 alg.name + ": expected distinct per-run final populations within one Experiment")) {
        return false;
    }

    return true;
}

std::vector<AlgorithmCase> make_algorithm_cases()
{
    std::vector<AlgorithmCase> cases;

    // 1. DEPD: SingleControlFunction + SingleControlParameter + Min/MaxFitnessControlUpdate
    cases.push_back({
        "DEPD",
        0.4, 1.0,
        0.0, 1.0,
        R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.4, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealSingleControlParameter" },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": {
                    "classname": "DepdFFunction",
                    "min": { "classname": "RealVariableFunction" },
                    "max": { "classname": "RealVariableFunction" },
                    "lower_bound": { "classname": "RealConstantFunction", "object": 0.4 }
                }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": [
                { "classname": "MinFitnessControlUpdate" },
                { "classname": "MaxFitnessControlUpdate" }
            ]
        })",
        R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealSingleControlParameter" },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": { "classname": "RealConstantFunction", "object": 0.5 }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })"
    });

    // 2. ISADE: MultipleControlFunction + Min/Average/CurrentFitnessControlUpdate on both F and CR
    cases.push_back({
        "ISADE",
        0.0, 1.0,
        0.0, 1.0,
        R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 10 },
            "ControlFunction": {
                "classname": "MultipleControlFunction",
                "number_of_functions": 10,
                "Function": {
                    "classname": "IsadeFFunction",
                    "object": { "classname": "RealVariableFunction", "object": 0.5 },
                    "min": { "classname": "RealVariableFunction" },
                    "average": { "classname": "RealVariableFunction" },
                    "current": { "classname": "RealVariableFunction" },
                    "tau": { "classname": "RealConstantFunction", "object": 0.1 }
                }
            },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [
                { "classname": "MinFitnessControlUpdate" },
                { "classname": "AverageFitnessControlUpdate" },
                { "classname": "CurrentFitnessControlUpdate" }
            ]
        })",
        R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 10 },
            "ControlFunction": {
                "classname": "MultipleControlFunction",
                "number_of_functions": 10,
                "Function": {
                    "classname": "IsadeCrFunction",
                    "object": { "classname": "RealVariableFunction", "object": 0.9 },
                    "min": { "classname": "RealVariableFunction" },
                    "average": { "classname": "RealVariableFunction" },
                    "current": { "classname": "RealVariableFunction" },
                    "tau": { "classname": "RealConstantFunction", "object": 0.1 }
                }
            },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [
                { "classname": "MinFitnessControlUpdate" },
                { "classname": "AverageFitnessControlUpdate" },
                { "classname": "CurrentFitnessControlUpdate" }
            ]
        })"
    });

    // 3. SaDE: SadeCrControlMechanism + SingleControlFunction + MultipleControlParameter + LearningPeriodFunction + GenerationControlUpdate
    cases.push_back({
        "SaDE",
        0.0, 1.0,
        0.0, 1.0,
        R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 10 },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": {
                    "classname": "RealNormalDisFunction",
                    "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                    "stddev": { "classname": "RealConstantFunction", "object": 0.3 }
                }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })",
        R"({
            "classname": "SadeCrControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 10 },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": {
                    "classname": "RealNormalDisFunction",
                    "mean": {
                        "classname": "RealLearningPeriodFunction",
                        "learning_period": 4,
                        "object": {
                            "classname": "RealMedianFunction",
                            "storage_size": 10,
                            "initial_value": 0.5
                        }
                    },
                    "stddev": { "classname": "RealConstantFunction", "object": 0.1 }
                }
            },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [
                { "classname": "GenerationControlUpdate" }
            ]
        })"
    });

    // 4. SDE: SdeFControlMechanism + SdeFControlUpdate + Function-initialized MultipleControlParameter
    cases.push_back({
        "SDE",
        0.0, 1.0,
        0.0, 1.0,
        R"({
            "classname": "SdeFControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": {
                "classname": "RealMultipleControlParameter",
                "number_of_objects": 10,
                "initial_value": {
                    "classname": "RealNormalDisFunction",
                    "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                    "stddev": { "classname": "RealConstantFunction", "object": 0.15 }
                }
            },
            "ControlFunction": {
                "classname": "MultipleControlFunction",
                "number_of_functions": 10,
                "Function": {
                    "classname": "SdeFFunction",
                    "rand": {
                        "classname": "RealNormalDisFunction",
                        "mean": { "classname": "RealConstantFunction", "object": 0.0 },
                        "stddev": { "classname": "RealConstantFunction", "object": 0.5 }
                    },
                    "number_of_parameters": 3
                }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": [
                { "classname": "SdeFControlUpdate" }
            ]
        })",
        R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 10 },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": {
                    "classname": "RealNormalDisFunction",
                    "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                    "stddev": { "classname": "RealConstantFunction", "object": 0.15 }
                }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })"
    });

    // 5. NSDE: IndirectControlMechanism + nested ControlMechanism (RandomSelectionFunction)
    cases.push_back({
        "NSDE",
        0.0, 1.0,
        0.0, 1.0,
        R"({
            "classname": "RealIndirectControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 10 },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": [],
            "ControlMechanism": {
                "classname": "RealControlControlMechanism",
                "ControlRange": { "classname": "RealControlControlRange" },
                "ControlParameter": { "classname": "RealControlMultipleControlParameter", "number_of_objects": 10 },
                "ControlFunction": {
                    "classname": "SingleControlFunction",
                    "Function": {
                        "classname": "RealControlRandomSelectionFunction",
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
                    }
                },
                "ControlSelection": { "classname": "RealControlBetterOffspringControlSelection" },
                "ControlUpdate": []
            }
        })",
        R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 10 },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": {
                    "classname": "RealUniformDisFunction",
                    "lower_bound": { "classname": "RealConstantFunction", "object": 0.0 },
                    "upper_bound": { "classname": "RealConstantFunction", "object": 1.0 }
                }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })"
    });

    // 6. SaNSDE: IndirectControlMechanism + nested LearningPeriodFunction + RouletteWheelSelectionFunction
    //    with GenerationControlUpdate on the inner mechanism + SadeCrControlMechanism (WeightedAverageFunction)
    cases.push_back({
        "SaNSDE",
        0.0, 1.0,
        0.0, 1.0,
        R"({
            "classname": "RealIndirectControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 10 },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": [],
            "ControlMechanism": {
                "classname": "RealControlControlMechanism",
                "ControlRange": { "classname": "RealControlControlRange" },
                "ControlParameter": { "classname": "RealControlMultipleControlParameter", "number_of_objects": 10 },
                "ControlFunction": {
                    "classname": "SingleControlFunction",
                    "Function": {
                        "classname": "RealControlLearningPeriodFunction",
                        "learning_period": 4,
                        "object": {
                            "classname": "RealControlRouletteWheelSelectionFunction",
                            "scoring_function": { "classname": "SuccessScoringFunction" },
                            "score_size": 10,
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
                        }
                    }
                },
                "ControlSelection": { "classname": "RealControlBetterOffspringControlSelection" },
                "ControlUpdate": [
                    { "classname": "GenerationControlUpdate" }
                ]
            }
        })",
        R"({
            "classname": "SadeCrControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 10 },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": {
                    "classname": "RealNormalDisFunction",
                    "mean": {
                        "classname": "RealLearningPeriodFunction",
                        "learning_period": 4,
                        "object": {
                            "classname": "WeightedAverageFunction",
                            "scoring_function": { "classname": "ImprovedPercentageScoringFunction" },
                            "object_size": 10,
                            "initial_value": 0.5
                        }
                    },
                    "stddev": { "classname": "RealConstantFunction", "object": 0.1 }
                }
            },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [
                { "classname": "GenerationControlUpdate" }
            ]
        })"
    });

    return cases;
}

} // namespace

int main()
{
    const auto cases = make_algorithm_cases();
    for (const auto& alg : cases) {
        if (!verify_algorithm_case(alg)) {
            return 1;
        }
    }

    std::cout << "All adaptive DE deterministic integration tests passed (DEPD, ISADE, SaDE, SDE, NSDE, SaNSDE)."
              << std::endl;
    return 0;
}
