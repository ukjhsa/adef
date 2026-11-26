#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include "Repository.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Evolution.h"
#include "Problem.h"
#include "Statistics.h"
#include "Parameters.h"
#include "Initializer.h"
#include "Evaluator.h"
#include "Population.h"
#include "Individual.h"
#include "Reproduction.h"
#include "EnvironmentalSelection.h"
#include "Mutation.h"
#include "Crossover.h"
#include "Repair.h"
#include "Random.h"

namespace adef {

Repository::ReadOnlyPopulationScope::ReadOnlyPopulationScope(Repository* repos) noexcept :
    owner_(repos != nullptr ? repos->weak_from_this().lock() : nullptr),
    repos_(repos)
{
    if (repos_ != nullptr) {
        ++repos_->read_only_population_scope_depth_;
    }
}

Repository::ReadOnlyPopulationScope::ReadOnlyPopulationScope(
    const std::shared_ptr<Repository>& repos) noexcept :
    owner_(repos),
    repos_(repos.get())
{
    if (repos_ != nullptr) {
        ++repos_->read_only_population_scope_depth_;
    }
}

Repository::ReadOnlyPopulationScope::ReadOnlyPopulationScope(
    ReadOnlyPopulationScope&& other) noexcept :
    owner_(std::move(other.owner_)),
    repos_(other.repos_)
{
    other.repos_ = nullptr;
}

Repository::ReadOnlyPopulationScope::~ReadOnlyPopulationScope() noexcept
{
    if (repos_ != nullptr) {
        repos_->exit_read_only_population_scope();
    }
}

Repository::Repository() :
    algorithm_name_("algorithm"),
    read_only_population_scope_depth_(0),
    population_fitness_summary_(std::nullopt)
{
}

Repository::Repository(const Repository& rhs) : Prototype(rhs),
    enable_shared_from_this(rhs), algorithm_name_(rhs.algorithm_name_),
    read_only_population_scope_depth_(0),
    population_fitness_summary_(std::nullopt)
{
    if (rhs.random_) { random_ = rhs.random_->clone(); }
    if (rhs.evolution_) { evolution_ = rhs.evolution_->clone(); }
    if (rhs.problem_) { problem_ = rhs.problem_->clone(); }
    if (rhs.statistics_) { statistics_ = rhs.statistics_->clone(); }
    if (rhs.parameters_) { parameters_ = rhs.parameters_->clone(); }

    if (rhs.initializer_) { initializer_ = rhs.initializer_->clone(); }
    if (rhs.evaluator_) { evaluator_ = rhs.evaluator_->clone(); }

    if (rhs.population_) { population_ = rhs.population_->clone(); }
    if (rhs.offspring_) { offspring_ = rhs.offspring_->clone(); }

    if (rhs.reproduction_) { reproduction_ = rhs.reproduction_->clone(); }
    if (rhs.environmental_selection_) {
            environmental_selection_ = rhs.environmental_selection_->clone();
    }
    if (rhs.mutation_) { mutation_ = rhs.mutation_->clone(); }
    if (rhs.crossover_) { crossover_ = rhs.crossover_->clone(); }
    if (rhs.repair_) { repair_ = rhs.repair_->clone(); }
}

void Repository::setup(const Configuration& config, const PrototypeManager& pm)
{
    algorithm_name_ = config.get_string_value("algorithm_name");

    evolution_ = make_and_setup_type<Evolution>("Evolution", config, pm);
    problem_ = make_and_setup_type<Problem>("Problem", config, pm);
    statistics_ = make_and_setup_type<Statistics>("Statistics", config, pm);
    parameters_ = make_and_setup_type<Parameters>("Parameters", config, pm);

    initializer_ = make_and_setup_type<Initializer>("Initializer", config, pm);
    evaluator_ = make_and_setup_type<Evaluator>("Evaluator", config, pm);

    population_ = make_and_setup_type<Population>("Population", config, pm);
    // offspring is created through population in init()

    reproduction_ = make_and_setup_type<Reproduction>(
                        "Reproduction", config, pm);
    environmental_selection_ = make_and_setup_type<EnvironmentalSelection>(
                                    "EnvironmentalSelection", config, pm);
    mutation_ = make_and_setup_type<Mutation>("Mutation", config, pm);
    crossover_ = make_and_setup_type<Crossover>("Crossover", config, pm);
    repair_ = make_and_setup_type<Repair>("Repair", config, pm);
}

void Repository::init(std::shared_ptr<Random> random)
{
    population_fitness_summary_.reset();
    random_ = random;

    evolution_->init(shared_from_this());
    problem_->init(shared_from_this());
    statistics_->init(shared_from_this());
    parameters_->init(shared_from_this());

    initializer_->init(shared_from_this());
    evaluator_->init(shared_from_this());

    population_->init(shared_from_this());
    offspring_ = population_->empty_clone();

    reproduction_->init(shared_from_this());
    environmental_selection_->init(shared_from_this());
    mutation_->init(shared_from_this());
    crossover_->init(shared_from_this());
    repair_->init(shared_from_this());
}

Repository::ReadOnlyPopulationScope Repository::enter_read_only_population_scope() noexcept
{
    return ReadOnlyPopulationScope(this);
}

void Repository::exit_read_only_population_scope() noexcept
{
    if (read_only_population_scope_depth_ > 0) {
        --read_only_population_scope_depth_;
        if (read_only_population_scope_depth_ == 0) {
            population_fitness_summary_.reset();
        }
    }
}

PopulationFitnessSummary Repository::population_fitness_summary() const
{
    if (read_only_population_scope_depth_ > 0) {
        if (!population_fitness_summary_.has_value()) {
            population_fitness_summary_ = compute_population_fitness_summary();
        }
        return *population_fitness_summary_;
    }
    return compute_population_fitness_summary();
}

PopulationFitnessSummary Repository::compute_population_fitness_summary() const
{
    if (!population_) {
        throw std::logic_error("Repository population is null");
    }

    PopulationFitnessSummary summary;
    const auto pop_size = population_->population_size();
    summary.count = pop_size;
    if (pop_size == 0) {
        return summary;
    }

    const double first = population_->at(0)->fitness();
    double sum = 0.0;
    sum += first;
    double min_val = first;
    double max_val = first;

    for (std::size_t idx = 1; idx < pop_size; ++idx) {
        const double current = population_->at(idx)->fitness();
        sum += current;
        if (min_val > current) {
            min_val = current;
        }
        if (max_val < current) {
            max_val = current;
        }
    }

    summary.min = min_val;
    summary.max = max_val;
    summary.average = sum / static_cast<double>(pop_size);
    return summary;
}

}
