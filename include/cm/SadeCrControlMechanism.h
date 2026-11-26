#ifndef SADE_CR_CONTROL_MECHANISM_H
#define SADE_CR_CONTROL_MECHANISM_H

#include <memory>
#include "ControlMechanism.h"

namespace adef {

class BaseFunction;
class Repository;

/**
@brief SadeCrControlMechanism adjusts the object according to
the procedure of the Cr value in the SaDE algorithm.

The difference between this and ControlMechanism:
- select(): the function to record is inside function "mean".
- update(): the function to record is inside function "mean".

@par Requirement
Take out parameters from Parameters:
- type @c std::size_t and name "target_index" in
generate(), select(), and update().
.

@par The configuration
SadeCrControlMechanism has no extra configurations.@n
See setup() for the details.
*/
class SadeCrControlMechanism : public ControlMechanism<double>
{
public:

    using T = double;
/// The type of the object.
    using Object = typename ControlMechanism<T>::Object;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<SadeCrControlMechanism> clone() const
    {
        return std::dynamic_pointer_cast<SadeCrControlMechanism>(clone_impl());
    }

    void select(std::shared_ptr<Repository> repos) override;

    void update(std::shared_ptr<Repository> repos) override;

protected:

    std::shared_ptr<BaseFunction> resolve_update_target(
        std::shared_ptr<BaseFunction> root) const override;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<SadeCrControlMechanism>(*this);
    }
};

}

#endif // SADE_CR_CONTROL_MECHANISM_H
