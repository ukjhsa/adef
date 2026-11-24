#ifndef MIN_FITNESS_CONTROL_UPDATE_H
#define MIN_FITNESS_CONTROL_UPDATE_H

#include <memory>
#include "ControlUpdate.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;
class BaseControlParameter;
class BaseFunction;

/**
@brief MinFitnessControlUpdate updates internal states by
the minimum fitness value of the current population.

@par Requirement
record parameters into Function:
- name "min"
.

@par The configuration
MinFitnessControlUpdate has no extra configurations.@n
See setup() for the details.
*/
class MinFitnessControlUpdate : public ControlUpdate
{
public:

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<MinFitnessControlUpdate> clone() const
    {
        return std::dynamic_pointer_cast<MinFitnessControlUpdate>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is MinFitnessControlUpdate,@n
its configuration should be
- JSON configuration
@code
"SomeThing": {
    "classname" : "MinFitnessControlUpdate"
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
        return std::make_shared<MinFitnessControlUpdate>(*this);
    }
};

}

#endif // MIN_FITNESS_CONTROL_UPDATE_H
