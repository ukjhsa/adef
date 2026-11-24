#ifndef RANDOM_SELECTION_FUNCTION_H
#define RANDOM_SELECTION_FUNCTION_H

#include <memory>
#include <vector>
#include <string>
#include <any>
#include <cstddef>
#include <stdexcept>
#include "Function.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Individual.h"

namespace adef {

/**
@brief RandomSelectionFunction generate the object
that is randomly selected from storage.

RandomSelectionFunction contains a variety of objects.
When calling generate(), it randomly selects one from them.

RandomSelectionFunction has the following feature:
- generate(): return the randomly selected object.
- record(): do nothing.
- update(): do nothing.

Now ADEF support many kinds of RandomSelectionFunction:
- ::IntegerRandomSelectionFunction.
- ::RealRandomSelectionFunction.
- ::IntegerControlRandomSelectionFunction.
- ::RealControlRandomSelectionFunction.

@par The configuration
ConstantFunction has extra configurations:
- member:
    - name: "object"
    - value: array configurations
        - element: number configurations or object configurations
                   depends on RandomSelectionFunction::Object
.
See setup() for the details.
*/
template<typename T>
class RandomSelectionFunction : public Function<T>
{
public:

/// @copydoc Function::Object
    using Object = typename Function<T>::Object;

/**
@brief The default constructor.
*/
    RandomSelectionFunction() :
        objects_()
    {
    }

/**
@brief The copy constructor.
*/
    RandomSelectionFunction(const RandomSelectionFunction& rhs) :
        Function<T>(rhs),
        objects_(rhs.objects_)
    {
    }

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<RandomSelectionFunction> clone() const
    {
        return std::dynamic_pointer_cast<RandomSelectionFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the ::RealRandomSelectionFunction and
has the following configuration:
- object: 0.3, 0.5
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealRandomSelectionFunction",
    "object" : [
        0.3,
        0.5
    ]
}
@endcode
.
If @em SomeThing is the ::RealControlRandomSelectionFunction and
has the following configuration:
- object: ::RealConstantFunction of object 0.3,
          ::RealConstantFunction of object 0.5
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealControlRandomSelectionFunction",
    "object" : [
        {
            "classname" : "RealConstantFunction",
            "object" : 0.3
        },
        {
            "classname" : "RealConstantFunction",
            "object" : 0.5
        }
    ]
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        auto object_config = config.get_config("object");
        auto object_size = object_config.get_array_size();
        for (decltype(object_size) idx = 0; idx < object_size; ++idx) {

            auto element_config = object_config.get_config(idx);
            auto element = CO::create(element_config, pm);
            objects_.push_back(element);
        }
    }

    void init(std::shared_ptr<Repository> repos) override
    {
        BaseFunction::init(repos);
        if constexpr (is_shared_ptr<Object>::value) {
            if constexpr (std::is_convertible_v<typename Object::element_type*, BaseFunction*>) {
                for (auto& element : objects_) {
                    if (element.object) {
                        element.object->init(repos);
                    }
                }
            }
        }
    }

    Object generate() override
    {
        if (objects_.empty()) {
            throw std::logic_error("RandomSelectionFunction has no objects.");
        }
        auto index = BaseFunction::random_->uniform_index(objects_.size());

        return objects_.at(index).object;
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
        return true;
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
/// The list of objects.
    std::vector<CO> objects_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<RandomSelectionFunction>(*this);
    }
};

/**
@brief IntegerRandomSelectionFunction is the
RandomSelectionFunction that controls the integer number.
*/
using IntegerRandomSelectionFunction = RandomSelectionFunction<int>;
/**
@brief RealRandomSelectionFunction is the
RandomSelectionFunction that controls the real number.
*/
using RealRandomSelectionFunction = RandomSelectionFunction<double>;
/**
@brief IntegerControlRandomSelectionFunction is the
RandomSelectionFunction that controls the ::IntegerFunction.
*/
using IntegerControlRandomSelectionFunction = RandomSelectionFunction<std::shared_ptr<IntegerFunction>>;
/**
@brief RealControlRandomSelectionFunction is the
RandomSelectionFunction that controls the ::RealFunction.
*/
using RealControlRandomSelectionFunction = RandomSelectionFunction<std::shared_ptr<RealFunction>>;

}

#endif // RANDOM_SELECTION_FUNCTION_H
