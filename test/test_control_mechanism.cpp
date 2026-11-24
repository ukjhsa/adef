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
#include "DEMutation.h"
#include "Individual.h"
#include "Parameters.h"
#include "Population.h"
#include "Problem.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Repository.h"
#include "Statistics.h"
#include "cm/ControlMechanism.h"
#include "cm/IndirectControlMechanism.h"
#include "cm/SadeCrControlMechanism.h"
#include "cm/SdeFControlMechanism.h"
#include "cm/cf/func/Function.h"
#include "cm/cf/func/RouletteWheelSelectionFunction.h"
#include "cm/cp/MultipleControlParameter.h"
#include "cm/cs/ControlSelection.h"
#include "cm/cu/ControlUpdate.h"
#include "cm/cu/SdeFControlUpdate.h"
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

std::shared_ptr<adef::Population> make_population_with_fitness(
    const std::vector<double>& fitnesses,
    const adef::PrototypeManager& pm,
    const std::string& tag)
{
    auto cfg = load_json_config(
        "{\"classname\":\"Population\",\"population_size\":" +
            std::to_string(fitnesses.size()) +
            ",\"Individual\":{\"classname\":\"Individual\"}}",
        "test_cm_pop_" + tag + ".json");
    auto pop = adef::make_and_setup_type<adef::Population>(cfg, pm);
    for (std::size_t idx = 0; idx < fitnesses.size(); ++idx) {
        pop->at(idx)->set_fitness_value(fitnesses[idx]);
    }
    return pop;
}

// ============================================================================
// Reusable test doubles for lifecycle, granularity, routing, ordering, cloning
// ============================================================================

struct FunctionObserver {
    int next_instance_id = 0;
    int total_generate_calls = 0;
    int total_update_calls = 0;
    int total_record_calls = 0;
    std::vector<int> generate_instance_ids;
    std::vector<int> update_instance_ids;
    std::vector<int> record_instance_ids;
    std::vector<double> recorded_values;
    std::vector<std::string> recorded_names;
};

class SequenceRealFunction : public adef::RealFunction
{
public:
    SequenceRealFunction(std::vector<double> sequence,
                         std::shared_ptr<FunctionObserver> observer = nullptr,
                         unsigned int num_params = 1) :
        sequence_(std::move(sequence)),
        cursor_(0),
        num_params_(num_params),
        observer_(std::move(observer)),
        instance_id_(observer_ ? observer_->next_instance_id++ : 0)
    {
    }

    SequenceRealFunction(const SequenceRealFunction& rhs) :
        adef::RealFunction(rhs),
        sequence_(rhs.sequence_),
        cursor_(rhs.cursor_),
        num_params_(rhs.num_params_),
        observer_(rhs.observer_),
        instance_id_(observer_ ? observer_->next_instance_id++ : rhs.instance_id_)
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    double generate() override
    {
        const double val = sequence_.at(cursor_ % sequence_.size());
        ++cursor_;
        if (observer_) {
            ++observer_->total_generate_calls;
            observer_->generate_instance_ids.push_back(instance_id_);
        }
        return val;
    }

    bool record(const std::vector<std::any>& params,
                const std::string& name = "") override
    {
        if (observer_) {
            ++observer_->total_record_calls;
            observer_->record_instance_ids.push_back(instance_id_);
            observer_->recorded_names.push_back(name);
            for (const auto& p : params) {
                observer_->recorded_values.push_back(std::any_cast<double>(p));
            }
        }
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
        if (observer_) {
            ++observer_->total_update_calls;
            observer_->update_instance_ids.push_back(instance_id_);
        }
    }

    unsigned int number_of_parameters() const override
    {
        return num_params_;
    }

private:
    std::vector<double> sequence_;
    std::size_t cursor_;
    unsigned int num_params_;
    std::shared_ptr<FunctionObserver> observer_;
    int instance_id_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<SequenceRealFunction>(*this);
    }
};

// Spy ControlUpdate that logs calls and records a marker into the target function
class LoggingControlUpdate : public adef::ControlUpdate
{
public:
    LoggingControlUpdate(std::string label,
                         std::shared_ptr<std::vector<std::string>> log,
                         std::shared_ptr<std::vector<std::shared_ptr<adef::BaseFunction>>> targets = nullptr,
                         bool invoke_record = false) :
        label_(std::move(label)),
        log_(std::move(log)),
        targets_(std::move(targets)),
        invoke_record_(invoke_record)
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    void update(std::shared_ptr<adef::Repository>,
                std::shared_ptr<adef::BaseControlParameter>,
                std::shared_ptr<adef::BaseFunction> function) const override
    {
        if (log_) {
            log_->push_back(label_);
        }
        if (targets_) {
            targets_->push_back(function);
        }
        if (invoke_record_ && function) {
            (void)function->record({ 1.0 }, label_);
        }
    }

private:
    std::string label_;
    std::shared_ptr<std::vector<std::string>> log_;
    std::shared_ptr<std::vector<std::shared_ptr<adef::BaseFunction>>> targets_;
    bool invoke_record_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<LoggingControlUpdate>(*this);
    }
};

class LoggingControlSelection : public adef::ControlSelection
{
public:
    LoggingControlSelection(std::string label,
                            std::shared_ptr<std::vector<std::string>> log,
                            std::shared_ptr<std::vector<std::shared_ptr<adef::BaseFunction>>> targets = nullptr,
                            bool invoke_record = false) :
        label_(std::move(label)),
        log_(std::move(log)),
        targets_(std::move(targets)),
        invoke_record_(invoke_record)
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    void select(std::shared_ptr<adef::Repository>,
                std::shared_ptr<adef::BaseControlParameter>,
                std::shared_ptr<adef::BaseFunction> function) const override
    {
        if (log_) {
            log_->push_back(label_);
        }
        if (targets_) {
            targets_->push_back(function);
        }
        if (invoke_record_ && function) {
            (void)function->record({ 2.0 }, label_);
        }
    }

private:
    std::string label_;
    std::shared_ptr<std::vector<std::string>> log_;
    std::shared_ptr<std::vector<std::shared_ptr<adef::BaseFunction>>> targets_;
    bool invoke_record_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<LoggingControlSelection>(*this);
    }
};

class LoggingRealFunction : public adef::RealFunction
{
public:
    LoggingRealFunction(std::string label,
                        double value,
                        std::shared_ptr<std::vector<std::string>> log) :
        label_(std::move(label)),
        value_(value),
        log_(std::move(log))
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    double generate() override
    {
        if (log_) {
            log_->push_back(label_ + "_generate");
        }
        return value_;
    }

    bool record(const std::vector<std::any>&,
                const std::string& name = "") override
    {
        if (log_) {
            log_->push_back(label_ + "_record:" + name);
        }
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
        if (log_) {
            log_->push_back(label_ + "_update");
        }
    }

    unsigned int number_of_parameters() const override
    {
        return 1;
    }

private:
    std::string label_;
    double value_;
    std::shared_ptr<std::vector<std::string>> log_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<LoggingRealFunction>(*this);
    }
};

class CompositeRootSpyFunction : public adef::RealFunction
{
public:
    explicit CompositeRootSpyFunction(std::shared_ptr<std::vector<std::string>> log) :
        log_(std::move(log))
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
        auto mean = std::make_shared<LoggingRealFunction>("mean", 0.5, log_);
        mean->set_function_name("mean");
        adef::BaseFunction::add_function(mean);

        auto stddev = std::make_shared<LoggingRealFunction>("stddev", 0.1, log_);
        stddev->set_function_name("stddev");
        adef::BaseFunction::add_function(stddev);
    }

    double generate() override
    {
        if (log_) {
            log_->push_back("root_generate");
        }
        return 0.5;
    }

    bool record(const std::vector<std::any>&,
                const std::string& name = "") override
    {
        if (log_) {
            log_->push_back("root_record:" + name);
        }
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
        if (log_) {
            log_->push_back("root_update");
        }
        get_function("mean")->update();
        get_function("stddev")->update();
    }

    unsigned int number_of_parameters() const override
    {
        return 0;
    }

private:
    std::shared_ptr<std::vector<std::string>> log_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<CompositeRootSpyFunction>(*this);
    }
};

template<typename T>
class LoggingMultipleControlParameter : public adef::MultipleControlParameter<T>
{
public:
    LoggingMultipleControlParameter(std::string label,
                                    std::shared_ptr<std::vector<std::string>> log) :
        label_(std::move(label)),
        log_(std::move(log))
    {
    }

    void reset_already_generated(std::size_t index) override
    {
        if (log_) {
            log_->push_back(label_ + "_reset");
        }
        adef::MultipleControlParameter<T>::reset_already_generated(index);
    }

private:
    std::string label_;
    std::shared_ptr<std::vector<std::string>> log_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<LoggingMultipleControlParameter>(*this);
    }
};

class LoggingControlFunction : public adef::RealControlFunction
{
public:
    LoggingControlFunction(std::shared_ptr<adef::RealFunction> leaf,
                           std::shared_ptr<std::vector<std::string>> log) :
        leaf_(std::move(leaf)),
        log_(std::move(log))
    {
    }

    LoggingControlFunction(const LoggingControlFunction& rhs) :
        adef::RealControlFunction(rhs),
        leaf_(rhs.leaf_ ? rhs.leaf_->clone() : nullptr),
        log_(rhs.log_)
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    std::shared_ptr<adef::RealFunction> generate() override
    {
        if (log_) {
            log_->push_back("inner_ctrl_fn_generate");
        }
        return leaf_;
    }

    bool record(const std::vector<std::any>&,
                const std::string& name = "") override
    {
        if (log_) {
            log_->push_back("inner_ctrl_fn_record:" + name);
        }
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
        if (log_) {
            log_->push_back("inner_ctrl_fn_update");
        }
    }

    unsigned int number_of_parameters() const override
    {
        return 1;
    }

private:
    std::shared_ptr<adef::RealFunction> leaf_;
    std::shared_ptr<std::vector<std::string>> log_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<LoggingControlFunction>(*this);
    }
};

// Deep-copied stateful components for clone() isolation verification
class StatefulCounterUpdate : public adef::ControlUpdate
{
public:
    StatefulCounterUpdate() : count_(0) {}
    StatefulCounterUpdate(const StatefulCounterUpdate& rhs) :
        adef::ControlUpdate(rhs),
        count_(rhs.count_)
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    void update(std::shared_ptr<adef::Repository>,
                std::shared_ptr<adef::BaseControlParameter>,
                std::shared_ptr<adef::BaseFunction> function) const override
    {
        ++count_;
        if (function) {
            (void)function->record({ static_cast<double>(count_) }, "cu_count");
        }
    }

private:
    mutable int count_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<StatefulCounterUpdate>(*this);
    }
};

class StatefulValueFunction : public adef::RealFunction
{
public:
    StatefulValueFunction() :
        update_count_(0),
        cu_count_(0.0),
        selected_sum_(0.0)
    {
    }

    StatefulValueFunction(const StatefulValueFunction& rhs) = default;

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    double generate() override
    {
        return 0.1 + 0.05 * static_cast<double>(update_count_) +
               0.01 * cu_count_ + 0.001 * selected_sum_;
    }

    bool record(const std::vector<std::any>& params,
                const std::string& name = "") override
    {
        if (name == "cu_count") {
            cu_count_ = std::any_cast<double>(params.front());
            return true;
        }
        if (name == "object") {
            selected_sum_ += std::any_cast<double>(params.front());
            return true;
        }
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
        ++update_count_;
    }

    unsigned int number_of_parameters() const override
    {
        return 1;
    }

private:
    int update_count_;
    double cu_count_;
    double selected_sum_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<StatefulValueFunction>(*this);
    }
};

class StatefulMeanCompositeFunction : public adef::RealFunction
{
public:
    StatefulMeanCompositeFunction() = default;
    StatefulMeanCompositeFunction(const StatefulMeanCompositeFunction& rhs) = default;

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
        auto mean = std::make_shared<StatefulValueFunction>();
        mean->set_function_name("mean");
        adef::BaseFunction::add_function(mean);
    }

    double generate() override
    {
        return get_function("mean")->generate();
    }

    void update() override
    {
        get_function("mean")->update();
    }

    unsigned int number_of_parameters() const override
    {
        return 1;
    }

private:
    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<StatefulMeanCompositeFunction>(*this);
    }
};

class CountingRandom : public adef::Random
{
public:
    explicit CountingRandom(SeedType seed_value) :
        adef::Random(seed_value),
        draw_count_(0)
    {
    }

    CountingRandom(const CountingRandom& rhs) :
        adef::Random(rhs),
        draw_count_(rhs.draw_count_)
    {
    }

    result_type next() override
    {
        ++draw_count_;
        return adef::Random::next();
    }

    std::size_t draw_count() const noexcept { return draw_count_; }

private:
    std::size_t draw_count_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<CountingRandom>(*this);
    }
};

struct SdeOrderTrace {
    std::size_t draws_after_init = 0;
    std::vector<std::size_t> target_indices;
    std::vector<std::size_t> draws_before_update;
    std::vector<std::size_t> draws_after_update;
    std::vector<double> param0_observed_at_update;
};

class TracingSdeFControlUpdate : public adef::SdeFControlUpdate
{
public:
    explicit TracingSdeFControlUpdate(std::shared_ptr<SdeOrderTrace> trace) :
        trace_(std::move(trace))
    {
    }

    void update(std::shared_ptr<adef::Repository> repos,
                std::shared_ptr<adef::BaseControlParameter> parameter,
                std::shared_ptr<adef::BaseFunction> function) const override
    {
        auto rng = std::dynamic_pointer_cast<CountingRandom>(repos->random());
        auto param = std::dynamic_pointer_cast<adef::ControlParameter<double>>(parameter);
        const std::size_t target_idx = repos->parameters()->take_out<std::size_t>("target_index");

        if (trace_ && rng && param) {
            trace_->target_indices.push_back(target_idx);
            trace_->draws_before_update.push_back(rng->draw_count());
            trace_->param0_observed_at_update.push_back(param->load(0));
        }

        adef::SdeFControlUpdate::update(repos, parameter, function);

        if (trace_ && rng) {
            trace_->draws_after_update.push_back(rng->draw_count());
        }
    }

private:
    std::shared_ptr<SdeOrderTrace> trace_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<TracingSdeFControlUpdate>(*this);
    }
};

// ============================================================================
// 2a. Base ControlMechanism lifecycle, resampling, and caching
// ============================================================================
bool test_control_mechanism_lifecycle_resampling_and_caching()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);

    auto obs = std::make_shared<FunctionObserver>();
    // First valid value is 0.75 (after rejecting -0.5 and 1.5),
    // then boundary values 0.0 and 1.0, then 0.25.
    pm.register_type("SeqFn", std::make_shared<SequenceRealFunction>(
        std::vector<double>{-0.5, 1.5, 0.75, 0.0, 1.0, 0.25}, obs));

    auto log = std::make_shared<std::vector<std::string>>();
    pm.register_type("LogCU", std::make_shared<LoggingControlUpdate>("cu", log));

    auto cfg = load_json_config(R"({
        "classname": "RealControlMechanism",
        "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
        "ControlParameter": { "classname": "RealSingleControlParameter" },
        "ControlFunction": {
            "classname": "SingleControlFunction",
            "Function": { "classname": "SeqFn" }
        },
        "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
        "ControlUpdate": [ { "classname": "LogCU" } ]
    })", "test_cm_lifecycle.json");

    auto cm = adef::make_and_setup_type<adef::RealControlMechanism>(cfg, pm);

    auto repos = std::make_shared<TestRepository>();
    repos->set_population(make_population_with_fitness({10.0}, pm, "lc_parent"));
    repos->set_offspring(make_population_with_fitness({12.0}, pm, "lc_offspring"));
    repos->parameters()->store("target_index", std::size_t{0});
    cm->init(repos);

    // update() -> generate() -> select()
    cm->update(repos);
    if (!require(log->size() == 1 && (*log)[0] == "cu" && obs->total_update_calls == 1,
                 "Expected update() to invoke ControlUpdate and Function::update()")) {
        return false;
    }

    // Resampling until ControlRange::is_valid() is satisfied
    const double v1 = cm->generate(repos);
    if (!require(approx_equal(v1, 0.75) && obs->total_generate_calls == 3,
                 "Expected generate() to resample out-of-range values (-0.5, 1.5) until valid (0.75)")) {
        return false;
    }

    // Caching via already_generated: second generate() returns cached value without calling Function::generate()
    const double v1_cached = cm->generate(repos);
    if (!require(approx_equal(v1_cached, 0.75) && obs->total_generate_calls == 3,
                 "Expected second generate() without update() to return cached value")) {
        return false;
    }

    // select() with worse offspring (12.0 >= 10.0): must NOT record
    cm->select(repos);
    if (!require(obs->total_record_calls == 0,
                 "Expected BetterOffspringControlSelection not to record when offspring is not better")) {
        return false;
    }

    // select() with better offspring (5.0 < 10.0): must record cached 0.75 under "object"
    repos->offspring()->at(0)->set_fitness_value(5.0);
    cm->select(repos);
    if (!require(obs->total_record_calls == 1 &&
                     obs->recorded_names.size() == 1 &&
                     obs->recorded_names[0] == "object" &&
                     obs->recorded_values.size() == 1 &&
                     approx_equal(obs->recorded_values[0], 0.75),
                 "Expected select() to record cached generated value 0.75 under \"object\"")) {
        return false;
    }

    // update() resets already_generated so next generate() draws fresh boundary value 0.0
    cm->update(repos);
    const double v2 = cm->generate(repos);
    if (!require(approx_equal(v2, 0.0) && obs->total_generate_calls == 4 &&
                     obs->total_update_calls == 2,
                 "Expected update() to reset cache and accept lower boundary 0.0")) {
        return false;
    }

    // Next cycle accepts upper boundary 1.0
    cm->update(repos);
    const double v3 = cm->generate(repos);
    if (!require(approx_equal(v3, 1.0) && obs->total_generate_calls == 5,
                 "Expected upper boundary 1.0 to be accepted without resampling")) {
        return false;
    }

    return true;
}

// ============================================================================
// 2b. Function / Parameter Granularity Matrix (2 x 2)
// ============================================================================
bool test_granularity_matrix()
{
    constexpr std::size_t kN = 4;

    // 1) SingleControlFunction + SingleControlParameter
    {
        adef::PrototypeManager pm;
        adef::register_type(pm);
        auto obs = std::make_shared<FunctionObserver>();
        pm.register_type("SeqFn", std::make_shared<SequenceRealFunction>(
            std::vector<double>{0.1, 0.2, 0.3, 0.4, 0.5, 0.6}, obs));

        auto cfg = load_json_config(R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealSingleControlParameter" },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": { "classname": "SeqFn" }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })", "test_cm_mat_ss.json");

        auto cm = adef::make_and_setup_type<adef::RealControlMechanism>(cfg, pm);
        auto repos = std::make_shared<TestRepository>();
        cm->init(repos);

        // Generating at index 0 populates the single shared slot; index 1 hits the same cache
        repos->parameters()->store("target_index", std::size_t{0});
        cm->update(repos);
        const double g0 = cm->generate(repos);

        repos->parameters()->store("target_index", std::size_t{1});
        const double g1_cached = cm->generate(repos);
        if (!require(approx_equal(g0, 0.1) && approx_equal(g1_cached, 0.1) &&
                         obs->total_generate_calls == 1,
                     "Single+Single: expected index 1 to hit single shared cache")) {
            return false;
        }

        // Updating at index 1 updates the single shared function and resets the single flag
        cm->update(repos);
        const double g1_fresh = cm->generate(repos);
        if (!require(approx_equal(g1_fresh, 0.2) && obs->total_generate_calls == 2 &&
                         obs->update_instance_ids.size() == 2 &&
                         obs->update_instance_ids[0] == obs->update_instance_ids[1],
                     "Single+Single: expected shared function and single reset flag")) {
            return false;
        }
    }

    // 2) SingleControlFunction + MultipleControlParameter (as in SaDE)
    {
        adef::PrototypeManager pm;
        adef::register_type(pm);
        auto obs = std::make_shared<FunctionObserver>();
        pm.register_type("SeqFn", std::make_shared<SequenceRealFunction>(
            std::vector<double>{0.11, 0.22, 0.33, 0.44, 0.55, 0.66, 0.77, 0.88}, obs));

        auto cfg = load_json_config(R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 4 },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": { "classname": "SeqFn" }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })", "test_cm_mat_sm.json");

        auto cm = adef::make_and_setup_type<adef::RealControlMechanism>(cfg, pm);
        auto repos = std::make_shared<TestRepository>();
        cm->init(repos);

        // Generation 1: all N indices generate distinct values from the single shared function
        std::vector<double> gen1(kN);
        for (std::size_t idx = 0; idx < kN; ++idx) {
            repos->parameters()->store("target_index", idx);
            cm->update(repos);
            gen1[idx] = cm->generate(repos);
        }
        if (!require(obs->total_generate_calls == static_cast<int>(kN) &&
                         obs->total_update_calls == static_cast<int>(kN),
                     "Single+Multiple: expected N updates and N generates in generation 1")) {
            return false;
        }
        // All updates and generates targeted the single shared function instance
        for (std::size_t idx = 1; idx < kN; ++idx) {
            if (!require(obs->update_instance_ids[idx] == obs->update_instance_ids[0] &&
                             obs->generate_instance_ids[idx] == obs->generate_instance_ids[0],
                         "Single+Multiple: expected all indices to share one Function instance")) {
                return false;
            }
        }
        // Re-calling generate() for every index 0..N-1 hits cache
        for (std::size_t idx = 0; idx < kN; ++idx) {
            repos->parameters()->store("target_index", idx);
            if (!require(approx_equal(cm->generate(repos), gen1[idx]),
                         "Single+Multiple: expected cached value per index in generation 1")) {
                return false;
            }
        }
        if (!require(obs->total_generate_calls == static_cast<int>(kN),
                     "Single+Multiple: expected zero extra generate() calls when cached")) {
            return false;
        }

        // Partial update on index 0 resets index 0 while leaving indices 1..N-1 cached
        repos->parameters()->store("target_index", std::size_t{0});
        cm->update(repos);
        for (std::size_t idx = 1; idx < kN; ++idx) {
            repos->parameters()->store("target_index", idx);
            if (!require(approx_equal(cm->generate(repos), gen1[idx]),
                         "Single+Multiple: expected indices 1..N-1 to remain cached after updating index 0 only")) {
                return false;
            }
        }
        if (!require(obs->total_generate_calls == static_cast<int>(kN),
                     "Single+Multiple: expected no regeneration for non-updated indices 1..N-1")) {
            return false;
        }

        // Complete generation 2 pass: explicitly verify already_generated is reset across ALL N indices
        std::vector<double> gen2(kN);
        repos->parameters()->store("target_index", std::size_t{0});
        gen2[0] = cm->generate(repos);
        for (std::size_t idx = 1; idx < kN; ++idx) {
            repos->parameters()->store("target_index", idx);
            cm->update(repos);
            gen2[idx] = cm->generate(repos);
        }
        if (!require(obs->total_generate_calls == static_cast<int>(2 * kN),
                     "Single+Multiple: expected all N indices to regenerate in generation 2")) {
            return false;
        }
        for (std::size_t idx = 0; idx < kN; ++idx) {
            if (!require(!approx_equal(gen2[idx], gen1[idx]),
                         "Single+Multiple: expected fresh generation 2 value at index " +
                             std::to_string(idx))) {
                return false;
            }
        }
    }

    // 3) MultipleControlFunction + SingleControlParameter
    {
        adef::PrototypeManager pm;
        adef::register_type(pm);
        auto obs = std::make_shared<FunctionObserver>();
        pm.register_type("SeqFn", std::make_shared<SequenceRealFunction>(
            std::vector<double>{0.2, 0.4, 0.6, 0.8}, obs));

        auto cfg = load_json_config(R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealSingleControlParameter" },
            "ControlFunction": {
                "classname": "MultipleControlFunction",
                "number_of_functions": 4,
                "Function": { "classname": "SeqFn" }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })", "test_cm_mat_ms.json");

        auto cm = adef::make_and_setup_type<adef::RealControlMechanism>(cfg, pm);
        auto repos = std::make_shared<TestRepository>();
        cm->init(repos);

        repos->parameters()->store("target_index", std::size_t{0});
        cm->update(repos);
        const double v0 = cm->generate(repos); // invokes function[0], cursor -> 1

        // Calling generate() at index 1 without update() hits the single shared parameter cache
        repos->parameters()->store("target_index", std::size_t{1});
        const double v1_cached = cm->generate(repos);
        if (!require(approx_equal(v0, 0.2) && approx_equal(v1_cached, 0.2) &&
                         obs->total_generate_calls == 1,
                     "Multiple+Single: expected single parameter cache to serve index 1 before update")) {
            return false;
        }

        // Calling update() at index 1 updates function[1] (distinct instance from function[0]) and resets single flag
        cm->update(repos);
        const double v1_fresh = cm->generate(repos); // invokes function[1] at its own cursor 0 -> 0.2
        if (!require(approx_equal(v1_fresh, 0.2) && obs->total_generate_calls == 2 &&
                         obs->update_instance_ids.size() == 2 &&
                         obs->update_instance_ids[0] != obs->update_instance_ids[1] &&
                         obs->generate_instance_ids[0] != obs->generate_instance_ids[1],
                     "Multiple+Single: expected distinct Function instances per index sharing one parameter slot")) {
            return false;
        }
    }

    // 4) MultipleControlFunction + MultipleControlParameter (as in ISADE / SDE)
    {
        adef::PrototypeManager pm;
        adef::register_type(pm);
        auto obs = std::make_shared<FunctionObserver>();
        pm.register_type("SeqFn", std::make_shared<SequenceRealFunction>(
            std::vector<double>{0.15, 0.35, 0.55, 0.75}, obs));

        auto cfg = load_json_config(R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 4 },
            "ControlFunction": {
                "classname": "MultipleControlFunction",
                "number_of_functions": 4,
                "Function": { "classname": "SeqFn" }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })", "test_cm_mat_mm.json");

        auto cm = adef::make_and_setup_type<adef::RealControlMechanism>(cfg, pm);
        auto repos = std::make_shared<TestRepository>();
        cm->init(repos);

        // Generate all 4 indices: each function instance starts at cursor 0 -> 0.15
        for (std::size_t idx = 0; idx < kN; ++idx) {
            repos->parameters()->store("target_index", idx);
            cm->update(repos);
            const double val = cm->generate(repos);
            if (!require(approx_equal(val, 0.15),
                         "Multiple+Multiple: expected independent initial function cursor at index " +
                             std::to_string(idx))) {
                return false;
            }
        }
        std::set<int> unique_fn_ids(obs->update_instance_ids.begin(), obs->update_instance_ids.end());
        if (!require(unique_fn_ids.size() == kN,
                     "Multiple+Multiple: expected N distinct Function instances")) {
            return false;
        }

        // Update & regenerate ONLY index 2 -> advances index 2 to 0.35 while indices 0, 1, 3 stay cached at 0.15
        repos->parameters()->store("target_index", std::size_t{2});
        cm->update(repos);
        const double v2_next = cm->generate(repos);
        if (!require(approx_equal(v2_next, 0.35) && obs->total_generate_calls == static_cast<int>(kN + 1),
                     "Multiple+Multiple: expected index 2 to advance independently to 0.35")) {
            return false;
        }

        for (std::size_t idx : {std::size_t{0}, std::size_t{1}, std::size_t{3}}) {
            repos->parameters()->store("target_index", idx);
            if (!require(approx_equal(cm->generate(repos), 0.15),
                         "Multiple+Multiple: expected untouched index to remain cached at 0.15")) {
                return false;
            }
        }
        if (!require(obs->total_generate_calls == static_cast<int>(kN + 1),
                     "Multiple+Multiple: expected untouched indices not to regenerate")) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 2c. SadeCrControlMechanism routing to "mean" child + root update()
// ============================================================================
bool test_sade_cr_control_mechanism_routing()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);

    auto log = std::make_shared<std::vector<std::string>>();
    auto cu_targets = std::make_shared<std::vector<std::shared_ptr<adef::BaseFunction>>>();
    auto cs_targets = std::make_shared<std::vector<std::shared_ptr<adef::BaseFunction>>>();

    pm.register_type("CompositeRootSpy", std::make_shared<CompositeRootSpyFunction>(log));
    pm.register_type("SpyCU", std::make_shared<LoggingControlUpdate>("cu", log, cu_targets, true));
    pm.register_type("SpyCS", std::make_shared<LoggingControlSelection>("cs", log, cs_targets, true));

    auto cfg = load_json_config(R"({
        "classname": "SadeCrControlMechanism",
        "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
        "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 2 },
        "ControlFunction": {
            "classname": "SingleControlFunction",
            "Function": { "classname": "CompositeRootSpy" }
        },
        "ControlSelection": { "classname": "SpyCS" },
        "ControlUpdate": [ { "classname": "SpyCU" } ]
    })", "test_cm_sade_routing.json");

    auto cm = adef::make_and_setup_type<adef::SadeCrControlMechanism>(cfg, pm);
    auto repos = std::make_shared<TestRepository>();
    repos->parameters()->store("target_index", std::size_t{0});
    cm->init(repos);

    // update(): routes ControlUpdate recording to "mean" child, then calls update() on root
    cm->update(repos);
    const std::vector<std::string> expected_update_log = {
        "cu",
        "mean_record:cu",
        "root_update",
        "mean_update",
        "stddev_update"
    };
    if (!require(*log == expected_update_log &&
                     cu_targets->size() == 1 &&
                     (*cu_targets)[0]->function_name() == "mean",
                 "SadeCrControlMechanism::update() did not route update to \"mean\" and invoke root::update()")) {
        return false;
    }

    log->clear();
    const double val = cm->generate(repos);
    if (!require(approx_equal(val, 0.5) && *log == std::vector<std::string>{"root_generate"},
                 "SadeCrControlMechanism::generate() should invoke root function")) {
        return false;
    }

    // select(): routes ControlSelection recording specifically to "mean" child
    log->clear();
    cm->select(repos);
    const std::vector<std::string> expected_select_log = {
        "cs",
        "mean_record:cs"
    };
    if (!require(*log == expected_select_log &&
                     cs_targets->size() == 1 &&
                     (*cs_targets)[0]->function_name() == "mean",
                 "SadeCrControlMechanism::select() did not route selection recording to \"mean\" child")) {
        return false;
    }

    return true;
}

// ============================================================================
// 2d. SdeFControlMechanism truncation without resampling & in-place visibility
// ============================================================================
bool test_sde_f_control_mechanism()
{
    // Part 1: Out-of-range truncation via std::abs(x - std::trunc(x)) once without resampling
    {
        adef::PrototypeManager pm;
        adef::register_type(pm);

        auto obs = std::make_shared<FunctionObserver>();
        // Sequence: 1.35 -> 0.35, -2.25 -> 0.25, 2.0 -> 0.0, 0.65 -> 0.65 (valid)
        pm.register_type("SeqFn", std::make_shared<SequenceRealFunction>(
            std::vector<double>{1.35, -2.25, 2.0, 0.65}, obs));

        auto cfg = load_json_config(R"({
            "classname": "SdeFControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealSingleControlParameter" },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": { "classname": "SeqFn" }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })", "test_cm_sde_trunc.json");

        auto cm = adef::make_and_setup_type<adef::SdeFControlMechanism>(cfg, pm);
        auto repos = std::make_shared<TestRepository>();
        repos->parameters()->store("target_index", std::size_t{0});
        cm->init(repos);

        const std::vector<double> expected = {0.35, 0.25, 0.0, 0.65};
        for (std::size_t step = 0; step < expected.size(); ++step) {
            cm->update(repos);
            const double val = cm->generate(repos);
            if (!require(approx_equal(val, expected[step]) &&
                             obs->total_generate_calls == static_cast<int>(step + 1),
                         "SdeFControlMechanism truncation mismatch at step " + std::to_string(step))) {
                return false;
            }
            // Repeated generate() uses cached truncated value
            if (!require(approx_equal(cm->generate(repos), expected[step]) &&
                             obs->total_generate_calls == static_cast<int>(step + 1),
                         "SdeFControlMechanism failed to cache generated value")) {
                return false;
            }
        }
    }

    // Part 2: Prove single truncation WITHOUT resampling even when truncated result is still outside range
    {
        adef::PrototypeManager pm;
        adef::register_type(pm);

        auto obs = std::make_shared<FunctionObserver>();
        // Range is [0.4, 1.0]; 1.1 truncates once to 0.1 (still < 0.4) and must NOT resample 0.8
        pm.register_type("SeqFn", std::make_shared<SequenceRealFunction>(
            std::vector<double>{1.1, 0.8}, obs));

        auto cfg = load_json_config(R"({
            "classname": "SdeFControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.4, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealSingleControlParameter" },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": { "classname": "SeqFn" }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": []
        })", "test_cm_sde_no_resample.json");

        auto cm = adef::make_and_setup_type<adef::SdeFControlMechanism>(cfg, pm);
        auto repos = std::make_shared<TestRepository>();
        repos->parameters()->store("target_index", std::size_t{0});
        cm->init(repos);

        cm->update(repos);
        const double val = cm->generate(repos);
        if (!require(approx_equal(val, 0.1) && obs->total_generate_calls == 1,
                     "SdeFControlMechanism must truncate once without resampling even if still outside range")) {
            return false;
        }
    }

    // Part 3: Newly saved F values are immediately visible in-place to subsequent individuals in the same generation
    {
        adef::PrototypeManager pm;
        adef::register_type(pm);

        auto obs = std::make_shared<FunctionObserver>();
        // Each individual's function requests all 3 parameter values (num_params = 3 == pop_size)
        // and generates 0.88 on its first call.
        pm.register_type("SpySdeFn", std::make_shared<SequenceRealFunction>(
            std::vector<double>{0.88, 0.77, 0.66}, obs, 3u));

        // Deterministic initializer function for RealMultipleControlParameter: {0.11, 0.22, 0.33}
        pm.register_type("InitSeqFn", std::make_shared<SequenceRealFunction>(
            std::vector<double>{0.11, 0.22, 0.33}, nullptr, 0u));

        auto cfg = load_json_config(R"({
            "classname": "SdeFControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": {
                "classname": "RealMultipleControlParameter",
                "number_of_objects": 3,
                "initial_value": { "classname": "InitSeqFn" }
            },
            "ControlFunction": {
                "classname": "MultipleControlFunction",
                "number_of_functions": 3,
                "Function": { "classname": "SpySdeFn" }
            },
            "ControlSelection": { "classname": "NonInfoControlSelection" },
            "ControlUpdate": [ { "classname": "SdeFControlUpdate" } ]
        })", "test_cm_sde_inplace.json");

        auto cm = adef::make_and_setup_type<adef::SdeFControlMechanism>(cfg, pm);
        auto repos = std::make_shared<TestRepository>();
        repos->set_population(make_population_with_fitness({1.0, 2.0, 3.0}, pm, "sde_inplace"));
        repos->set_random(std::make_shared<adef::Random>(55u));
        cm->init(repos);

        // Individual 0: update() reads initial {0.11, 0.22, 0.33}, then generate() saves 0.88 in-place at slot 0
        repos->parameters()->store("target_index", std::size_t{0});
        cm->update(repos);
        const std::vector<double> ind0_seen(obs->recorded_values.begin(), obs->recorded_values.end());
        const double f0 = cm->generate(repos);
        if (!require(approx_equal(f0, 0.88) && ind0_seen.size() == 3,
                     "SdeFControlMechanism: expected individual 0 to generate 0.88")) {
            return false;
        }
        {
            std::set<double> seen_set(ind0_seen.begin(), ind0_seen.end());
            std::set<double> initial_set{0.11, 0.22, 0.33};
            if (!require(seen_set == initial_set,
                         "SdeFControlMechanism: expected individual 0 to observe initial {0.11, 0.22, 0.33}")) {
                return false;
            }
        }

        // Individual 1 in the SAME generation: update() MUST immediately observe 0.88 (not 0.11) from slot 0
        obs->recorded_values.clear();
        repos->parameters()->store("target_index", std::size_t{1});
        cm->update(repos);
        std::set<double> ind1_seen_set(obs->recorded_values.begin(), obs->recorded_values.end());
        std::set<double> expected_inplace_set{0.88, 0.22, 0.33};
        if (!require(ind1_seen_set == expected_inplace_set,
                     "SdeFControlMechanism: expected individual 1 to immediately observe in-place updated F=0.88 at slot 0")) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 2e. IndirectControlMechanism delegation ordering in update() and select()
// ============================================================================
bool test_indirect_control_mechanism_delegation_ordering()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);

    auto log = std::make_shared<std::vector<std::string>>();
    auto outer_cu_targets = std::make_shared<std::vector<std::shared_ptr<adef::BaseFunction>>>();
    auto outer_cs_targets = std::make_shared<std::vector<std::shared_ptr<adef::BaseFunction>>>();
    auto inner_cs_targets = std::make_shared<std::vector<std::shared_ptr<adef::BaseFunction>>>();

    auto leaf_fn = std::make_shared<LoggingRealFunction>("leaf", 0.5, log);

    pm.register_type("LogOuterParam",
                     std::make_shared<LoggingMultipleControlParameter<double>>("outer_param", log));
    pm.register_type("LogInnerParam",
                     std::make_shared<LoggingMultipleControlParameter<std::shared_ptr<adef::RealFunction>>>("inner_param", log));
    pm.register_type("LogInnerCtrlFn",
                     std::make_shared<LoggingControlFunction>(leaf_fn, log));
    pm.register_type("OuterCU",
                     std::make_shared<LoggingControlUpdate>("outer_update", log, outer_cu_targets));
    pm.register_type("InnerCU",
                     std::make_shared<LoggingControlUpdate>("inner_update", log));
    pm.register_type("OuterCS",
                     std::make_shared<LoggingControlSelection>("outer_select", log, outer_cs_targets));
    pm.register_type("InnerCS",
                     std::make_shared<LoggingControlSelection>("inner_select", log, inner_cs_targets));

    auto cfg = load_json_config(R"({
        "classname": "RealIndirectControlMechanism",
        "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
        "ControlParameter": { "classname": "LogOuterParam", "number_of_objects": 2 },
        "ControlSelection": { "classname": "OuterCS" },
        "ControlUpdate": [ { "classname": "OuterCU" } ],
        "ControlMechanism": {
            "classname": "RealControlControlMechanism",
            "ControlRange": { "classname": "RealControlControlRange" },
            "ControlParameter": { "classname": "LogInnerParam", "number_of_objects": 2 },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": { "classname": "LogInnerCtrlFn" }
            },
            "ControlSelection": { "classname": "InnerCS" },
            "ControlUpdate": [ { "classname": "InnerCU" } ]
        }
    })", "test_cm_indirect_order.json");

    auto cm = adef::make_and_setup_type<adef::RealIndirectControlMechanism>(cfg, pm);
    auto repos = std::make_shared<TestRepository>();
    repos->parameters()->store("target_index", std::size_t{0});
    cm->init(repos);

    // Cold-start update() + generate():
    // update() calls cm_->generate() BEFORE cm_->update() resets inner cache,
    // so subsequent outer generate() calls inner_ctrl_fn_generate a second time.
    cm->update(repos);
    const double g1 = cm->generate(repos);
    const std::vector<std::string> expected_cold_cycle = {
        "inner_ctrl_fn_generate", // cm_->generate(repos) inside outer update()
        "outer_update",           // outer updates_ receive generated leaf function
        "leaf_update",            // cm_->generate(repos)->update() (inner cache hit)
        "outer_param_reset",      // parameter_->reset_already_generated(index)
        "inner_update",           // cm_->update(repos) -> inner updates_
        "inner_ctrl_fn_update",   // cm_->update(repos) -> inner function update()
        "inner_param_reset",      // cm_->update(repos) -> inner parameter reset
        "inner_ctrl_fn_generate", // outer generate() -> cm_->generate(repos) draws again
        "leaf_generate"           // leaf->generate()
    };
    if (!require(approx_equal(g1, 0.5) && *log == expected_cold_cycle,
                 "IndirectControlMechanism cold update()->generate() delegation ordering mismatch")) {
        return false;
    }

    // select() with warm inner cache: outer selection_->select(...) -> inner cm_->select(repos)
    log->clear();
    cm->select(repos);
    const std::vector<std::string> expected_select = {
        "outer_select",
        "inner_select"
    };
    if (!require(*log == expected_select &&
                     outer_cu_targets->size() == 1 &&
                     outer_cs_targets->size() == 1 &&
                     (*outer_cu_targets)[0] == (*outer_cs_targets)[0] &&
                     inner_cs_targets->size() == 1 &&
                     (*inner_cs_targets)[0] != (*outer_cs_targets)[0],
                 "IndirectControlMechanism::select() delegation ordering or target function mismatch")) {
        return false;
    }

    // Warm-cache update() (second generation): inner cache is warm from previous generate(),
    // so update() does NOT re-invoke inner_ctrl_fn_generate until after inner_param_reset.
    log->clear();
    cm->update(repos);
    const std::vector<std::string> expected_warm_update = {
        "outer_update",
        "leaf_update",
        "outer_param_reset",
        "inner_update",
        "inner_ctrl_fn_update",
        "inner_param_reset"
    };
    if (!require(*log == expected_warm_update,
                 "IndirectControlMechanism warm update() delegation ordering mismatch")) {
        return false;
    }

    return true;
}

// ============================================================================
// 2f. Prototype clone() isolation across all 4 ControlMechanism variants
// ============================================================================
bool test_prototype_clone_isolation()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);
    pm.register_type("StatefulCounterUpdate", std::make_shared<StatefulCounterUpdate>());
    pm.register_type("StatefulValueFunction", std::make_shared<StatefulValueFunction>());
    pm.register_type("StatefulMeanCompositeFunction", std::make_shared<StatefulMeanCompositeFunction>());

    // Helper: drives a ControlMechanism<double> over several generations/individuals
    // and returns all generated values.
    auto drive_cm = [&](std::shared_ptr<adef::ControlMechanism<double>> cm,
                        std::size_t pop_size,
                        unsigned int gens,
                        adef::Random::SeedType seed,
                        bool better_offspring,
                        const std::string& tag) {
        auto repos = std::make_shared<TestRepository>();
        repos->set_random(std::make_shared<adef::Random>(seed));
        repos->set_population(make_population_with_fitness(
            std::vector<double>(pop_size, 10.0), pm, "clone_p_" + tag));
        repos->set_offspring(make_population_with_fitness(
            std::vector<double>(pop_size, better_offspring ? 2.0 : 20.0), pm, "clone_o_" + tag));
        cm->init(repos);

        std::vector<double> generated;
        for (unsigned int gen = 1; gen <= gens; ++gen) {
            repos->parameters()->store("generation", gen);
            for (std::size_t idx = 0; idx < pop_size; ++idx) {
                repos->parameters()->store("target_index", idx);
                cm->update(repos);
                generated.push_back(cm->generate(repos));
            }
            for (std::size_t idx = 0; idx < pop_size; ++idx) {
                repos->parameters()->store("target_index", idx);
                cm->select(repos);
            }
        }
        return generated;
    };

    // Verify helper:
    // 1. Clone clean reference and capture pristine baseline BEFORE mutating anything.
    // 2. Initialize a live clone and cache index 0 BEFORE mutating worker clone.
    // 3. Clone worker and heavily mutate it (many gens, better offspring, distinct seed).
    // 4. Verify live clone's cached state was not corrupted by worker mutation.
    // 5. Clone fresh instance AFTER worker mutation and verify it and original prototype match baseline.
    auto verify_variant_isolation = [&](std::shared_ptr<adef::ControlMechanism<double>> proto,
                                        std::size_t pop_size,
                                        const std::string& label) {
        auto ref_clone = proto->clone();
        auto live_clone = proto->clone();
        auto mutated_clone = proto->clone();

        const auto ref_out = drive_cm(ref_clone, pop_size, 4, 1234u, true, label + "_ref");

        auto live_repos = std::make_shared<TestRepository>();
        live_repos->set_random(std::make_shared<adef::Random>(1234u));
        live_repos->set_population(make_population_with_fitness(
            std::vector<double>(pop_size, 10.0), pm, "clone_live_p_" + label));
        live_repos->set_offspring(make_population_with_fitness(
            std::vector<double>(pop_size, 2.0), pm, "clone_live_o_" + label));
        live_clone->init(live_repos);
        live_repos->parameters()->store("generation", 1u);
        live_repos->parameters()->store("target_index", std::size_t{0});
        live_clone->update(live_repos);
        const double live_cached_before = live_clone->generate(live_repos);

        const auto mutated_out = drive_cm(mutated_clone, pop_size, 6, 999u, true, label + "_mut");
        if (!require(!mutated_out.empty(), label + ": mutated clone produced no values")) {
            return false;
        }

        const double live_cached_after = live_clone->generate(live_repos);
        if (!require(approx_equal(live_cached_before, ref_out[0]) &&
                         approx_equal(live_cached_after, live_cached_before),
                     label + ": mutating cloned instance corrupted another live clone's cached state")) {
            return false;
        }

        auto post_clone = proto->clone();
        const auto post_out = drive_cm(post_clone, pop_size, 4, 1234u, true, label + "_post");
        const auto proto_out = drive_cm(proto, pop_size, 4, 1234u, true, label + "_proto");

        if (!require(ref_out == post_out && ref_out == proto_out,
                     label + ": mutating cloned instance leaked state into prototype")) {
            return false;
        }
        return true;
    };

    // Variant 1: RealControlMechanism (with stateful Function, MultipleControlParameter, and StatefulCounterUpdate)
    {
        auto cfg = load_json_config(R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 100.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 3 },
            "ControlFunction": {
                "classname": "MultipleControlFunction",
                "number_of_functions": 3,
                "Function": { "classname": "StatefulValueFunction" }
            },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [ { "classname": "StatefulCounterUpdate" } ]
        })", "test_cm_clone_base.json");
        auto proto = adef::make_and_setup_type<adef::RealControlMechanism>(cfg, pm);
        if (!verify_variant_isolation(proto, 3, "RealControlMechanism")) {
            return false;
        }
    }

    // Variant 2a: SadeCrControlMechanism (realistic SaDE CR with LearningPeriodFunction + RealMedianFunction)
    {
        auto cfg = load_json_config(R"({
            "classname": "SadeCrControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 4 },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": {
                    "classname": "RealNormalDisFunction",
                    "mean": {
                        "classname": "RealLearningPeriodFunction",
                        "learning_period": 2,
                        "object": {
                            "classname": "RealMedianFunction",
                            "storage_size": 8,
                            "initial_value": 0.5
                        }
                    },
                    "stddev": { "classname": "RealConstantFunction", "object": 0.1 }
                }
            },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [ { "classname": "GenerationControlUpdate" } ]
        })", "test_cm_clone_sade.json");
        auto proto = adef::make_and_setup_type<adef::SadeCrControlMechanism>(cfg, pm);
        if (!verify_variant_isolation(proto, 4, "SadeCrControlMechanism")) {
            return false;
        }
    }

    // Variant 2b: SadeCrControlMechanism with StatefulCounterUpdate targeting "mean"
    {
        auto cfg = load_json_config(R"({
            "classname": "SadeCrControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 100.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 3 },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": { "classname": "StatefulMeanCompositeFunction" }
            },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [ { "classname": "StatefulCounterUpdate" } ]
        })", "test_cm_clone_sade_cu.json");
        auto proto = adef::make_and_setup_type<adef::SadeCrControlMechanism>(cfg, pm);
        if (!verify_variant_isolation(proto, 3, "SadeCrControlMechanism(StatefulCU)")) {
            return false;
        }
    }

    // Variant 3: SdeFControlMechanism (Function-initialized MultipleControlParameter + SdeFFunction + SdeFControlUpdate)
    {
        auto cfg = load_json_config(R"({
            "classname": "SdeFControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": {
                "classname": "RealMultipleControlParameter",
                "number_of_objects": 4,
                "initial_value": {
                    "classname": "RealNormalDisFunction",
                    "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                    "stddev": { "classname": "RealConstantFunction", "object": 0.15 }
                }
            },
            "ControlFunction": {
                "classname": "MultipleControlFunction",
                "number_of_functions": 4,
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
            "ControlUpdate": [ { "classname": "SdeFControlUpdate" } ]
        })", "test_cm_clone_sde.json");
        auto proto = adef::make_and_setup_type<adef::SdeFControlMechanism>(cfg, pm);
        if (!verify_variant_isolation(proto, 4, "SdeFControlMechanism")) {
            return false;
        }
    }

    // Variant 3b: SdeFControlMechanism with StatefulCounterUpdate
    {
        auto cfg = load_json_config(R"({
            "classname": "SdeFControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 100.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 3 },
            "ControlFunction": {
                "classname": "MultipleControlFunction",
                "number_of_functions": 3,
                "Function": { "classname": "StatefulValueFunction" }
            },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [ { "classname": "StatefulCounterUpdate" } ]
        })", "test_cm_clone_sde_cu.json");
        auto proto = adef::make_and_setup_type<adef::SdeFControlMechanism>(cfg, pm);
        if (!verify_variant_isolation(proto, 3, "SdeFControlMechanism(StatefulCU)")) {
            return false;
        }
    }

    // Variant 4a: RealIndirectControlMechanism (nested LearningPeriod + RouletteWheel + GenerationControlUpdate)
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
                            "score_size": 10,
                            "object": [
                                {
                                    "classname": "RealNormalDisFunction",
                                    "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                                    "stddev": { "classname": "RealConstantFunction", "object": 0.3 }
                                },
                                {
                                    "classname": "RealCauchyDisFunction",
                                    "location": { "classname": "RealConstantFunction", "object": 0.5 },
                                    "scale": { "classname": "RealConstantFunction", "object": 0.1 }
                                }
                            ]
                        }
                    }
                },
                "ControlSelection": { "classname": "RealControlBetterOffspringControlSelection" },
                "ControlUpdate": [ { "classname": "GenerationControlUpdate" } ]
            }
        })", "test_cm_clone_indirect.json");
        auto proto = adef::make_and_setup_type<adef::RealIndirectControlMechanism>(cfg, pm);
        if (!verify_variant_isolation(proto, 4, "RealIndirectControlMechanism")) {
            return false;
        }
    }

    // Variant 4b: RealIndirectControlMechanism with StatefulCounterUpdate on outer mechanism
    {
        auto cfg = load_json_config(R"({
            "classname": "RealIndirectControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 100.0 },
            "ControlParameter": { "classname": "RealMultipleControlParameter", "number_of_objects": 3 },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [ { "classname": "StatefulCounterUpdate" } ],
            "ControlMechanism": {
                "classname": "RealControlControlMechanism",
                "ControlRange": { "classname": "RealControlControlRange" },
                "ControlParameter": { "classname": "RealControlMultipleControlParameter", "number_of_objects": 3 },
                "ControlFunction": {
                    "classname": "SingleControlFunction",
                    "Function": {
                        "classname": "RealControlConstantFunction",
                        "object": { "classname": "StatefulValueFunction" }
                    }
                },
                "ControlSelection": { "classname": "NonInfoControlSelection" },
                "ControlUpdate": []
            }
        })", "test_cm_clone_indirect_cu.json");
        auto proto = adef::make_and_setup_type<adef::RealIndirectControlMechanism>(cfg, pm);
        if (!verify_variant_isolation(proto, 3, "RealIndirectControlMechanism(StatefulCU)")) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 3a. Behavioral Characterization: RouletteWheelSelectionFunction::update()
// ============================================================================
bool test_roulette_wheel_repeated_update_characterization()
{
    adef::PrototypeManager pm;
    adef::register_type(pm);

    auto parent = std::make_shared<adef::Individual>();
    parent->set_fitness_value(10.0);
    auto better_offspring = std::make_shared<adef::Individual>();
    better_offspring->set_fitness_value(5.0);
    auto worse_offspring = std::make_shared<adef::Individual>();
    worse_offspring->set_fitness_value(15.0);

    // Part 1: Direct RouletteWheelSelectionFunction single update() vs repeated update()
    {
        auto cfg = load_json_config(R"({
            "classname": "RealRouletteWheelSelectionFunction",
            "scoring_function": { "classname": "SuccessScoringFunction" },
            "score_size": 10,
            "object": [1.0, 2.0]
        })", "test_cm_rws_direct.json");

        auto proto = adef::make_and_setup_type<adef::RealRouletteWheelSelectionFunction>(cfg, pm);
        auto once_fn = proto->clone();
        auto twice_fn = proto->clone();

        auto repos_a = std::make_shared<TestRepository>();
        repos_a->set_random(std::make_shared<adef::Random>(2026u));
        once_fn->init(repos_a);

        auto repos_b = std::make_shared<TestRepository>();
        repos_b->set_random(std::make_shared<adef::Random>(2026u));
        twice_fn->init(repos_b);

        // Record losses (score=0) for 1.0 (index 0) and wins (score=1) for 2.0 (index 1)
        for (int i = 0; i < 5; ++i) {
            once_fn->record({ 1.0 }, parent, worse_offspring);
            once_fn->record({ 2.0 }, parent, better_offspring);

            twice_fn->record({ 1.0 }, parent, worse_offspring);
            twice_fn->record({ 2.0 }, parent, better_offspring);
        }

        // Single update(): computes scores [0.0, 1.0] and clears scores_
        once_fn->update();

        // Repeated update(): 1st call computes [0.0, 1.0] and clears scores_;
        // 2nd call sees empty scores_ and overwrites both score_ values with 0.0.
        twice_fn->update();
        twice_fn->update();

        int once_picked_two = 0;
        int twice_picked_two = 0;
        constexpr int kDraws = 400;
        for (int i = 0; i < kDraws; ++i) {
            if (approx_equal(once_fn->generate(), 2.0)) {
                ++once_picked_two;
            }
            if (approx_equal(twice_fn->generate(), 2.0)) {
                ++twice_picked_two;
            }
        }

        // After single update(): 2.0 has score 1.0 vs 0.0 -> selected 100% of the time.
        // After repeated update(): scores are reset to [0.0, 0.0] -> 2.0 loses its dominance
        // and selection reverts to uniform sampling across both candidates (~50/50).
        if (!require(once_picked_two == kDraws,
                     "Expected single RouletteWheelSelectionFunction::update() to select winning option 2.0")) {
            return false;
        }
        if (!require(twice_picked_two >= 140 && twice_picked_two <= 260,
                     "Expected repeated RouletteWheelSelectionFunction::update() to clear scores_ and revert to uniform selection")) {
            return false;
        }
    }

    // Part 2: Under SingleControlFunction + LearningPeriodFunction + MultipleControlParameter,
    // repeated ControlMechanism::update() calls in the same update generation wipe computed scores.
    {
        auto cfg = load_json_config(R"({
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 10.0 },
            "ControlParameter": {
                "classname": "RealMultipleControlParameter",
                "number_of_objects": 2,
                "initial_value": 2.0
            },
            "ControlFunction": {
                "classname": "SingleControlFunction",
                "Function": {
                    "classname": "RealLearningPeriodFunction",
                    "learning_period": 2,
                    "object": {
                        "classname": "RealRouletteWheelSelectionFunction",
                        "scoring_function": { "classname": "SuccessScoringFunction" },
                        "score_size": 10,
                        "object": [1.0, 2.0]
                    }
                }
            },
            "ControlSelection": { "classname": "RealBetterOffspringControlSelection" },
            "ControlUpdate": [ { "classname": "GenerationControlUpdate" } ]
        })", "test_cm_rws_scf.json");

        auto proto = adef::make_and_setup_type<adef::RealControlMechanism>(cfg, pm);
        auto cm_once = proto->clone();
        auto cm_twice = proto->clone();

        auto prepare_and_record = [&](std::shared_ptr<adef::RealControlMechanism> cm,
                                      const std::string& tag) {
            auto repos = std::make_shared<TestRepository>();
            repos->set_random(std::make_shared<adef::Random>(42u));
            repos->set_population(make_population_with_fitness({10.0, 10.0}, pm, "rws_p_" + tag));
            repos->set_offspring(make_population_with_fitness({5.0, 5.0}, pm, "rws_o_" + tag));
            cm->init(repos);

            // Record winning scores for option 2.0 (preloaded in ControlParameter) via select()
            repos->parameters()->store("target_index", std::size_t{0});
            for (int trial = 0; trial < 4; ++trial) {
                cm->select(repos);
            }
            return repos;
        };

        auto repos_once = prepare_and_record(cm_once, "once");
        auto repos_twice = prepare_and_record(cm_twice, "twice");

        // Hit learning_period generation (gen = 2):
        // cm_once updates ONCE (target_index = 0) -> preserves learned score favoring 2.0.
        repos_once->parameters()->store("generation", 2u);
        repos_once->parameters()->store("target_index", std::size_t{0});
        cm_once->update(repos_once);

        // cm_twice updates TWICE in gen = 2 (target_index = 0, then target_index = 1) ->
        // SingleControlFunction triggers RouletteWheelSelectionFunction::update() twice, wiping scores to 0.
        repos_twice->parameters()->store("generation", 2u);
        repos_twice->parameters()->store("target_index", std::size_t{0});
        cm_twice->update(repos_twice);
        repos_twice->parameters()->store("target_index", std::size_t{1});
        cm_twice->update(repos_twice);

        // Switch to non-update generation (gen = 3) and observe generated choices.
        // When all scores are wiped to 0, RouletteWheelSelectionFunction::generate()
        // falls back to uniform selection across both candidates (~50/50).
        repos_once->parameters()->store("generation", 3u);
        repos_twice->parameters()->store("generation", 3u);
        int cm_once_twos = 0;
        int cm_twice_twos = 0;
        constexpr int kCmDraws = 400;
        for (int i = 0; i < kCmDraws; ++i) {
            repos_once->parameters()->store("target_index", std::size_t{0});
            cm_once->update(repos_once);
            if (approx_equal(cm_once->generate(repos_once), 2.0)) {
                ++cm_once_twos;
            }

            repos_twice->parameters()->store("target_index", std::size_t{0});
            cm_twice->update(repos_twice);
            if (approx_equal(cm_twice->generate(repos_twice), 2.0)) {
                ++cm_twice_twos;
            }
        }

        if (!require(cm_once_twos == kCmDraws &&
                         cm_twice_twos >= 140 && cm_twice_twos <= 260,
                     "Expected repeated SingleControlFunction updates in the same learning_period generation to wipe RouletteWheel scores and revert to uniform selection")) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// 3b. Behavioral Characterization: DEMutation + SdeFControlMechanism + SdeFControlUpdate
// ============================================================================
bool test_demutation_sde_f_behavior_and_order()
{
    constexpr std::size_t kPopSize = 6;
    constexpr std::size_t kDim = 4;

    auto make_mut_json = [](const std::string& cu_classname) {
        return std::string(R"({
            "classname": "DEMutation",
            "DEVariantX": { "classname": "DERand" },
            "DEVariantY": { "classname": "DEVariantY", "number_of_difference_vector": 1 },
            "F": {
                "classname": "SdeFControlMechanism",
                "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
                "ControlParameter": {
                    "classname": "RealMultipleControlParameter",
                    "number_of_objects": 6,
                    "initial_value": {
                        "classname": "RealNormalDisFunction",
                        "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                        "stddev": { "classname": "RealConstantFunction", "object": 0.15 }
                    }
                },
                "ControlFunction": {
                    "classname": "MultipleControlFunction",
                    "number_of_functions": 6,
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
                "ControlUpdate": [ { "classname": ")") + cu_classname + R"(" } ]
            }
        })";
    };

    struct PassResult {
        std::vector<double> f_values;
        std::vector<std::vector<double>> offspring_vars;
    };

    auto run_mutation_pass = [&](adef::Random::SeedType seed,
                                 const std::string& cu_classname,
                                 std::shared_ptr<SdeOrderTrace> trace,
                                 const std::string& tag) {
        adef::PrototypeManager pm;
        adef::register_type(pm);
        pm.register_type("CapturingStatistics", std::make_shared<CapturingStatistics>());
        if (trace) {
            pm.register_type("TracingSdeFControlUpdate",
                             std::make_shared<TracingSdeFControlUpdate>(trace));
        }

        auto prob_cfg = load_json_config(
            R"({"classname":"Sphere","dimension_of_decision_space":4})",
            "test_cm_demut_prob_" + tag + ".json");
        auto prob = adef::make_and_setup_type<adef::Problem>(prob_cfg, pm);

        auto pop_cfg = load_json_config(
            R"({"classname":"Population","population_size":6,"Individual":{"classname":"Individual"}})",
            "test_cm_demut_pop_" + tag + ".json");
        auto pop = adef::make_and_setup_type<adef::Population>(pop_cfg, pm);

        auto stats_cfg = load_json_config(
            R"({"classname":"CapturingStatistics","item":["f"]})",
            "test_cm_demut_stats_" + tag + ".json");
        auto stats = adef::make_and_setup_type<CapturingStatistics>(stats_cfg, pm);

        auto mut_cfg = load_json_config(make_mut_json(cu_classname),
                                        "test_cm_demut_mut_" + tag + ".json");
        auto mutation = adef::make_and_setup_type<adef::DEMutation>(mut_cfg, pm);

        auto repos = std::make_shared<TestRepository>();
        auto rng = std::make_shared<CountingRandom>(seed);
        repos->set_random(rng);
        repos->set_problem(prob);
        repos->set_population(pop);
        repos->set_statistics(stats);

        prob->init(repos);
        pop->init(repos);
        repos->set_offspring(pop->empty_clone());

        // Deterministically populate parent decision variables independent of RNG draw count
        for (std::size_t i = 0; i < kPopSize; ++i) {
            for (std::size_t d = 0; d < kDim; ++d) {
                pop->at(i)->variables(d) =
                    static_cast<double>(i + 1) * 1.5 - static_cast<double>(d) * 0.75;
            }
        }

        mutation->init(repos);
        if (trace) {
            trace->draws_after_init = rng->draw_count();
        }
        mutation->mutate(repos);

        PassResult res;
        res.f_values = stats->recorded(adef::Statistics::Item::f).at(0);
        res.offspring_vars.resize(kPopSize);
        for (std::size_t i = 0; i < kPopSize; ++i) {
            res.offspring_vars[i].resize(kDim);
            for (std::size_t d = 0; d < kDim; ++d) {
                res.offspring_vars[i][d] = repos->offspring()->at(i)->variables(d);
            }
        }
        return res;
    };

    // Part 1: Real DEMutation + SdeFControlMechanism + SdeFControlUpdate fixed-seed reproducibility
    const auto pass_a = run_mutation_pass(2026u, "SdeFControlUpdate", nullptr, "real_a");
    const auto pass_b = run_mutation_pass(2026u, "SdeFControlUpdate", nullptr, "real_b");
    const auto pass_c = run_mutation_pass(9999u, "SdeFControlUpdate", nullptr, "real_c");

    if (!require(pass_a.f_values.size() == kPopSize,
                 "Expected DEMutation pass to record F for all individuals")) {
        return false;
    }
    for (std::size_t i = 0; i < kPopSize; ++i) {
        const double f_val = pass_a.f_values[i];
        if (!require(std::isfinite(f_val) &&
                         adef::is_less_equal(0.0, f_val) &&
                         adef::is_less_equal(f_val, 1.0),
                     "Expected finite SDE F within [0, 1] at index " + std::to_string(i))) {
            return false;
        }
        for (std::size_t d = 0; d < kDim; ++d) {
            if (!require(std::isfinite(pass_a.offspring_vars[i][d]),
                         "Expected finite mutated offspring decision variable")) {
                return false;
            }
        }
    }
    if (!require(pass_a.f_values == pass_b.f_values &&
                     pass_a.offspring_vars == pass_b.offspring_vars,
                 "Expected identical F and offspring outputs for same-seed DEMutation + SDE pass")) {
        return false;
    }
    if (!require(pass_a.f_values != pass_c.f_values ||
                     pass_a.offspring_vars != pass_c.offspring_vars,
                 "Expected distinct outputs for different seeds in DEMutation + SDE pass")) {
        return false;
    }

    // Part 2: Verify exact per-individual interleaved RNG draws and in-place F overwrite order
    auto trace = std::make_shared<SdeOrderTrace>();
    const auto traced_pass = run_mutation_pass(2026u, "TracingSdeFControlUpdate", trace, "traced");

    // Traced pass uses the exact same RNG draws as the real pass -> outputs must match bit-for-bit
    if (!require(traced_pass.f_values == pass_a.f_values &&
                     traced_pass.offspring_vars == pass_a.offspring_vars,
                 "Expected TracingSdeFControlUpdate pass to preserve identical numerical outputs")) {
        return false;
    }
    if (!require(trace->target_indices.size() == kPopSize,
                 "Expected TracingSdeFControlUpdate to run once per individual")) {
        return false;
    }
    for (std::size_t i = 0; i < kPopSize; ++i) {
        if (!require(trace->target_indices[i] == i,
                     "Expected target_index execution order 0..pop_size-1")) {
            return false;
        }
        if (i == 0) {
            if (!require(trace->draws_before_update[0] > trace->draws_after_init,
                         "Expected DEMutation index sampling to consume RNG draws before individual 0's SdeFControlUpdate")) {
                return false;
            }
        }
        if (!require(trace->draws_after_update[i] > trace->draws_before_update[i],
                     "Expected SdeFControlUpdate to consume RNG draws during update()")) {
            return false;
        }
        if (i > 0) {
            // Between individual i-1's SdeFControlUpdate and individual i's SdeFControlUpdate,
            // both SdeFFunction::generate() for i-1 and DEMutation's index sampling for i consume RNG draws.
            if (!require(trace->draws_before_update[i] > trace->draws_after_update[i - 1],
                         "Expected interleaved RNG draws between consecutive individuals' SdeFControlUpdate calls")) {
                return false;
            }
            // Individual i >= 1 must already see individual 0's newly saved F in slot 0 (in-place overwrite)
            if (!require(approx_equal(trace->param0_observed_at_update[i], traced_pass.f_values[0]),
                         "Expected slot 0's newly generated F to be immediately visible in-place to subsequent individuals")) {
                return false;
            }
        }
    }

    return true;
}

} // namespace

int main()
{
    if (!test_control_mechanism_lifecycle_resampling_and_caching()) {
        return 1;
    }
    if (!test_granularity_matrix()) {
        return 1;
    }
    if (!test_sade_cr_control_mechanism_routing()) {
        return 1;
    }
    if (!test_sde_f_control_mechanism()) {
        return 1;
    }
    if (!test_indirect_control_mechanism_delegation_ordering()) {
        return 1;
    }
    if (!test_prototype_clone_isolation()) {
        return 1;
    }
    if (!test_roulette_wheel_repeated_update_characterization()) {
        return 1;
    }
    if (!test_demutation_sde_f_behavior_and_order()) {
        return 1;
    }

    std::cout << "All ControlMechanism lifecycle, isolation, and behavioral characterization tests passed."
              << std::endl;
    return 0;
}
