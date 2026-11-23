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

#include "Configuration.h"
#include "DEBinomial.h"
#include "DECrossover.h"
#include "DEMutation.h"
#include "DERand.h"
#include "DEVariantY.h"
#include "Individual.h"
#include "Parameters.h"
#include "Population.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Repository.h"
#include "cm/BaseControlMechanism.h"
#include "cm/ControlMechanism.h"
#include "cm/IndirectControlMechanism.h"
#include "cm/SdeFControlMechanism.h"
#include "cm/cf/MultipleControlFunction.h"
#include "cm/cf/SingleControlFunction.h"
#include "cm/cf/func/CauchyDisFunction.h"
#include "cm/cf/func/ConstantFunction.h"
#include "cm/cf/func/Function.h"
#include "cm/cf/func/LearningPeriodFunction.h"
#include "cm/cf/func/NormalDisFunction.h"
#include "cm/cf/func/RandomSelectionFunction.h"
#include "cm/cf/func/RouletteWheelSelectionFunction.h"
#include "cm/cf/func/SdeFFunction.h"
#include "cm/cf/func/UniformDisFunction.h"
#include "cm/cf/func/VariableFunction.h"
#include "cm/cf/func/sf/SuccessScoringFunction.h"
#include "cm/cp/MultipleControlParameter.h"
#include "cm/cp/SingleControlParameter.h"
#include "cm/cr/ControlRange.h"
#include "cm/cs/BetterOffspringControlSelection.h"
#include "cm/cs/NonInfoControlSelection.h"
#include "cm/cu/GenerationControlUpdate.h"
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

class TestRepository : public adef::Repository
{
public:
    TestRepository()
    {
        parameters_ = std::make_shared<adef::Parameters>();
    }

    void set_random(std::shared_ptr<adef::Random> random)
    {
        random_ = std::move(random);
    }

    void set_population(std::shared_ptr<adef::Population> population)
    {
        population_ = std::move(population);
    }
};

class SpySdeFunction : public adef::RealFunction
{
public:
    explicit SpySdeFunction(unsigned int num_params = 3) :
        num_params_(num_params)
    {
    }

    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    double generate() override
    {
        return 0.5;
    }

    bool record(const std::vector<std::any>& params,
                const std::string& /*name*/ = "") override
    {
        last_recorded_.clear();
        for (const auto& p : params) {
            last_recorded_.push_back(std::any_cast<double>(p));
        }
        return true;
    }

    bool record(const std::vector<std::any>& params,
                std::shared_ptr<const adef::Individual> /*parent*/,
                std::shared_ptr<const adef::Individual> /*offspring*/,
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

    const std::vector<double>& last_recorded() const
    {
        return last_recorded_;
    }

private:
    unsigned int num_params_;
    std::vector<double> last_recorded_;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<SpySdeFunction>(*this);
    }
};

void register_test_types(adef::PrototypeManager& pm)
{
    pm.register_type("Random", std::make_shared<adef::Random>());
    pm.register_type("Population", std::make_shared<adef::Population>());
    pm.register_type("Individual", std::make_shared<adef::Individual>());
    pm.register_type("DEMutation", std::make_shared<adef::DEMutation>());
    pm.register_type("DERand", std::make_shared<adef::DERand>());
    pm.register_type("DEVariantY", std::make_shared<adef::DEVariantY>());
    pm.register_type("DECrossover", std::make_shared<adef::DECrossover>());
    pm.register_type("DEBinomial", std::make_shared<adef::DEBinomial>());

    pm.register_type("RealControlMechanism",
                     std::make_shared<adef::RealControlMechanism>());
    pm.register_type("RealControlControlMechanism",
                     std::make_shared<adef::RealControlControlMechanism>());
    pm.register_type("SdeFControlMechanism",
                     std::make_shared<adef::SdeFControlMechanism>());
    pm.register_type("RealIndirectControlMechanism",
                     std::make_shared<adef::RealIndirectControlMechanism>());

    pm.register_type("RealControlRange",
                     std::make_shared<adef::RealControlRange>());
    pm.register_type("RealControlControlRange",
                     std::make_shared<adef::RealControlControlRange>());

    pm.register_type("RealSingleControlParameter",
                     std::make_shared<adef::RealSingleControlParameter>());
    pm.register_type("RealMultipleControlParameter",
                     std::make_shared<adef::RealMultipleControlParameter>());
    pm.register_type("RealControlMultipleControlParameter",
                     std::make_shared<adef::RealControlMultipleControlParameter>());

    pm.register_type("SingleControlFunction",
                     std::make_shared<adef::SingleControlFunction>());
    pm.register_type("MultipleControlFunction",
                     std::make_shared<adef::MultipleControlFunction>());

    pm.register_type("NonInfoControlSelection",
                     std::make_shared<adef::NonInfoControlSelection>());
    pm.register_type("RealControlBetterOffspringControlSelection",
                     std::make_shared<adef::RealControlBetterOffspringControlSelection>());

    pm.register_type("GenerationControlUpdate",
                     std::make_shared<adef::GenerationControlUpdate>());
    pm.register_type("SdeFControlUpdate",
                     std::make_shared<adef::SdeFControlUpdate>());

    pm.register_type("RealConstantFunction",
                     std::make_shared<adef::RealConstantFunction>());
    pm.register_type("RealControlConstantFunction",
                     std::make_shared<adef::RealControlConstantFunction>());
    pm.register_type("IntegerVariableFunction",
                     std::make_shared<adef::IntegerVariableFunction>());
    pm.register_type("RealControlVariableFunction",
                     std::make_shared<adef::RealControlVariableFunction>());
    pm.register_type("RealNormalDisFunction",
                     std::make_shared<adef::RealNormalDisFunction>());
    pm.register_type("RealCauchyDisFunction",
                     std::make_shared<adef::RealCauchyDisFunction>());
    pm.register_type("RealUniformDisFunction",
                     std::make_shared<adef::RealUniformDisFunction>());
    pm.register_type("RealRandomSelectionFunction",
                     std::make_shared<adef::RealRandomSelectionFunction>());
    pm.register_type("RealControlRandomSelectionFunction",
                     std::make_shared<adef::RealControlRandomSelectionFunction>());
    pm.register_type("RealControlRouletteWheelSelectionFunction",
                     std::make_shared<adef::RealControlRouletteWheelSelectionFunction>());
    pm.register_type("RealControlLearningPeriodFunction",
                     std::make_shared<adef::RealControlLearningPeriodFunction>());
    pm.register_type("SdeFFunction",
                     std::make_shared<adef::SdeFFunction>());
    pm.register_type("SuccessScoringFunction",
                     std::make_shared<adef::SuccessScoringFunction>());
}

std::shared_ptr<adef::Random> make_random(unsigned int seed,
                                          const adef::PrototypeManager& pm,
                                          const std::string& tag)
{
    auto cfg = load_json_config(
        "{\"classname\":\"Random\",\"seed\":" + std::to_string(seed) + "}",
        "test_issue20_rng_" + tag + ".json");
    return adef::make_and_setup_type<adef::Random>(cfg, pm);
}

std::shared_ptr<adef::Population> make_population(unsigned int pop_size,
                                                  const adef::PrototypeManager& pm,
                                                  const std::string& tag)
{
    auto cfg = load_json_config(
        "{\"classname\":\"Population\",\"population_size\":" +
            std::to_string(pop_size) +
            ",\"Individual\":{\"classname\":\"Individual\"}}",
        "test_issue20_pop_" + tag + ".json");
    return adef::make_and_setup_type<adef::Population>(cfg, pm);
}

// 1. Verify init() propagation through DEMutation, DECrossover,
//    IndirectControlMechanism, BaseFunction children, and ControlledObject containers.
bool test_init_propagation(const adef::PrototypeManager& pm)
{
    auto repos = std::make_shared<TestRepository>();
    repos->set_random(make_random(42, pm, "init"));
    repos->set_population(make_population(4, pm, "init"));
    repos->parameters()->store("generation", 1u);
    repos->parameters()->store("target_index", std::size_t{0});

    // DEMutation with IndirectControlMechanism -> LearningPeriod -> RouletteWheel -> [Normal, Cauchy]
    auto mut_cfg = load_json_config(R"({
        "classname": "DEMutation",
        "DEVariantX": { "classname": "DERand" },
        "DEVariantY": { "classname": "DEVariantY", "number_of_difference_vector": 1 },
        "F": {
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
                        "learning_period": 10,
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
        }
    })", "test_issue20_mut.json");

    auto mutation = adef::make_and_setup_type<adef::DEMutation>(mut_cfg, pm);
    mutation->init(repos);

    auto f_base = repos->parameters()->take_out<std::shared_ptr<adef::BaseControlMechanism>>("f");
    auto f_cm = std::dynamic_pointer_cast<adef::ControlMechanism<double>>(f_base);
    if (!require(f_cm != nullptr, "Expected F ControlMechanism in Parameters")) {
        return false;
    }
    f_cm->update(repos);
    const double f_val = f_cm->generate(repos);
    if (!require(std::isfinite(f_val) && f_val >= 0.0 && f_val <= 1.0,
                 "Expected finite F in [0, 1] after IndirectControlMechanism init")) {
        return false;
    }

    // DECrossover with RealUniformDisFunction
    auto cr_cfg = load_json_config(R"({
        "classname": "DECrossover",
        "DEVariantZ": { "classname": "DEBinomial" },
        "CR": {
            "classname": "RealControlMechanism",
            "ControlRange": { "classname": "RealControlRange", "lower_bound": 0.0, "upper_bound": 1.0 },
            "ControlParameter": { "classname": "RealSingleControlParameter" },
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
        }
    })", "test_issue20_cr.json");

    auto crossover = adef::make_and_setup_type<adef::DECrossover>(cr_cfg, pm);
    crossover->init(repos);

    auto cr_base = repos->parameters()->take_out<std::shared_ptr<adef::BaseControlMechanism>>("cr");
    auto cr_cm = std::dynamic_pointer_cast<adef::ControlMechanism<double>>(cr_base);
    if (!require(cr_cm != nullptr, "Expected CR ControlMechanism in Parameters")) {
        return false;
    }
    cr_cm->update(repos);
    const double cr_val = cr_cm->generate(repos);
    if (!require(std::isfinite(cr_val) && cr_val >= 0.0 && cr_val <= 1.0,
                 "Expected finite CR in [0, 1] after DECrossover init")) {
        return false;
    }

    // ControlledObject containers: ConstantFunction, VariableFunction, RandomSelectionFunction
    auto const_ctrl_cfg = load_json_config(R"({
        "classname": "RealControlConstantFunction",
        "object": {
            "classname": "RealNormalDisFunction",
            "mean": { "classname": "RealConstantFunction", "object": 0.5 },
            "stddev": { "classname": "RealConstantFunction", "object": 0.1 }
        }
    })", "test_issue20_const_ctrl.json");
    auto const_ctrl = adef::make_and_setup_type<adef::RealControlConstantFunction>(const_ctrl_cfg, pm);
    const_ctrl->init(repos);
    auto inner_from_const = const_ctrl->generate();
    inner_from_const->update();
    if (!require(std::isfinite(inner_from_const->generate()),
                 "Expected RealControlConstantFunction child to receive Random via init")) {
        return false;
    }

    auto var_ctrl_cfg = load_json_config(R"({
        "classname": "RealControlVariableFunction",
        "object": {
            "classname": "RealCauchyDisFunction",
            "location": { "classname": "RealConstantFunction", "object": 0.5 },
            "scale": { "classname": "RealConstantFunction", "object": 0.1 }
        }
    })", "test_issue20_var_ctrl.json");
    auto var_ctrl = adef::make_and_setup_type<adef::RealControlVariableFunction>(var_ctrl_cfg, pm);
    var_ctrl->init(repos);
    auto inner_from_var = var_ctrl->generate();
    inner_from_var->update();
    if (!require(std::isfinite(inner_from_var->generate()),
                 "Expected RealControlVariableFunction child to receive Random via init")) {
        return false;
    }

    auto rnd_ctrl_cfg = load_json_config(R"({
        "classname": "RealControlRandomSelectionFunction",
        "object": [
            {
                "classname": "RealNormalDisFunction",
                "mean": { "classname": "RealConstantFunction", "object": 0.5 },
                "stddev": { "classname": "RealConstantFunction", "object": 0.1 }
            }
        ]
    })", "test_issue20_rnd_ctrl.json");
    auto rnd_ctrl = adef::make_and_setup_type<adef::RealControlRandomSelectionFunction>(rnd_ctrl_cfg, pm);
    rnd_ctrl->init(repos);
    auto inner_from_rnd = rnd_ctrl->generate();
    inner_from_rnd->update();
    if (!require(std::isfinite(inner_from_rnd->generate()),
                 "Expected RealControlRandomSelectionFunction child to receive Random via init")) {
        return false;
    }

    // SDE F mechanism with Function-typed MultipleControlParameter initial_value and SdeFFunction child "rand"
    auto sde_cfg = load_json_config(R"({
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
    })", "test_issue20_sde_cm.json");

    auto sde_cm = adef::make_and_setup_type<adef::SdeFControlMechanism>(sde_cfg, pm);
    sde_cm->init(repos);
    sde_cm->update(repos);
    const double sde_val = sde_cm->generate(repos);
    if (!require(std::isfinite(sde_val) && sde_val >= 0.0 && sde_val <= 1.0,
                 "Expected finite SDE F in [0, 1] after deferred initial_value and recursive init")) {
        return false;
    }

    return true;
}

// 2. Verify SingleControlParameter & MultipleControlParameter deferred Function initial_value
//    and clone() determinism.
bool test_deferred_parameter_initial_value(const adef::PrototypeManager& pm)
{
    auto single_cfg = load_json_config(R"({
        "classname": "RealSingleControlParameter",
        "initial_value": {
            "classname": "RealNormalDisFunction",
            "mean": { "classname": "RealConstantFunction", "object": 0.5 },
            "stddev": { "classname": "RealConstantFunction", "object": 0.15 }
        }
    })", "test_issue20_single_cp.json");

    auto single_proto = adef::make_and_setup_type<adef::RealSingleControlParameter>(single_cfg, pm);
    auto single_a = single_proto->clone();
    auto single_b = single_proto->clone();

    auto repos_a = std::make_shared<TestRepository>();
    repos_a->set_random(make_random(123, pm, "cp_a"));
    single_a->init(repos_a);

    auto repos_b = std::make_shared<TestRepository>();
    repos_b->set_random(make_random(123, pm, "cp_b"));
    single_b->init(repos_b);

    if (!require(std::isfinite(single_a->load(0)) && single_a->load(0) == single_b->load(0),
                 "Expected cloned RealSingleControlParameter with Function initial_value to initialize deterministically")) {
        return false;
    }

    auto multi_cfg = load_json_config(R"({
        "classname": "RealMultipleControlParameter",
        "number_of_objects": 5,
        "initial_value": {
            "classname": "RealNormalDisFunction",
            "mean": { "classname": "RealConstantFunction", "object": 0.5 },
            "stddev": { "classname": "RealConstantFunction", "object": 0.15 }
        }
    })", "test_issue20_multi_cp.json");

    auto multi_proto = adef::make_and_setup_type<adef::RealMultipleControlParameter>(multi_cfg, pm);
    auto multi_a = multi_proto->clone();
    auto multi_b = multi_proto->clone();

    auto repos_ma = std::make_shared<TestRepository>();
    repos_ma->set_random(make_random(321, pm, "mcp_a"));
    multi_a->init(repos_ma);

    auto repos_mb = std::make_shared<TestRepository>();
    repos_mb->set_random(make_random(321, pm, "mcp_b"));
    multi_b->init(repos_mb);

    for (std::size_t idx = 0; idx < 5; ++idx) {
        if (!require(std::isfinite(multi_a->load(idx)) &&
                         multi_a->load(idx) == multi_b->load(idx),
                     "Expected cloned RealMultipleControlParameter with Function initial_value to match across same-seed runs")) {
            return false;
        }
    }

    return true;
}

// 3. Verify SdeFControlUpdate produces strictly mutually exclusive indices and guards invalid ranges.
bool test_sdef_control_update_exclusivity(const adef::PrototypeManager& pm)
{
    auto repos = std::make_shared<TestRepository>();
    repos->set_population(make_population(4, pm, "sde_excl"));

    auto cp_cfg = load_json_config(R"({
        "classname": "RealMultipleControlParameter",
        "number_of_objects": 4
    })", "test_issue20_sde_cp.json");
    auto param = adef::make_and_setup_type<adef::RealMultipleControlParameter>(cp_cfg, pm);
    const std::vector<double> distinct_values{10.0, 20.0, 30.0, 40.0};
    for (std::size_t idx = 0; idx < distinct_values.size(); ++idx) {
        param->save(distinct_values[idx], idx);
    }

    auto spy = std::make_shared<SpySdeFunction>(3u);
    adef::SdeFControlUpdate updater;

    repos->set_random(make_random(7, pm, "sde_excl"));
    for (int trial = 0; trial < 100; ++trial) {
        updater.update(repos, param, spy);
        const auto& recorded = spy->last_recorded();
        if (!require(recorded.size() == 3,
                     "Expected SdeFControlUpdate to record 3 values")) {
            return false;
        }
        std::set<double> unique_vals(recorded.begin(), recorded.end());
        if (!require(unique_vals.size() == 3,
                     "Expected SdeFControlUpdate sampled indices to be mutually exclusive on trial " +
                         std::to_string(trial))) {
            return false;
        }
    }

    // Requesting more unique indices than population size should throw std::invalid_argument.
    auto spy_too_many = std::make_shared<SpySdeFunction>(5u);
    bool threw_too_many = false;
    try {
        updater.update(repos, param, spy_too_many);
    }
    catch (const std::invalid_argument&) {
        threw_too_many = true;
    }
    if (!require(threw_too_many,
                 "Expected std::invalid_argument when number_of_parameters exceeds population size")) {
        return false;
    }

    // Empty population with positive number_of_parameters should throw std::invalid_argument.
    auto empty_repos = std::make_shared<TestRepository>();
    empty_repos->set_random(make_random(7, pm, "sde_empty"));
    empty_repos->set_population(make_population(0, pm, "sde_empty"));
    bool threw_empty = false;
    try {
        updater.update(empty_repos, param, spy);
    }
    catch (const std::invalid_argument&) {
        threw_empty = true;
    }
    if (!require(threw_empty,
                 "Expected std::invalid_argument on empty population in SdeFControlUpdate")) {
        return false;
    }

    return true;
}

// 4. Verify RandomSelectionFunction does not share a static distribution across different sizes.
bool test_random_selection_function_independent_bounds(const adef::PrototypeManager& pm)
{
    auto repos = std::make_shared<TestRepository>();
    repos->set_random(make_random(99, pm, "rsf"));

    auto cfg2 = load_json_config(R"({
        "classname": "RealRandomSelectionFunction",
        "object": [1.0, 2.0]
    })", "test_issue20_rsf2.json");
    auto rsf2 = adef::make_and_setup_type<adef::RealRandomSelectionFunction>(cfg2, pm);
    rsf2->init(repos);
    for (int i = 0; i < 10; ++i) {
        const double val = rsf2->generate();
        if (!require(val == 1.0 || val == 2.0,
                     "Expected RealRandomSelectionFunction(2) to return 1.0 or 2.0")) {
            return false;
        }
    }

    auto cfg5 = load_json_config(R"({
        "classname": "RealRandomSelectionFunction",
        "object": [10.0, 20.0, 30.0, 40.0, 50.0]
    })", "test_issue20_rsf5.json");
    auto rsf5 = adef::make_and_setup_type<adef::RealRandomSelectionFunction>(cfg5, pm);
    rsf5->init(repos);
    bool saw_beyond_first_two = false;
    for (int i = 0; i < 200; ++i) {
        const double val = rsf5->generate();
        if (val >= 30.0) {
            saw_beyond_first_two = true;
            break;
        }
    }
    if (!require(saw_beyond_first_two,
                 "Expected RealRandomSelectionFunction(5) to sample indices >= 2 (no static distribution reuse)")) {
        return false;
    }

    auto cfg1 = load_json_config(R"({
        "classname": "RealRandomSelectionFunction",
        "object": [99.0]
    })", "test_issue20_rsf1.json");
    auto rsf1 = adef::make_and_setup_type<adef::RealRandomSelectionFunction>(cfg1, pm);
    rsf1->init(repos);
    for (int i = 0; i < 20; ++i) {
        if (!require(rsf1->generate() == 99.0,
                     "Expected RealRandomSelectionFunction(1) to always return 99.0 without out_of_range")) {
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    adef::PrototypeManager pm;
    register_test_types(pm);

    if (!test_init_propagation(pm)) {
        return 1;
    }
    if (!test_deferred_parameter_initial_value(pm)) {
        return 1;
    }
    if (!test_sdef_control_update_exclusivity(pm)) {
        return 1;
    }
    if (!test_random_selection_function_independent_bounds(pm)) {
        return 1;
    }

    std::cout << "All ControlMechanism/Function init & SdeFControlUpdate tests passed."
              << std::endl;
    return 0;
}
