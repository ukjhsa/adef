#ifndef MULTIPLE_CONTROL_FUNCTION_H
#define MULTIPLE_CONTROL_FUNCTION_H

#include <memory>
#include <vector>
#include "ControlFunction.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;
class BaseFunction;

/**
@brief MultipleControlFunction manages multiple functions.

@par The configuration
MultipleControlFunction has extra configurations:
- member
    - name: "number_of_functions"
    - value: <tt>unsigned int</tt>
- member
    - name: "Function"
    - value: object configurations which represents Function.
.
See setup() for the details.
*/
class MultipleControlFunction : public ControlFunction
{
public:

    MultipleControlFunction() = default;
    MultipleControlFunction(const MultipleControlFunction& rhs);

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<MultipleControlFunction> clone() const
    {
        return std::dynamic_pointer_cast<MultipleControlFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If MultipleControlFunction has the following configuration:
- number_of_functions: 100
- Function: ::RealConstantFunction of object 0.5
.
its configuration should be
- JSON configuration
@code
"ControlFunction" : {
    "classname" : "MultipleControlFunction",
    "number_of_functions" : 100,
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

    std::shared_ptr<BaseFunction> at(std::size_t index) override;

    std::size_t number_of_functions() const override
    {
        return functions_.size();
    }

private:

/// The function.
    std::vector<std::shared_ptr<BaseFunction>> functions_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<MultipleControlFunction>(*this);
    }
};

}

#endif // MULTIPLE_CONTROL_FUNCTION_H
