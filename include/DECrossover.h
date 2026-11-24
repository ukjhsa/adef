#ifndef DE_CROSSOVER_H
#define DE_CROSSOVER_H

#include <memory>
#include "Crossover.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;
class Individual;
class DEVariantZ;
class BaseControlMechanism;

/**
@brief DECrossover controls the crossover flow according to the DE algorithm.

crossover() calls ControlMechanism::update_generation(),
ControlMechanism::update_individual(), and ControlMechanism::generate() of
the parameter @em CR.

@par The configuration
DECrossover has extra configurations:
- member
    - name: "DEVariantZ"
    - value: object configurations which represents DEVariantZ.
- member
    - name: "CR"
    - value: object configurations which represents ControlMechanism.
.
See setup() for the details.
*/
class DECrossover : public Crossover
{
public:

    DECrossover() = default;
    DECrossover(const DECrossover& rhs);
    DECrossover& operator=(const DECrossover& rhs) = delete;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<DECrossover> clone() const
    {
        return std::dynamic_pointer_cast<DECrossover>(clone_impl());
    }
/**
@brief Set up the internal states.

DECrossover's configuration should be
- JSON configuration
@code
"Crossover" : {
    "classname" : "DECrossover",
    "DEVariantZ" : {
        ...skip...
    },
    "CR" : {
        ...skip...
    }
}
@endcode
.
*/
    void setup(const Configuration& config,
               const PrototypeManager& pm) override;

    void init(std::shared_ptr<Repository> repos) override;

    void crossover(std::shared_ptr<Repository> repos) const override;

/**
@brief Exchange and produce a child in DE.
@param mutant_vector The mutated vector.
@param target_vector The target vector.
@param crossover_rate The crossover probability.
@return The trial vector.
*/
    std::shared_ptr<Individual> crossover(
                                std::shared_ptr<const Individual> mutant_vector,
                                std::shared_ptr<const Individual> target_vector,
                                double crossover_rate) const;

private:

/**
@brief The DE variant @e z.
*/
    std::shared_ptr<DEVariantZ> variant_z_;
/**
@brief The crossover probability used in crossover.
*/
    std::shared_ptr<BaseControlMechanism> cr_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<DECrossover>(*this);
    }
};

}

#endif
