#ifndef EXPERIMENT_H
#define EXPERIMENT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Prototype.h"

namespace adef {

class Random;
class ExperimentalStatistics;
class Repository;
class Configuration;
class PrototypeManager;

/**
@brief Experiment represents a experiment which contains the number of runs
and informations of the algorithm.

Experiment contains one repository and some experimental information
likes the number of runs.

@par The configuration
Experiment has extra configurations:
- member
    - name: "number_of_runs"
    - value: <tt>unsigned int</tt>
- member (optional)
    - name: "Random"
    - value: object configurations which represents Random. When omitted, a
      default auto-seeded Random instance is created.
- member
    - name: "ExperimentalStatistics"
    - value: object configurations which represents ExperimentalStatistics.
- member
    - name: "Repository"
    - value: object configurations which represents Repository.
.
See setup() for the details.

@sa See System and Repository for the content.
*/
class Experiment : public Prototype
{
public:

    Experiment();
    Experiment(const Experiment& rhs);
    virtual ~Experiment() = default;
    Experiment& operator=(const Experiment& rhs) = delete;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<Experiment> clone() const
    {
        return std::dynamic_pointer_cast<Experiment>(clone_impl());
    }
/**
@brief Set up the internal states.

The name of this experiment sets to the concatenation of
algorithm name and problem name.

If Experiment has the following configuration:
- number_of_runs: 30
.
its configuration should be
- JSON configuration
@code
{
    "classname" : "Experiment",
    "number_of_runs" : 30,
    "ExperimentalStatistics" : {
        ...skip...
    },
    "Repository" : {
        ...skip...
    }
}
@endcode
.
*/
    virtual void setup(const Configuration& config,
                       const PrototypeManager& pm) override;

/**
@brief Execute the algorithm.

It calls Evolution::run() to execute the algorithm.
After finishing all of runs, it output the statistics of entire runs.
*/
    virtual void run();
/**
@brief Return the random mechanism.
*/
    std::shared_ptr<const Random> random() const {
        return random_;
    }
/**
@brief Return the effective per-run seeds used during the most recent run().
*/
    const std::vector<std::uint32_t>& run_seeds() const {
        return run_seeds_;
    }
/**
@brief Return the statistics.
*/
    std::shared_ptr<const ExperimentalStatistics> statistics() const {
        return statistics_;
    }
/**
@brief Return the repository
*/
    std::shared_ptr<const Repository> repository() const { return repository_; }

protected:

/**
@brief The name of the experiment.
*/
    std::string name_;
/**
@brief The number of runs.
*/
    unsigned int number_of_runs_;
/**
@brief The random mechanism on current experiment.
*/
    std::shared_ptr<Random> random_;
/**
@brief Effective per-run seeds derived during run().
*/
    std::vector<std::uint32_t> run_seeds_;
/**
@brief The statistics on the experimental level.
*/
    std::shared_ptr<ExperimentalStatistics> statistics_;
/**
@brief The repository contained by the experiment.
*/
    std::shared_ptr<Repository> repository_;

private:

    virtual std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<Experiment>(*this);
    }
};

}

#endif // EXPERIMENT_H
