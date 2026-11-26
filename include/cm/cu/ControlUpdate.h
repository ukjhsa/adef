#ifndef CONTROL_UPDATE_H
#define CONTROL_UPDATE_H

#include <memory>
#include "Prototype.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;
class BaseControlParameter;
class BaseFunction;

/**
@brief Declares the update granularity of a ControlUpdate.
*/
enum class UpdateScope
{
/// Value is invariant across individuals within a generation.
    Generation,
/// Value varies per individual (or has per-individual side effects / RNG draws).
    Individual
};

/**
@brief ControlUpdate is the abstract base class of
how to update internal states by the current state.

It is called by ControlMechanism::update().

@par The configuration
ControlUpdate has no extra configurations.@n
See setup() for the details.

@sa ControlMechanism
*/
class ControlUpdate : public Prototype
{
public:

    virtual ~ControlUpdate() = default;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<ControlUpdate> clone() const
    {
        return std::dynamic_pointer_cast<ControlUpdate>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the @em Class which is derived from ControlUpdate,@n
its configuration should be
- JSON configuration
@code
"SomeThing": {
    "classname" : "Class"
}
@endcode
.
*/
    virtual void setup(const Configuration& config,
                       const PrototypeManager& pm) override = 0;
/**
@brief Return the update granularity scope of this ControlUpdate.

Defaults to UpdateScope::Individual so custom or unknown subclasses retain
per-individual update semantics safely.
*/
    virtual UpdateScope update_scope() const
    {
        return UpdateScope::Individual;
    }
/**
@brief Update the internal states by given method.
*/
    virtual void update(std::shared_ptr<Repository> repos,
                        std::shared_ptr<BaseControlParameter> parameter,
                        std::shared_ptr<BaseFunction> function) const = 0;

private:

    virtual std::shared_ptr<Prototype> clone_impl() const override = 0;
};

}

#endif // CONTROL_UPDATE_H
