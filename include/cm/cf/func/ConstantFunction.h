#ifndef CONSTANT_FUNCTION_H
#define CONSTANT_FUNCTION_H

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
@brief ConstantFunction generates the constant object.
@tparam T The type of the object.

ConstantFunction has the following feature:
- generate(): return the object directly.
- record(): do nothing. (because the object is constant.)
- update(): do nothing.

Now ADEF support many kinds of ConstantFunction:
- ::IntegerConstantFunction.
- ::RealConstantFunction.
- ::IntegerControlConstantFunction.
- ::RealControlConstantFunction.

@par The configuration
ConstantFunction has extra configurations:
- member
    - name: "object"
    - value: number configurations or object configurations
             depends on ConstantFunction::Object.
.
See setup() for the details.
*/
template<typename T>
class ConstantFunction : public Function<T>
{
public:

/// @copydoc Function::Object
    using Object = typename Function<T>::Object;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<ConstantFunction> clone() const
    {
        return std::dynamic_pointer_cast<ConstantFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the ::RealConstantFunction and
has the following configuration:
- object: 0.5
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealConstantFunction",
    "object" : 0.5
}
@endcode
.
If @em SomeThing is the ::RealControlConstantFunction and
has the following configuration:
- object: ::RealConstantFunction of object 0.5
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealControlConstantFunction",
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
        auto object_config = config.get_config("object");
        object_ = CO::create(object_config, pm);
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
        return 0;
    }

private:

/// The type of the encapsulated object.
    using CO = ControlledObject<Object>;
/// The object.
    CO object_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<ConstantFunction>(*this);
    }
};

/**
@brief IntegerConstantFunction is the ConstantFunction that
controls the integer number.
*/
using IntegerConstantFunction = ConstantFunction<int>;
/**
@brief RealConstantFunction is the ConstantFunction that
controls the real number.
*/
using RealConstantFunction = ConstantFunction<double>;
/**
@brief IntegerControlConstantFunction is the ConstantFunction
that controls the ::IntegerFunction.
*/
using IntegerControlConstantFunction = ConstantFunction<std::shared_ptr<IntegerFunction>>;
/**
@brief RealControlConstantFunction is the ConstantFunction
that controls the ::RealFunction.
*/
using RealControlConstantFunction = ConstantFunction<std::shared_ptr<RealFunction>>;

}

#endif // CONSTANT_FUNCTION_H
