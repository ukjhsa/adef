#ifndef CONTROL_FUNCTION_H
#define CONTROL_FUNCTION_H

#include <cstddef>
#include <functional>
#include <memory>
#include "Prototype.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;
class BaseFunction;

/**
@brief ControlFunction is the abstract base class of functions storage.

ControlFunction defines the following interfaces:
- at(): return the function.

@par The configuration
ControlFunction has no extra configurations.@n
See setup() for the details.
*/
class ControlFunction : public Prototype
{
public:

    virtual ~ControlFunction() = default;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<ControlFunction> clone() const
    {
        return std::dynamic_pointer_cast<ControlFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em Class is derived from ControlFunction, its configuration should be
- JSON configuration
@code
"ControlFunction": {
    "classname" : "Class"
}
@endcode
.
*/
    virtual void setup(const Configuration& config,
                       const PrototypeManager& pm) override = 0;
/**
@brief Initialize this state from other states.
@param repos The Repository to get initialization informations.
*/
    virtual void init(std::shared_ptr<Repository> repos) = 0;
/**
@brief Return the function at position @em index.
@param index The index of function to return.
*/
    virtual std::shared_ptr<BaseFunction> at(std::size_t index) = 0;
/**
@brief Return the number of unique managed functions.
*/
    virtual std::size_t number_of_functions() const = 0;
/**
@brief Invoke @em func once for each unique managed function.
*/
    virtual void for_each_function(
        const std::function<void(std::shared_ptr<BaseFunction>)>& func)
    {
        const auto count = number_of_functions();
        for (std::size_t idx = 0; idx < count; ++idx) {
            auto fn = at(idx);
            if (fn) {
                func(fn);
            }
        }
    }

private:

    virtual std::shared_ptr<Prototype> clone_impl() const override = 0;
};

}

#endif // CONTROL_FUNCTION_H
