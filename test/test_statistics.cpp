#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "adef.h"
#include "Configuration.h"
#include "Evaluator.h"
#include "Evolution.h"
#include "Experiment.h"
#include "ExperimentalStatistics.h"
#include "Individual.h"
#include "Population.h"
#include "Problem.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Repository.h"
#include "Statistics.h"
#include "System.h"
#include "SystemStatistics.h"

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

bool approx_printed(double actual, double expected)
{
    const double tol = std::max(1e-9, 1e-5 * std::abs(expected));
    return std::abs(actual - expected) <= tol;
}

class TempFilesGuard
{
public:
    TempFilesGuard() = default;

    explicit TempFilesGuard(std::string filename)
    {
        files_.push_back(std::move(filename));
    }

    ~TempFilesGuard()
    {
        for (const auto& file : files_) {
            std::remove(file.c_str());
        }
    }

    void track(std::string filename)
    {
        files_.push_back(std::move(filename));
    }

    TempFilesGuard(const TempFilesGuard&) = delete;
    TempFilesGuard& operator=(const TempFilesGuard&) = delete;

private:
    std::vector<std::string> files_;
};

adef::Configuration load_json_config(const std::string& json_text,
                                     const std::string& filename)
{
    TempFilesGuard guard(filename);
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

std::string read_file_text(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> split_csv_row(const std::string& line)
{
    std::vector<std::string> cols;
    std::string field;
    for (char ch : line) {
        if (ch == ',') {
            cols.push_back(field);
            field.clear();
        }
        else {
            field.push_back(ch);
        }
    }
    cols.push_back(field);
    return cols;
}

class InspectableStatistics : public adef::Statistics
{
public:
    bool has_item(Item item) const
    {
        return statistics_all_individuals_.find(item) !=
               statistics_all_individuals_.end();
    }

    std::size_t item_history_size(Item item) const
    {
        auto it = statistics_all_individuals_.find(item);
        return it == statistics_all_individuals_.end() ? 0 : it->second.size();
    }

    const std::vector<std::string>& configured_items() const
    {
        return names_of_items_;
    }

private:
    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<InspectableStatistics>(*this);
    }
};

class TestRepository : public adef::Repository
{
public:
    void set_population(std::shared_ptr<adef::Population> pop)
    {
        population_ = std::move(pop);
    }

    void set_problem(std::shared_ptr<adef::Problem> prob)
    {
        problem_ = std::move(prob);
    }
};

class ScriptedFesProblem : public adef::Problem
{
public:
    ScriptedFesProblem(std::shared_ptr<unsigned int> next_run_index,
                       std::size_t pop_size) :
        adef::Problem("ScriptedFes"),
        next_run_index_(std::move(next_run_index)),
        pop_size_(pop_size)
    {
        dimension_of_decision_space_ = 2;
        boundaries_of_decision_space_.resize(2, Boundary(-10.0, 10.0));
        dimension_of_objective_space_ = 1;
        boundaries_of_objective_space_.resize(1, Boundary(0.0, 100.0));
        optimal_solution_ = 0.0;
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    void init(std::shared_ptr<adef::Repository> repos) override
    {
        adef::Problem::init(repos);
        current_run_ = (*next_run_index_)++;
        eval_count_ = 0;
    }

    void evaluation_function(std::shared_ptr<adef::Individual> individual) const override
    {
        ++eval_count_;
        // Run 0: succeeds immediately on initial evaluation (G=0 -> FEs=P)
        // Run 1: succeeds on generation 2 offspring (G=2 -> FEs=3*P)
        // Run 2+: never succeeds (runs full max_generation -> FEs=(max_gen+1)*P)
        if (current_run_ == 0) {
            individual->set_fitness_value(0.0);
        }
        else if (current_run_ == 1 && eval_count_ > 2 * pop_size_) {
            individual->set_fitness_value(0.0);
        }
        else {
            individual->set_fitness_value(5.0);
        }
    }

private:
    std::shared_ptr<unsigned int> next_run_index_;
    std::size_t pop_size_;
    unsigned int current_run_ = 0;
    mutable unsigned int eval_count_ = 0;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<ScriptedFesProblem>(*this);
    }
};

std::string build_repository_json(const std::string& algorithm_name,
                                  unsigned int max_generation,
                                  unsigned int dimension,
                                  std::size_t population_size,
                                  double success_error,
                                  const std::string& problem_classname = "Sphere")
{
    std::ostringstream oss;
    oss << R"({
        "classname": "Repository",
        "algorithm_name": ")" << algorithm_name << R"(",
        "Evolution": {
            "classname": "Evolution",
            "max_generation": )" << max_generation << R"(
        },
        "Problem": {
            "classname": ")" << problem_classname << R"(",
            "dimension_of_decision_space": )" << dimension << R"(
        },
        "Statistics": {
            "classname": "Statistics",
            "success_error": )" << success_error << R"(,
            "item": []
        },
        "Parameters": { "classname": "Parameters" },
        "Initializer": { "classname": "Initializer" },
        "Evaluator": { "classname": "Evaluator" },
        "Population": {
            "classname": "Population",
            "population_size": )" << population_size << R"(,
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
    })";
    return oss.str();
}

std::string build_experiment_json(const std::string& algorithm_name,
                                  adef::Random::SeedType seed,
                                  unsigned int number_of_runs,
                                  unsigned int max_generation,
                                  unsigned int dimension,
                                  std::size_t population_size,
                                  double success_error,
                                  const std::string& problem_classname = "Sphere")
{
    std::ostringstream oss;
    oss << R"({
        "classname": "Experiment",
        "Random": { "classname": "Random", "seed": )" << seed << R"( },
        "number_of_runs": )" << number_of_runs << R"(,
        "ExperimentalStatistics": { "classname": "ExperimentalStatistics" },
        "Repository": )" << build_repository_json(algorithm_name,
                                                  max_generation,
                                                  dimension,
                                                  population_size,
                                                  success_error,
                                                  problem_classname) << R"(
    })";
    return oss.str();
}

// ============================================================================
// 1. Fresh construction & dirty-memory zero-initialization
// ============================================================================
bool test_default_construction_and_dirty_memory()
{
    // Direct stack construction
    adef::Statistics stack_stats;
    if (!require(stack_stats.number_of_fitness_evaluations() == 0,
                 "Default-constructed Statistics must start with 0 FEs")) {
        return false;
    }
    if (!require(stack_stats.best_fitness().empty(),
                 "Default-constructed Statistics must start with empty best_fitness")) {
        return false;
    }
    if (!require(stack_stats.best_fitness_so_far() == std::numeric_limits<double>::max(),
                 "Default-constructed Statistics must start with max double best_fitness_so_far")) {
        return false;
    }

    // Placement-new into non-zero dirty memory to deterministically catch uninitialized members
    alignas(adef::Statistics) unsigned char dirty_storage[sizeof(adef::Statistics)];
    std::memset(dirty_storage, 0xA5, sizeof(dirty_storage));
    auto* placed = ::new (static_cast<void*>(dirty_storage)) adef::Statistics();
    struct PlacementDestroyer {
        adef::Statistics* ptr;
        ~PlacementDestroyer() { ptr->~Statistics(); }
    } destroyer{ placed };

    if (!require(placed->number_of_fitness_evaluations() == 0,
                 "Placement-new Statistics over 0xA5 memory must initialize FEs to 0")) {
        return false;
    }
    placed->increase_number_of_fitness_evaluations();
    placed->increase_number_of_fitness_evaluations();
    if (!require(placed->number_of_fitness_evaluations() == 2,
                 "Incrementing placement-new Statistics must count cleanly from 0")) {
        return false;
    }

    // PrototypeManager registration & factory creation
    adef::PrototypeManager pm;
    adef::register_type(pm);
    auto proto_stats = pm.make_type<adef::Statistics>("Statistics");
    if (!require(proto_stats != nullptr && proto_stats->number_of_fitness_evaluations() == 0,
                 "PrototypeManager-created Statistics must start with 0 FEs")) {
        return false;
    }

    return true;
}

// ============================================================================
// 2. Exact increment and Evaluator accounting
// ============================================================================
bool test_increment_and_evaluator_accounting()
{
    adef::Statistics stats;
    for (unsigned int expected = 1; expected <= 25; ++expected) {
        stats.increase_number_of_fitness_evaluations();
        if (!require(stats.number_of_fitness_evaluations() == expected,
                     "Expected FEs == " + std::to_string(expected) + ", got " +
                         std::to_string(stats.number_of_fitness_evaluations()))) {
            return false;
        }
    }

    adef::PrototypeManager pm;
    adef::register_type(pm);

    constexpr std::size_t kPopSize = 7;
    auto pop_cfg = load_json_config(
        R"({"classname":"Population","population_size":7,"Individual":{"classname":"Individual"}})",
        "test_stats_eval_pop.json");
    auto pop = adef::make_and_setup_type<adef::Population>(pop_cfg, pm);

    auto prob_cfg = load_json_config(
        R"({"classname":"Sphere","dimension_of_decision_space":4})",
        "test_stats_eval_prob.json");
    auto prob = adef::make_and_setup_type<adef::Problem>(prob_cfg, pm);

    auto repos = std::make_shared<TestRepository>();
    repos->set_problem(prob);
    repos->set_population(pop);
    pop->init(repos);
    for (std::size_t idx = 0; idx < kPopSize; ++idx) {
        for (unsigned int dim = 0; dim < prob->dimension_of_decision_space(); ++dim) {
            pop->at(idx)->variables(dim) = static_cast<double>(idx + dim + 1);
        }
    }

    auto evaluator = pm.make_type<adef::Evaluator>("Evaluator");
    auto eval_stats = pm.make_type<adef::Statistics>("Statistics");

    for (unsigned int round = 1; round <= 3; ++round) {
        evaluator->evaluate(pop, prob, eval_stats);
        const unsigned int expected = static_cast<unsigned int>(kPopSize * round);
        if (!require(eval_stats->number_of_fitness_evaluations() == expected,
                     "Evaluator::evaluate round " + std::to_string(round) +
                         " expected FEs == " + std::to_string(expected) + ", got " +
                         std::to_string(eval_stats->number_of_fitness_evaluations()))) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 3. Clone isolation, init() reset, and setup() reset
// ============================================================================
bool test_reinit_setup_and_clone_isolation()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);
    pm.register_type("InspectableStatistics", std::make_shared<InspectableStatistics>());

    auto cfg = load_json_config(
        R"({"classname":"InspectableStatistics","success_error":0.25,"item":["parents","f"]})",
        "test_stats_setup_1.json");
    auto proto = adef::make_and_setup_type<InspectableStatistics>(cfg, pm);
    if (!require(proto != nullptr && proto->number_of_fitness_evaluations() == 0,
                 "Configured Statistics prototype must start at 0 FEs")) {
        return false;
    }

    // Build a small Repository with Sphere optimal 0.0 and fitness 0.1 (< 0.25, > 0.00001)
    auto prob_cfg = load_json_config(
        R"({"classname":"Sphere","dimension_of_decision_space":2})",
        "test_stats_iso_prob.json");
    auto prob = adef::make_and_setup_type<adef::Problem>(prob_cfg, pm);

    auto pop_cfg = load_json_config(
        R"({"classname":"Population","population_size":2,"Individual":{"classname":"Individual"}})",
        "test_stats_iso_pop.json");
    auto pop = adef::make_and_setup_type<adef::Population>(pop_cfg, pm);
    pop->at(0)->set_fitness_value(0.1);
    pop->at(1)->set_fitness_value(0.5);

    auto repos = std::make_shared<TestRepository>();
    repos->set_problem(prob);
    repos->set_population(pop);

    // Clone two independent runs from the configured prototype
    auto clone_a = std::dynamic_pointer_cast<InspectableStatistics>(proto->clone());
    auto clone_b = std::dynamic_pointer_cast<InspectableStatistics>(proto->clone());
    if (!require(clone_a != nullptr && clone_b != nullptr &&
                     clone_a->number_of_fitness_evaluations() == 0 &&
                     clone_b->number_of_fitness_evaluations() == 0,
                 "Cloned Statistics instances from prototype must start at 0 FEs")) {
        return false;
    }

    for (int i = 0; i < 11; ++i) {
        clone_a->increase_number_of_fitness_evaluations();
    }
    clone_a->take_statistics(repos);
    clone_a->store_statistics_all_individuals(adef::Statistics::Item::f, { 0.5, 0.6 });

    for (int i = 0; i < 5; ++i) {
        clone_b->increase_number_of_fitness_evaluations();
    }

    if (!require(proto->number_of_fitness_evaluations() == 0 &&
                     proto->best_fitness().empty() &&
                     !proto->has_item(adef::Statistics::Item::f),
                 "Mutating clones must not mutate prototype Statistics")) {
        return false;
    }
    if (!require(clone_a->number_of_fitness_evaluations() == 11 &&
                     clone_b->number_of_fitness_evaluations() == 5 &&
                     clone_a->best_fitness().size() == 1 &&
                     clone_b->best_fitness().empty(),
                 "Cloned Statistics instances must remain isolated from each other")) {
        return false;
    }

    // Clone an already-used Statistics, verify isolation, then re-initialize with init()
    auto clone_used = std::dynamic_pointer_cast<InspectableStatistics>(clone_a->clone());
    if (!require(clone_used != nullptr &&
                     clone_used->number_of_fitness_evaluations() == 11,
                 "Cloning a used Statistics preserves its current count before init()")) {
        return false;
    }
    clone_used->increase_number_of_fitness_evaluations();
    if (!require(clone_used->number_of_fitness_evaluations() == 12 &&
                     clone_a->number_of_fitness_evaluations() == 11,
                 "Mutating clone of used Statistics must not mutate source")) {
        return false;
    }

    clone_used->init(repos);
    if (!require(clone_used->number_of_fitness_evaluations() == 0 &&
                     clone_used->best_fitness().empty() &&
                     clone_used->best_fitness_so_far() == std::numeric_limits<double>::max() &&
                     !clone_used->has_item(adef::Statistics::Item::f),
                 "Statistics::init() must reset FEs and per-run history")) {
        return false;
    }
    if (!require(clone_a->number_of_fitness_evaluations() == 11 &&
                     clone_a->best_fitness().size() == 1 &&
                     clone_a->has_item(adef::Statistics::Item::f),
                 "Re-initializing clone must not affect source Statistics")) {
        return false;
    }

    // Verify init() preserved configured success_error_ (0.25) and names_of_items_ (["parents","f"])
    clone_used->take_statistics(repos);
    clone_used->store_statistics_all_individuals(adef::Statistics::Item::f, { 0.4, 0.7 });
    if (!require(clone_used->is_evolve_success(repos) &&
                     approx_equal(clone_used->best_fitness_so_far(), 0.1) &&
                     clone_used->item_history_size(adef::Statistics::Item::f) == 1,
                 "Statistics::init() must preserve configured success_error and statistical items")) {
        return false;
    }

    // Verify re-running setup() clears previous run state and replaces configured items cleanly
    auto cfg2 = load_json_config(
        R"({"classname":"InspectableStatistics","success_error":0.05,"item":["cr"]})",
        "test_stats_setup_2.json");
    clone_a->setup(cfg2, pm);
    if (!require(clone_a->number_of_fitness_evaluations() == 0 &&
                     clone_a->best_fitness().empty() &&
                     clone_a->best_fitness_so_far() == std::numeric_limits<double>::max() &&
                     !clone_a->has_item(adef::Statistics::Item::f) &&
                     clone_a->configured_items().size() == 1 &&
                     clone_a->configured_items()[0] == "cr",
                 "Statistics::setup() must reset per-run state and replace item configuration")) {
        return false;
    }
    clone_a->take_statistics(repos);
    clone_a->store_statistics_all_individuals(adef::Statistics::Item::f, { 0.5, 0.5 });
    clone_a->store_statistics_all_individuals(adef::Statistics::Item::cr, { 0.9, 0.9 });
    if (!require(!clone_a->is_evolve_success(repos) &&
                     !clone_a->has_item(adef::Statistics::Item::f) &&
                     clone_a->item_history_size(adef::Statistics::Item::cr) == 1,
                 "Statistics::setup() must apply updated success_error and item filter")) {
        return false;
    }

    return true;
}

// ============================================================================
// 4. Evolution exact P * (G + 1) accounting (full, G=0, early-stop, repo re-init)
// ============================================================================
bool test_evolution_fes_accounting_and_repo_reinit()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);

    // 4a. Full max_generation run (no early termination)
    {
        constexpr std::size_t kPop = 12;
        constexpr unsigned int kMaxGen = 15;
        auto cfg = load_json_config(
            build_repository_json("DE_Full", kMaxGen, 5, kPop, 0.0),
            "test_stats_evo_full.json");
        auto repos = adef::make_and_setup_type<adef::Repository>(cfg, pm);
        repos->init(std::make_shared<adef::Random>(1234u));
        repos->evolution()->evolve(repos);

        const std::size_t best_count = repos->statistics()->best_fitness().size();
        const unsigned int expected_fes = static_cast<unsigned int>(kPop * (kMaxGen + 1));
        if (!require(best_count == kMaxGen + 1 &&
                         repos->statistics()->number_of_fitness_evaluations() == expected_fes,
                     "Full evolution run expected best_fitness.size()==" +
                         std::to_string(kMaxGen + 1) + " and FEs==" +
                         std::to_string(expected_fes) + ", got size=" +
                         std::to_string(best_count) + " and FEs=" +
                         std::to_string(repos->statistics()->number_of_fitness_evaluations()))) {
            return false;
        }

        // 4d. Re-initialize the SAME Repository and evolve a second run; FEs must reset to 0
        repos->init(std::make_shared<adef::Random>(5678u));
        if (!require(repos->statistics()->number_of_fitness_evaluations() == 0 &&
                         repos->statistics()->best_fitness().empty(),
                     "Repository::init() must reset Statistics before a subsequent run")) {
            return false;
        }
        repos->evolution()->evolve(repos);
        if (!require(repos->statistics()->best_fitness().size() == kMaxGen + 1 &&
                         repos->statistics()->number_of_fitness_evaluations() == expected_fes,
                     "Re-initialized Repository run expected exact FEs==" +
                         std::to_string(expected_fes) + ", got " +
                         std::to_string(repos->statistics()->number_of_fitness_evaluations()))) {
            return false;
        }
    }

    // 4b. Immediate termination after initial population evaluation (G = 0)
    {
        constexpr std::size_t kPop = 9;
        constexpr unsigned int kMaxGen = 20;
        auto cfg = load_json_config(
            build_repository_json("DE_ZeroGen", kMaxGen, 3, kPop, 1e9),
            "test_stats_evo_zero.json");
        auto repos = adef::make_and_setup_type<adef::Repository>(cfg, pm);
        repos->init(std::make_shared<adef::Random>(2222u));
        repos->evolution()->evolve(repos);

        if (!require(repos->statistics()->is_evolve_success(repos) &&
                         repos->statistics()->best_fitness().size() == 1 &&
                         repos->statistics()->number_of_fitness_evaluations() == kPop,
                     "Immediate-termination run (G=0) expected FEs==" +
                         std::to_string(kPop) + ", got " +
                         std::to_string(repos->statistics()->number_of_fitness_evaluations()))) {
            return false;
        }
    }

    // 4c. Mid-run early termination (1 <= G < max_generation)
    {
        constexpr std::size_t kPop = 15;
        constexpr unsigned int kMaxGen = 500;
        auto cfg = load_json_config(
            build_repository_json("DE_EarlyStop", kMaxGen, 2, kPop, 1e-2),
            "test_stats_evo_early.json");
        auto repos = adef::make_and_setup_type<adef::Repository>(cfg, pm);
        repos->init(std::make_shared<adef::Random>(42u));
        repos->evolution()->evolve(repos);

        const std::size_t eval_phases = repos->statistics()->best_fitness().size();
        const std::size_t executed_gens = eval_phases > 0 ? eval_phases - 1 : 0;
        const unsigned int expected_fes = static_cast<unsigned int>(kPop * eval_phases);
        if (!require(repos->statistics()->is_evolve_success(repos) &&
                         executed_gens >= 1 && executed_gens < kMaxGen &&
                         repos->statistics()->number_of_fitness_evaluations() == expected_fes,
                     "Early-stopping run expected 1 <= G < " + std::to_string(kMaxGen) +
                         " and FEs==" + std::to_string(expected_fes) + ", got G=" +
                         std::to_string(executed_gens) + " and FEs=" +
                         std::to_string(repos->statistics()->number_of_fitness_evaluations()))) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 5. End-to-end Experiment + ExperimentalStatistics + System + SystemStatistics
//    CSV verification (FEs, NFE, SP, multi-run isolation, same-seed determinism)
// ============================================================================
struct ParsedExpRow {
    double fes = 0.0;
    double error = 0.0;
    double best_fitness = 0.0;
};

std::vector<ParsedExpRow> parse_experiment_csv(const std::string& filename)
{
    const auto lines = split_lines(read_file_text(filename));
    if (lines.empty() || lines[0] != "FEs,Error,best_fitness") {
        throw std::runtime_error("Unexpected Experiment CSV header in " + filename);
    }

    std::vector<ParsedExpRow> rows;
    for (std::size_t idx = 1; idx < lines.size(); ++idx) {
        if (lines[idx].empty()) {
            continue;
        }
        const auto cols = split_csv_row(lines[idx]);
        if (cols.size() != 3) {
            throw std::runtime_error("Expected 3 columns in Experiment CSV row: " + lines[idx]);
        }
        rows.push_back({
            std::stod(cols[0]),
            std::stod(cols[1]),
            std::stod(cols[2])
        });
    }
    return rows;
}

std::size_t count_best_fitness_csv_rows(const std::string& filename)
{
    const auto lines = split_lines(read_file_text(filename));
    if (lines.empty() || lines[0] != "Gen,best_fitness") {
        throw std::runtime_error("Unexpected best_fitness CSV header in " + filename);
    }
    std::size_t count = 0;
    for (std::size_t idx = 1; idx < lines.size(); ++idx) {
        if (!lines[idx].empty()) {
            ++count;
        }
    }
    return count;
}

struct ParsedSystemItem {
    double best = 0.0;
    double worst = 0.0;
    double mean = 0.0;
    double stddev = 0.0;
};

// Parse <prefix>_FEs.csv into map[problem][algorithm] = {best, worst, mean, stddev}
std::map<std::string, std::map<std::string, ParsedSystemItem>>
parse_system_item_csv(const std::string& filename)
{
    const std::string text = read_file_text(filename);
    const std::size_t first_nl = text.find('\n');
    if (first_nl == std::string::npos) {
        throw std::runtime_error("Missing header in System item CSV: " + filename);
    }
    std::string header = text.substr(0, first_nl);
    if (!header.empty() && header.back() == '\r') {
        header.pop_back();
    }
    const auto header_cols = split_csv_row(header);
    if (header_cols.size() < 2 || header_cols[0] != "Problem/Algorithm") {
        throw std::runtime_error("Unexpected System item CSV header: " + header);
    }

    std::vector<std::string> algorithms;
    for (std::size_t idx = 1; idx < header_cols.size(); ++idx) {
        if (!header_cols[idx].empty()) {
            algorithms.push_back(header_cols[idx]);
        }
    }

    std::map<std::string, std::map<std::string, ParsedSystemItem>> result;
    std::size_t pos = first_nl + 1;
    while (pos < text.size()) {
        while (pos < text.size() && (text[pos] == '\r' || text[pos] == '\n')) {
            ++pos;
        }
        if (pos >= text.size()) {
            break;
        }

        const std::size_t comma = text.find(',', pos);
        if (comma == std::string::npos) {
            throw std::runtime_error("Malformed problem row in " + filename);
        }
        const std::string problem = text.substr(pos, comma - pos);
        pos = comma + 1;

        for (const auto& alg : algorithms) {
            if (pos >= text.size()) {
                throw std::runtime_error("Unexpected EOF while parsing cell for " + alg);
            }
            if (text[pos] == '"') {
                ++pos;
                const std::size_t close_quote = text.find('"', pos);
                if (close_quote == std::string::npos) {
                    throw std::runtime_error("Unterminated quoted cell in " + filename);
                }
                const auto vals = split_lines(text.substr(pos, close_quote - pos));
                if (vals.size() != 4) {
                    throw std::runtime_error("Expected 4 stat lines in quoted cell for " + alg);
                }
                result[problem][alg] = {
                    std::stod(vals[0]),
                    std::stod(vals[1]),
                    std::stod(vals[2]),
                    std::stod(vals[3])
                };
                pos = close_quote + 1;
                if (pos < text.size() && text[pos] == ',') {
                    ++pos;
                }
            }
            else if (text[pos] == ',') {
                ++pos;
            }
            else {
                throw std::runtime_error("Unexpected token in System item CSV: " + filename);
            }
        }
    }

    return result;
}

struct ParsedSPRow {
    bool has_nfe = false;
    double nfe = 0.0;
    unsigned int srun = 0;
    double srate = 0.0;
    bool has_sp = false;
    double sp = 0.0;
};

// Parse <prefix>_success_performance.csv into map[problem][algorithm]
std::map<std::string, std::map<std::string, ParsedSPRow>>
parse_system_sp_csv(const std::string& filename)
{
    const auto lines = split_lines(read_file_text(filename));
    if (lines.size() < 3) {
        throw std::runtime_error("Too few lines in SP CSV: " + filename);
    }

    const auto alg_cols = split_csv_row(lines[0]);
    if (alg_cols.empty() || alg_cols[0] != "Problem/Algorithm") {
        throw std::runtime_error("Unexpected SP CSV header: " + lines[0]);
    }
    std::vector<std::string> algorithms;
    for (std::size_t idx = 1; idx < alg_cols.size(); ++idx) {
        if (!alg_cols[idx].empty()) {
            algorithms.push_back(alg_cols[idx]);
        }
    }

    std::map<std::string, std::map<std::string, ParsedSPRow>> result;
    for (std::size_t row_idx = 2; row_idx < lines.size(); ++row_idx) {
        if (lines[row_idx].empty()) {
            continue;
        }
        const auto cols = split_csv_row(lines[row_idx]);
        if (cols.size() < 1 + 4 * algorithms.size()) {
            throw std::runtime_error("Too few columns in SP CSV row: " + lines[row_idx]);
        }
        const std::string& problem = cols[0];
        for (std::size_t alg_idx = 0; alg_idx < algorithms.size(); ++alg_idx) {
            const std::size_t base = 1 + 4 * alg_idx;
            if (cols[base].empty() && cols[base + 1].empty() &&
                cols[base + 2].empty() && cols[base + 3].empty()) {
                continue;
            }
            ParsedSPRow sp;
            if (!cols[base].empty()) {
                sp.has_nfe = true;
                sp.nfe = std::stod(cols[base]);
            }
            sp.srun = static_cast<unsigned int>(std::stoul(cols[base + 1]));
            sp.srate = std::stod(cols[base + 2]);
            if (!cols[base + 3].empty()) {
                sp.has_sp = true;
                sp.sp = std::stod(cols[base + 3]);
            }
            result[problem][algorithms[alg_idx]] = sp;
        }
    }
    return result;
}

ParsedSystemItem compute_expected_item(const std::vector<double>& data)
{
    ParsedSystemItem item;
    item.best = data.front();
    item.worst = data.front();
    item.mean = 0.0;
    for (double v : data) {
        item.best = std::min(item.best, v);
        item.worst = std::max(item.worst, v);
        item.mean += v;
    }
    item.mean /= static_cast<double>(data.size());

    double var = 0.0;
    for (double v : data) {
        var += (v - item.mean) * (v - item.mean);
    }
    item.stddev = std::sqrt(var / static_cast<double>(data.size()));
    return item;
}

bool test_experiment_and_system_statistics_end_to_end()
{
    constexpr unsigned int kFullRuns = 3;
    constexpr std::size_t kFullPop = 10;
    constexpr unsigned int kFullMaxGen = 6;

    constexpr unsigned int kEarlyRuns = 4;
    constexpr std::size_t kEarlyPop = 12;
    constexpr unsigned int kEarlyMaxGen = 300;

    // Deterministic mixed success/failure case via ScriptedFesProblem:
    // Run 0 -> G=0 (FEs=10, success), Run 1 -> G=2 (FEs=30, success), Run 2 -> G=4 (FEs=50, fail)
    // Exact summary: best=10, worst=50, mean=30, stddev=sqrt(800/3),
    //                SRun=2, SRate=2/3, NFE=40/3, SP=20 (NFE != SP).
    constexpr unsigned int kMixedRuns = 3;
    constexpr std::size_t kMixedPop = 10;
    constexpr unsigned int kMixedMaxGen = 4;

    auto run_system_and_verify = [&](const std::string& sys_prefix,
                                     const std::string& full_alg,
                                     const std::string& early_alg,
                                     const std::string& mixed_alg,
                                     std::string& out_full_csv,
                                     std::string& out_early_csv,
                                     std::string& out_mixed_csv,
                                     std::string& out_fes_csv,
                                     std::string& out_sp_csv) -> bool {
        adef::PrototypeManager pm;
        adef::register_type(pm);
        auto scripted_run_counter = std::make_shared<unsigned int>(0);
        pm.register_type("ScriptedFesProblem",
                         std::make_shared<ScriptedFesProblem>(scripted_run_counter, kMixedPop));

        TempFilesGuard files;
        auto track_experiment_files = [&](const std::string& alg,
                                          const std::string& problem_name,
                                          unsigned int num_runs) {
            const std::string base = alg + "_" + problem_name;
            files.track(base + ".csv");
            for (unsigned int r = 1; r <= num_runs; ++r) {
                files.track(base + "_run" + std::to_string(r) + "_best_fitness.csv");
            }
        };
        track_experiment_files(full_alg, "Sphere", kFullRuns);
        track_experiment_files(early_alg, "Sphere", kEarlyRuns);
        track_experiment_files(mixed_alg, "ScriptedFes", kMixedRuns);
        files.track(sys_prefix + "_FEs.csv");
        files.track(sys_prefix + "_Error.csv");
        files.track(sys_prefix + "_Fitness_so_far.csv");
        files.track(sys_prefix + "_success_performance.csv");

        const std::string system_json = R"({
            "classname": "System",
            "name": ")" + sys_prefix + R"(",
            "SystemStatistics": { "classname": "SystemStatistics" },
            "Experiment": [
                )" + build_experiment_json(full_alg, 101u, kFullRuns, kFullMaxGen, 5, kFullPop, 0.0) + R"(,
                )" + build_experiment_json(early_alg, 202u, kEarlyRuns, kEarlyMaxGen, 2, kEarlyPop, 1e-3) + R"(,
                )" + build_experiment_json(mixed_alg, 303u, kMixedRuns, kMixedMaxGen, 2, kMixedPop, 1e-5, "ScriptedFesProblem") + R"(
            ]
        })";

        auto cfg = load_json_config(system_json, sys_prefix + "_config.json");
        auto sys = adef::make_and_setup_type<adef::System>(cfg, pm);
        sys->run();

        out_full_csv = read_file_text(full_alg + "_Sphere.csv");
        out_early_csv = read_file_text(early_alg + "_Sphere.csv");
        out_mixed_csv = read_file_text(mixed_alg + "_ScriptedFes.csv");
        out_fes_csv = read_file_text(sys_prefix + "_FEs.csv");
        out_sp_csv = read_file_text(sys_prefix + "_success_performance.csv");

        const auto sys_fes = parse_system_item_csv(sys_prefix + "_FEs.csv");
        const auto sys_sp = parse_system_sp_csv(sys_prefix + "_success_performance.csv");

        auto verify_experiment_outputs = [&](const std::string& alg,
                                             const std::string& problem_name,
                                             unsigned int num_runs,
                                             std::size_t pop_size,
                                             unsigned int max_gen,
                                             bool expect_all_full,
                                             bool expect_all_success) -> bool {
            const std::string base = alg + "_" + problem_name;
            const auto exp_rows = parse_experiment_csv(base + ".csv");
            if (!require(exp_rows.size() == num_runs,
                         alg + ": expected " + std::to_string(num_runs) +
                             " rows in Experiment CSV, got " + std::to_string(exp_rows.size()))) {
                return false;
            }

            std::vector<double> expected_fes_vec;
            expected_fes_vec.reserve(num_runs);
            double succ_fes_sum = 0.0;
            unsigned int succ_runs = 0;

            for (unsigned int r = 0; r < num_runs; ++r) {
                const std::string run_bf_file =
                    base + "_run" + std::to_string(r + 1) + "_best_fitness.csv";
                const std::size_t eval_phases = count_best_fitness_csv_rows(run_bf_file);
                const std::size_t executed_gens = eval_phases > 0 ? eval_phases - 1 : 0;
                const double expected_fes = static_cast<double>(pop_size * eval_phases);

                if (expect_all_full) {
                    if (!require(executed_gens == max_gen,
                                 alg + ": expected full " + std::to_string(max_gen) +
                                     " generations on run " + std::to_string(r + 1))) {
                        return false;
                    }
                }
                if (!require(approx_equal(exp_rows[r].fes, expected_fes),
                             alg + ": run " + std::to_string(r + 1) +
                                 " Experiment CSV FEs expected " + std::to_string(expected_fes) +
                                 ", got " + std::to_string(exp_rows[r].fes))) {
                    return false;
                }

                expected_fes_vec.push_back(expected_fes);
                if (std::abs(exp_rows[r].error - 0.0) <
                    std::numeric_limits<double>::epsilon()) {
                    succ_fes_sum += expected_fes;
                    ++succ_runs;
                }
            }

            if (expect_all_success) {
                if (!require(succ_runs == num_runs,
                             alg + ": expected all " + std::to_string(num_runs) +
                                 " runs to succeed, got " + std::to_string(succ_runs))) {
                    return false;
                }
            }

            // Verify <prefix>_FEs.csv matches exact per-run FEs summary
            const auto expected_item = compute_expected_item(expected_fes_vec);
            const auto& actual_item = sys_fes.at(problem_name).at(alg);
            if (!require(approx_printed(actual_item.best, expected_item.best) &&
                             approx_printed(actual_item.worst, expected_item.worst) &&
                             approx_printed(actual_item.mean, expected_item.mean) &&
                             approx_printed(actual_item.stddev, expected_item.stddev),
                         alg + ": SystemStatistics FEs CSV mismatch")) {
                return false;
            }

            // Verify <prefix>_success_performance.csv matches exact NFE / SRun / SRate / SP
            const auto& actual_sp = sys_sp.at(problem_name).at(alg);
            const double expected_srate =
                static_cast<double>(succ_runs) / static_cast<double>(num_runs);
            if (!require(actual_sp.srun == succ_runs &&
                             approx_printed(actual_sp.srate, expected_srate),
                         alg + ": SystemStatistics SP CSV SRun/SRate mismatch")) {
                return false;
            }

            if (succ_runs == 0) {
                if (!require(!actual_sp.has_nfe && !actual_sp.has_sp,
                             alg + ": expected empty NFE and SP when SRun == 0")) {
                    return false;
                }
            }
            else {
                const double expected_nfe = succ_fes_sum / static_cast<double>(num_runs);
                const double expected_sp = expected_nfe / expected_srate;
                if (!require(actual_sp.has_nfe && actual_sp.has_sp &&
                                 approx_printed(actual_sp.nfe, expected_nfe) &&
                                 approx_printed(actual_sp.sp, expected_sp),
                             alg + ": SystemStatistics SP CSV NFE/SP mismatch (expected NFE=" +
                                 std::to_string(expected_nfe) + ", SP=" +
                                 std::to_string(expected_sp) + ", got NFE=" +
                                 std::to_string(actual_sp.nfe) + ", SP=" +
                                 std::to_string(actual_sp.sp) + ")")) {
                    return false;
                }
            }

            return true;
        };

        if (!verify_experiment_outputs(full_alg, "Sphere", kFullRuns, kFullPop, kFullMaxGen, true, false)) {
            return false;
        }
        if (!verify_experiment_outputs(early_alg, "Sphere", kEarlyRuns, kEarlyPop, kEarlyMaxGen, false, true)) {
            return false;
        }
        if (!verify_experiment_outputs(mixed_alg, "ScriptedFes", kMixedRuns, kMixedPop, kMixedMaxGen, false, false)) {
            return false;
        }

        // Verify deterministic scripted mixed-success exact figures (FEs = {10, 30, 50}, SRun = 2/3)
        const auto mixed_rows = parse_experiment_csv(mixed_alg + "_ScriptedFes.csv");
        if (!require(approx_equal(mixed_rows[0].fes, 10.0) &&
                         approx_equal(mixed_rows[1].fes, 30.0) &&
                         approx_equal(mixed_rows[2].fes, 50.0) &&
                         approx_equal(mixed_rows[0].error, 0.0) &&
                         approx_equal(mixed_rows[1].error, 0.0) &&
                         approx_equal(mixed_rows[2].error, 5.0),
                     mixed_alg + ": expected exact scripted per-run FEs {10, 30, 50} and errors {0, 0, 5}")) {
            return false;
        }
        const auto& mixed_fes = sys_fes.at("ScriptedFes").at(mixed_alg);
        const auto& mixed_sp = sys_sp.at("ScriptedFes").at(mixed_alg);
        if (!require(approx_printed(mixed_fes.best, 10.0) &&
                         approx_printed(mixed_fes.worst, 50.0) &&
                         approx_printed(mixed_fes.mean, 30.0) &&
                         approx_printed(mixed_fes.stddev, std::sqrt(800.0 / 3.0)) &&
                         mixed_sp.srun == 2 &&
                         approx_printed(mixed_sp.srate, 2.0 / 3.0) &&
                         mixed_sp.has_nfe && approx_printed(mixed_sp.nfe, 40.0 / 3.0) &&
                         mixed_sp.has_sp && approx_printed(mixed_sp.sp, 20.0),
                     mixed_alg + ": scripted mixed-success SystemStatistics exact values mismatch")) {
            return false;
        }

        // Verify that early_alg actually exercised early stopping (not all runs at max_gen)
        const auto& early_fes_item = sys_fes.at("Sphere").at(early_alg);
        if (!require(early_fes_item.worst < static_cast<double>(kEarlyPop * (kEarlyMaxGen + 1)),
                     early_alg + ": expected early convergence before max_generation")) {
            return false;
        }

        return true;
    };

    std::string full_a, early_a, mixed_a, fes_a, sp_a;
    std::string full_b, early_b, mixed_b, fes_b, sp_b;

    if (!run_system_and_verify("test_stats_sys_a",
                               "TestStatsFull",
                               "TestStatsEarly",
                               "TestStatsMixed",
                               full_a,
                               early_a,
                               mixed_a,
                               fes_a,
                               sp_a)) {
        return false;
    }

    if (!run_system_and_verify("test_stats_sys_b",
                               "TestStatsFull",
                               "TestStatsEarly",
                               "TestStatsMixed",
                               full_b,
                               early_b,
                               mixed_b,
                               fes_b,
                               sp_b)) {
        return false;
    }

    // Same-seed determinism across independent System runs
    if (!require(full_a == full_b &&
                     early_a == early_b &&
                     mixed_a == mixed_b &&
                     fes_a == fes_b &&
                     sp_a == sp_b,
                 "Identical-seed System runs must produce byte-for-byte identical FEs and SP CSVs")) {
        return false;
    }

    return true;
}

} // namespace

int main()
{
    if (!test_default_construction_and_dirty_memory()) {
        return 1;
    }
    if (!test_increment_and_evaluator_accounting()) {
        return 1;
    }
    if (!test_reinit_setup_and_clone_isolation()) {
        return 1;
    }
    if (!test_evolution_fes_accounting_and_repo_reinit()) {
        return 1;
    }
    if (!test_experiment_and_system_statistics_end_to_end()) {
        return 1;
    }

    std::cout << "All Statistics FEs unit and integration tests passed." << std::endl;
    return 0;
}
