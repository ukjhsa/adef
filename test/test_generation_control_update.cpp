#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include "Configuration.h"
#include "Parameters.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "cm/cf/func/Function.h"
#include "cm/cf/func/LearningPeriodFunction.h"
#include "cm/cf/func/VariableFunction.h"
#include "cm/cu/GenerationControlUpdate.h"

namespace {

class CountingRealFunction : public adef::RealFunction
{
public:
    void setup(const adef::Configuration&, const adef::PrototypeManager&) override
    {
    }

    double generate() override
    {
        return static_cast<double>(update_count_);
    }

    void update() override
    {
        ++update_count_;
    }

    unsigned int number_of_parameters() const override
    {
        return 0;
    }

private:
    int update_count_ = 0;

    std::shared_ptr<adef::Prototype> clone_impl() const override
    {
        return std::make_shared<CountingRealFunction>(*this);
    }
};

class TestRepository : public adef::Repository
{
public:
    TestRepository()
    {
        parameters_ = std::make_shared<adef::Parameters>();
    }
};

bool require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        return false;
    }
    return true;
}

std::shared_ptr<adef::RealLearningPeriodFunction> make_learning_period_function(
    unsigned int learning_period,
    const std::string& config_filename,
    const adef::PrototypeManager& pm)
{
    {
        std::ofstream ofs(config_filename);
        ofs << "{\n"
            << "  \"object\": { \"classname\": \"CountingRealFunction\" },\n"
            << "  \"learning_period\": " << learning_period << "\n"
            << "}\n";
    }

    adef::Configuration config;
    const bool loaded = config.load_config(config_filename);
    std::remove(config_filename.c_str());
    if (!loaded) {
        throw std::runtime_error("Failed to load test configuration: " + config_filename);
    }

    auto lpf = std::make_shared<adef::RealLearningPeriodFunction>();
    lpf->setup(config, pm);
    return lpf;
}

} // namespace

int main()
{
    adef::PrototypeManager pm;
    pm.register_type("IntegerVariableFunction", std::make_shared<adef::IntegerVariableFunction>());
    pm.register_type("CountingRealFunction", std::make_shared<CountingRealFunction>());

    auto repos = std::make_shared<TestRepository>();
    adef::GenerationControlUpdate updater;

    // 1. Normal unsigned int generation recorded into LearningPeriodFunction (period = 3)
    auto lpf = make_learning_period_function(3, "test_lpf_period3.json", pm);
    for (unsigned int gen = 1; gen <= 6; ++gen) {
        repos->parameters()->store("generation", gen);
        updater.update(repos, nullptr, lpf);
        lpf->update();

        const double expected_updates = static_cast<double>(gen / 3);
        if (!require(lpf->generate() == expected_updates,
                     "Expected update count " + std::to_string(expected_updates) +
                     " at generation " + std::to_string(gen) +
                     ", got " + std::to_string(lpf->generate()))) {
            return 1;
        }
    }

    // 2. Overflow protection when unsigned int exceeds INT_MAX
    bool threw_overflow = false;
    try {
        const unsigned int huge_gen =
            static_cast<unsigned int>(std::numeric_limits<int>::max()) + 1u;
        repos->parameters()->store("generation", huge_gen);
        updater.update(repos, nullptr, lpf);
    }
    catch (const std::overflow_error&) {
        threw_overflow = true;
    }
    if (!require(threw_overflow, "Expected std::overflow_error for generation > INT_MAX")) {
        return 1;
    }

    // 3. learning_period == 0 should safely skip updates without dividing by zero
    auto lpf_zero = make_learning_period_function(0, "test_lpf_period0.json", pm);
    repos->parameters()->store("generation", 1u);
    updater.update(repos, nullptr, lpf_zero);
    lpf_zero->update();
    if (!require(lpf_zero->generate() == 0.0,
                 "Expected 0 updates when learning_period == 0")) {
        return 1;
    }

    // 4. Negative generation should not trigger LearningPeriodFunction update
    auto lpf_neg = make_learning_period_function(3, "test_lpf_neg.json", pm);
    lpf_neg->record({ -3 }, "generation");
    lpf_neg->update();
    if (!require(lpf_neg->generate() == 0.0,
                 "Expected 0 updates when generation < 0")) {
        return 1;
    }

    std::cout << "All GenerationControlUpdate & LearningPeriodFunction tests passed." << std::endl;
    return 0;
}
