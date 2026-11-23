#ifndef VARIABLE_FUNCTION_H
#define VARIABLE_FUNCTION_H

#include <memory>
#include <vector>
#include <string>
#include <any>
#include "Function.h"
#include "cm/ControlledObject.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Individual.h"

namespace adef {

/**
@brief VariableFunction generates the object
that can be changed by recording another object.
@tparam T The type of the object.

VariableFunction has the following feature:
- generate(): return the object.
- record(): replace the object.
- update(): do nothing.

Now ADEF support many kinds of VariableFunction:
- ::IntegerVariableFunction.
- ::RealVariableFunction.
- ::IntegerControlVariableFunction.
- ::RealControlVariableFunction.

@par The configuration
VariableFunction has extra configurations:
- member
    - name: "object"
    - value: nothing or number configurations or object configurations
             depends on VariableFunction::Object.
.
See setup() for the details.
*/
template<typename T>
class VariableFunction : public Function<T>
{
public:

/// @copydoc Function::Object
    using Object = typename Function<T>::Object;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<VariableFunction> clone() const
    {
        return std::dynamic_pointer_cast<VariableFunction>(clone_impl());
    }

/**
@brief Set up the internal states.

If @em SomeThing is the ::RealVariableFunction,@n
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealVariableFunction"
}
@endcode
.
If @em SomeThing is the ::RealVariableFunction and
has the following configuration:
- object: 0.5
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealVariableFunction",
    "object" : 0.5
}
@endcode
.
If @em SomeThing is the ::RealControlVariableFunction and
has the following configuration:
- object: ::RealConstantFunction of object 0.5
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealControlVariableFunction",
    "object" : {
        "classname" : "RealConstantFunction",
        "object" : 0.5
    }
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        using CO = typename Function<T>::CO;

        auto object_config = config.get_config("object");
        if (object_config.is_null()) {
            object_ = CO::create(Object());
        }
        else {
            object_ = CO::create(object_config, pm);
        }
    }

    void init(std::shared_ptr<Repository> repos) override
    {
        BaseFunction::init(repos);
        if constexpr (is_shared_ptr<Object>::value) {
            if constexpr (std::is_convertible_v<typename Object::element_type*, BaseFunction*>) {
                if (object_.object) {
                    object_.object->init(repos);
                }
            }
        }
    }

    Object generate() override
    {
        return object_.object;
    }

    bool record(const std::vector<std::any>& params,
                const std::string& name = "") override
    {
        object_.object = std::any_cast<Object>(params.front());
        return true;
    }

    bool record(const std::vector<std::any>& params,
                std::shared_ptr<const Individual> parent,
                std::shared_ptr<const Individual> offspring,
                const std::string& name = "") override
    {
        return record(params, name);
    }

    void update() override
    {
    }

    unsigned int number_of_parameters() const override
    {
        return 1;
    }

private:

/// The type of the encapsulated object.
    using CO = ControlledObject<Object>;
/// The object.
    CO object_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<VariableFunction>(*this);
    }
};

/**
@brief IntegerVariableFunction is the VariableFunction that
controls the integer number.
*/
using IntegerVariableFunction = VariableFunction<int>;
/**
@brief RealVariableFunction is the VariableFunction that
controls the real number.
*/
using RealVariableFunction = VariableFunction<double>;
/**
@brief IntegerControlVariableFunction is the VariableFunction
that controls the ::IntegerFunction.
*/
using IntegerControlVariableFunction = VariableFunction<std::shared_ptr<IntegerFunction>>;
/**
@brief RealControlVariableFunction is the VariableFunction
that controls the ::RealFunction.
*/
using RealControlVariableFunction = VariableFunction<std::shared_ptr<RealFunction>>;

}

#endif // VARIABLE_FUNCTION_H
