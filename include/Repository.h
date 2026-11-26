#ifndef EVOLUTIONSTATE_H
#define EVOLUTIONSTATE_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include "Prototype.h"

namespace adef {

/**
@brief Summary statistics of population fitness values.
*/
struct PopulationFitnessSummary
{
    std::size_t count = 0;
    double min = 0.0;
    double max = 0.0;
    double average = 0.0;
};

class Configuration;
class PrototypeManager;
class Evolution;
class Problem;
class Statistics;
class Parameters;
class Initializer;
class Evaluator;
class Population;
class Reproduction;
class EnvironmentalSelection;
class Mutation;
class Crossover;
class Repair;
class Random;

/**
@brief Repository contains informations of the algorithm.

The algorithm includes what evolution process is,
what problem evaluates, what mutation and crossover use,
and how environmental selection operate, etc.

Repository includes all evolutionary states used in the algorithm
and some helper feature such as statistics and parameter storage.

@par The configuration
Repository has extra configurations:
- member
    - name: "algorithm_name"
    - value: @c string
- member
    - name: "Evolution"
    - value: object configurations which represents Evolution.
- member
    - name: "Problem"
    - value: object configurations which represents Problem.
- member
    - name: "Statistics"
    - value: object configurations which represents Statistics.
- member
    - name: "Parameters"
    - value: object configurations which represents Parameters.
- member
    - name: "Initializer"
    - value: object configurations which represents Initializer.
- member
    - name: "Evaluator"
    - value: object configurations which represents Evaluator.
- member
    - name: "Population"
    - value: object configurations which represents Population.
- member
    - name: "Reproduction"
    - value: object configurations which represents Reproduction.
- member
    - name: "EnvironmentalSelection"
    - value: object configurations which represents EnvironmentalSelection.
- member
    - name: "Mutation"
    - value: object configurations which represents Mutation.
- member
    - name: "Crossover"
    - value: object configurations which represents Crossover.
- member
    - name: "Repair"
    - value: object configurations which represents Repair.
.
See setup() for the details.

@sa See System and Experiment for which contains.
*/
class Repository :
    public Prototype, public std::enable_shared_from_this<Repository>
{
public:

/**
@brief RAII guard bounding a read-only population observation scope.

While at least one scope is active on a Repository, population fitness
summary queries compute a single-pass summary lazily and reuse it until
the outermost scope exits.
*/
    class ReadOnlyPopulationScope
    {
    public:
        explicit ReadOnlyPopulationScope(Repository* repos) noexcept;
        explicit ReadOnlyPopulationScope(const std::shared_ptr<Repository>& repos) noexcept;
        ~ReadOnlyPopulationScope() noexcept;

        ReadOnlyPopulationScope(const ReadOnlyPopulationScope&) = delete;
        ReadOnlyPopulationScope& operator=(const ReadOnlyPopulationScope&) = delete;
        ReadOnlyPopulationScope(ReadOnlyPopulationScope&& other) noexcept;
        ReadOnlyPopulationScope& operator=(ReadOnlyPopulationScope&& other) = delete;

    private:
        std::shared_ptr<Repository> owner_;
        Repository* repos_;
    };

    Repository();
    Repository(const Repository& rhs);
    virtual ~Repository() = default;
    Repository& operator=(const Repository& rhs) = delete;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<Repository> clone() const
    {
        return std::make_shared<Repository>(*this);
    }
/**
@brief Set up the internal states.

If Repository has the following configuration:
- algorithm_name: Algorithm
.
its configuration should be
- JSON configuration
@code
"Repository" : {
    "algorithm_name" : "Algorithm",
    "Evolution" : {
        ...skip...
    },
    "Problem": {
        ...skip...
    },
    "Statistics" : {
        ...skip...
    },
    "Parameters" : {
        ...skip...
    },
    "Initializer" : {
        ...skip...
    },
    "Evaluator" : {
        ...skip...
    },
    "Population" : {
        ...skip...
    },
    "Reproduction" : {
        ...skip...
    },
    "EnvironmentalSelection" : {
        ...skip...
    },
    "Mutation" : {
        ...skip...
    },
    "Crossover" : {
        ...skip...
    },
    "Repair" : {
        ...skip...
    }
}
@endcode
.
*/
    virtual void setup(const Configuration& config,
                       const PrototypeManager& pm) override;
/**
@brief Initialize the internal states.
*/
    virtual void init(std::shared_ptr<Random> random);


/// Return the name of the algorithm.
    const std::string& algorithm_name() const { return algorithm_name_; }

    std::shared_ptr<Random> random() { return random_; }
    std::shared_ptr<const Random> random() const { return random_; }

/**
@name evolutionary states access
*/
///@{
    std::shared_ptr<Evolution> evolution() { return evolution_; }
    std::shared_ptr<const Evolution> evolution() const { return evolution_; }

    std::shared_ptr<Problem> problem() { return problem_; }
    std::shared_ptr<const Problem> problem() const { return problem_; }
    std::shared_ptr<Statistics> statistics() { return statistics_; }
    std::shared_ptr<const Statistics> statistics() const { return statistics_; }
    std::shared_ptr<Parameters> parameters() { return parameters_; }
    std::shared_ptr<const Parameters> parameters() const { return parameters_; }

    std::shared_ptr<Initializer> initializer() { return initializer_; }
    std::shared_ptr<const Initializer> initializer() const {
        return initializer_; }
    std::shared_ptr<Evaluator> evaluator() { return evaluator_; }
    std::shared_ptr<const Evaluator> evaluator() const { return evaluator_; }

    std::shared_ptr<Population> population() { return population_; }
    std::shared_ptr<const Population> population() const { return population_; }
    std::shared_ptr<Population> offspring() { return offspring_; }
    std::shared_ptr<const Population> offspring() const { return offspring_; }

    std::shared_ptr<Reproduction> reproduction() { return reproduction_; }
    std::shared_ptr<const Reproduction> reproduction() const {
        return reproduction_; }
    std::shared_ptr<EnvironmentalSelection> environmental_selection() {
        return environmental_selection_; }
    std::shared_ptr<const EnvironmentalSelection>
        environmental_selection() const { return environmental_selection_; }
    std::shared_ptr<Mutation> mutation() { return mutation_; }
    std::shared_ptr<const Mutation> mutation() const { return mutation_; }
    std::shared_ptr<Crossover> crossover() { return crossover_; }
    std::shared_ptr<const Crossover> crossover() const { return crossover_; }
    std::shared_ptr<Repair> repair() { return repair_; }
    std::shared_ptr<const Repair> repair() const { return repair_; }
///@}

/**
@brief Enter a re-entrant read-only population observation scope.
*/
    ReadOnlyPopulationScope enter_read_only_population_scope() noexcept;
/**
@brief Check whether a read-only population observation scope is active.
*/
    bool has_read_only_population_scope() const noexcept
    {
        return read_only_population_scope_depth_ > 0;
    }
/**
@brief Return single-pass population fitness summary statistics.

Inside an active ReadOnlyPopulationScope, computes lazily on first request
and reuses the cached summary until the outermost scope exits. Outside a
scope, computes directly on demand without caching.
*/
    PopulationFitnessSummary population_fitness_summary() const;

protected:

/// The name of the algorithm.
    std::string algorithm_name_;
/// The random mechanism.
    std::shared_ptr<Random> random_;

/// The evolution flow of evolutionary algorithm.
    std::shared_ptr<Evolution> evolution_;
/// The test problem.
    std::shared_ptr<Problem> problem_;
/// The statistics used in data analysis.
    std::shared_ptr<Statistics> statistics_;
/**
@brief The parameters storage.

It can be used for temporary storage or passing information to
other evolutionary states.
*/
    std::shared_ptr<Parameters> parameters_;
/// The initializer to initialize population.
    std::shared_ptr<Initializer> initializer_;
/// The evaluator to evaluate individuals.
    std::shared_ptr<Evaluator> evaluator_;
/// The population held in evolution process.
    std::shared_ptr<Population> population_;
/// The offspring produced by the population.
    std::shared_ptr<Population> offspring_;
/// The reproduction process.
    std::shared_ptr<Reproduction> reproduction_;
/// The environmental selection process.
    std::shared_ptr<EnvironmentalSelection> environmental_selection_;
/// The mutation process in reproduction.
    std::shared_ptr<Mutation> mutation_;
/// The crossover process in reproduction.
    std::shared_ptr<Crossover> crossover_;
/// The repair process to repair individuals that are illegal.
    std::shared_ptr<Repair> repair_;

private:

    friend class ReadOnlyPopulationScope;

    void exit_read_only_population_scope() noexcept;
    PopulationFitnessSummary compute_population_fitness_summary() const;

    std::size_t read_only_population_scope_depth_;
    mutable std::optional<PopulationFitnessSummary> population_fitness_summary_;

    virtual std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<Repository>(*this);
    }
};

}

#endif
