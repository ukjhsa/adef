#ifndef AVERAGE_FITNESS_CONTROL_UPDATE_H
#define AVERAGE_FITNESS_CONTROL_UPDATE_H

#include <memory>
#include "ControlUpdate.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;
class BaseControlParameter;
class BaseFunction;

/**
@brief AverageFitnessControlUpdate updates internal states by
the average fitness value of the current population.

@par Requirement
record parameters into Function:
- name "average"
.

@par The configuration
AverageFitnessControlUpdate has no extra configurations.@n
See setup() for the details.
*/
class AverageFitnessControlUpdate : public ControlUpdate
{
public:

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<AverageFitnessControlUpdate> clone() const
    {
        return std::dynamic_pointer_cast<AverageFitnessControlUpdate>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the AverageFitnessControlUpdate,@n
its configuration should be
- JSON configuration
@code
"SomeThing": {
    "classname" : "AverageFitnessControlUpdate"
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override;

    void update(std::shared_ptr<Repository> repos,
                std::shared_ptr<BaseControlParameter> parameter,
                std::shared_ptr<BaseFunction> function) const override;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<AverageFitnessControlUpdate>(*this);
    }
};

}

#endif // AVERAGE_FITNESS_CONTROL_UPDATE_H
