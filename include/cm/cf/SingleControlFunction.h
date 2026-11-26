#ifndef SINGLE_CONTROL_FUNCTION_H
#define SINGLE_CONTROL_FUNCTION_H

#include <memory>
#include "ControlFunction.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;
class BaseFunction;

/**
@brief SingleControlFunction manages single function.

@par The configuration
SingleControlFunction has extra configurations:
- member
    - name: "Function"
    - value: object configurations which represents Function.
.
See setup() for the details.
*/
class SingleControlFunction : public ControlFunction
{
public:

    SingleControlFunction() = default;
    SingleControlFunction(const SingleControlFunction& rhs);

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<SingleControlFunction> clone() const
    {
        return std::dynamic_pointer_cast<SingleControlFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If SingleControlFunction has the following configuration:
- Function: ::RealConstantFunction of object 0.5
.
its configuration should be
- JSON configuration
@code
"ControlFunction" : {
    "classname" : "SingleControlFunction",
    "Function" : {
        "classname" : "RealConstantFunction",
        "object" : 0.5
    }
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override;

    void init(std::shared_ptr<Repository> repos) override;
/**
@copydoc ControlFunction::at()

It ignores @em index because there is only one function.
*/
    std::shared_ptr<BaseFunction> at(std::size_t /*index*/) override
    {
        return function_;
    }

    std::size_t number_of_functions() const override
    {
        return function_ ? 1 : 0;
    }

private:

/// The function.
    std::shared_ptr<BaseFunction> function_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<SingleControlFunction>(*this);
    }
};

}

#endif // SINGLE_CONTROL_FUNCTION_H
