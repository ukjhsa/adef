#include <memory>
#include <string>
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
#include "Reproduction.h"
#include "EnvironmentalSelection.h"
#include "Mutation.h"
#include "Crossover.h"
#include "Repair.h"
#include "Random.h"

namespace adef {

Repository::Repository() : algorithm_name_("algorithm")
{
}

Repository::Repository(const Repository& rhs) : Prototype(rhs),
    enable_shared_from_this(rhs), algorithm_name_(rhs.algorithm_name_)
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

}
