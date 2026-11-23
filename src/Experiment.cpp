#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include "Experiment.h"
#include "Random.h"
#include "ExperimentalStatistics.h"
#include "Repository.h"
#include "Problem.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Evolution.h"

namespace adef {

Experiment::Experiment() : name_("experiment"), number_of_runs_(0)
{
}

Experiment::Experiment(const Experiment& rhs) :
    Prototype(rhs),
    name_(rhs.name_),
    number_of_runs_(rhs.number_of_runs_),
    run_seeds_(rhs.run_seeds_)
{
    if (rhs.random_) { random_ = rhs.random_->clone(); }
    if (rhs.statistics_) { statistics_ = rhs.statistics_->clone(); }
    if (rhs.repository_) { repository_ = rhs.repository_->clone(); }
}

void Experiment::setup(const Configuration& config, const PrototypeManager& pm)
{
    number_of_runs_ = config.get_uint_value("number_of_runs");

    auto random_config = config.get_config("Random");
    if (random_config.is_null()) {
        try {
            random_ = pm.make_type<Random>("Random");
        }
        catch (const std::logic_error&) {
            random_ = std::make_shared<Random>();
        }
        if (!random_) {
            random_ = std::make_shared<Random>();
        }
        random_->auto_seed();
    }
    else {
        random_ = make_and_setup_type<Random>(random_config, pm);
    }

    statistics_ = make_and_setup_type<ExperimentalStatistics>(
                                "ExperimentalStatistics", config, pm);
    repository_ = make_and_setup_type<Repository>("Repository", config, pm);

    name_ = repository_->algorithm_name() + "_" +
            repository_->problem()->name();
}

void Experiment::run()
{
    if (!random_) {
        throw std::logic_error("Experiment random generator is not initialized.");
    }

    run_seeds_.clear();
    run_seeds_.reserve(number_of_runs_);

    for (decltype(number_of_runs_) times = 0; times < number_of_runs_; ++times) {
        // clone a Repository for each run
        auto repos = repository_->clone();
        // derive an independent, deterministic per-run Random instance
        auto run_random = random_->derive(times);
        run_seeds_.push_back(run_random->seed());
        // initialize all evolutionary states
        repos->init(run_random);

        // evolve the evolution
        repos->evolution()->evolve(repos);

        std::stringstream ss;
        ss << name_ << "_run" << times + 1;
        statistics_->output_statistics_on_run(repos, ss.str());

        statistics_->take_statistics(repos);
    }
    statistics_->output_statistics(name_ + ".csv");
}

} // namespace adef
