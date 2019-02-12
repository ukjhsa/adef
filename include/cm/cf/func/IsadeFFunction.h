#ifndef ISADE_F_FUNCTION_H
#define ISADE_F_FUNCTION_H

#include <memory>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <random>
#include "Function.h"
#include "Any.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Individual.h"

namespace adef {

/**
@brief IsadeFFunction generates numbers according to
the procedure of the F value in the ISADE algorithm.

@c IsadeFFunction has the following feature:
- generate() : return the numbers generated by
the procedure of the F value in the ISADE algorithm.
- record() : record parameters into the object, min, average, current
            and/or tau component.
- update() : update values of the object, min, average, current and tau.

@par Requirement
record parameters into Function:
- name "object"
- name "min"
- name "average"
- name "current"
- name "tau"
.

ADEF supports many kinds of IsadeFFunction:
- IsadeFFunction.

@par The configuration
IsadeCrFunction has extra configurations:
- member
    - name: "object"
    - value: object configuration which is the ::RealVariableFunction.
- member
    - name: "min"
    - value: object configuration which is the ::RealVariableFunction.
- member
    - name: "average"
    - value: object configuration which is the ::RealVariableFunction.
- member
    - name: "current"
    - value: object configuration which is the ::RealVariableFunction.
- member
    - name: "tau"
    - value: object configuration which is the class derived from
             Function.
.
See setup() for the details.
*/
class IsadeFFunction : public Function<double>
{
public:

    using T = double;

/// @copydoc Function::Object
    using Object = typename Function<T>::Object;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<IsadeFFunction> clone() const
    {
        return std::dynamic_pointer_cast<IsadeFFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the IsadeFFunction and
has the following configuration:
- object: ::RealVariableFunction of object 0.5
- tau: ::RealConstantFunction of object 0.1
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "IsadeFFunction",
    "object" : {
        "classname" : "RealVariableFunction",
        "object" : 0.5
    },
    "min" : {
        "classname" : "RealVariableFunction"
    },
    "average" : {
        "classname" : "RealVariableFunction"
    },
    "current" : {
        "classname" : "RealVariableFunction"
    },
    "tau" : {
        "classname" : "RealConstantFunction",
        "object" : 0.1
    }
}
@endcode
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        auto object_config = config.get_config("object");
        auto object = make_and_setup_type<BaseFunction>(object_config, pm);
        object->set_function_name("object");
        add_function(object);

        auto min_config = config.get_config("min");
        auto min = make_and_setup_type<BaseFunction>(min_config, pm);
        min->set_function_name("min");
        add_function(min);

        auto average_config = config.get_config("average");
        auto average = make_and_setup_type<BaseFunction>(average_config, pm);
        average->set_function_name("average");
        add_function(average);

        auto current_config = config.get_config("current");
        auto current = make_and_setup_type<BaseFunction>(current_config, pm);
        current->set_function_name("current");
        add_function(current);

        auto tau_config = config.get_config("tau");
        auto tau = make_and_setup_type<BaseFunction>(tau_config, pm);
        tau->set_function_name("tau");
        add_function(tau);

        object_ = 0;
        min_ = 0;
        average_ = 0;
        current_ = 0;
        tau_ = 0.1;
    }

    Object generate() override
    {
        std::mt19937 gen(random_->random());
        std::uniform_real_distribution<> uniform;
        auto random = uniform(gen);
        if (random < tau_ && current_ < average_) {
            if (std::abs(average_ - min_) < std::numeric_limits<Object>::epsilon()) {
                average_ += std::numeric_limits<Object>::epsilon();
            }
            return 0.1 + (object_ - 0.1) *
                   (current_ - min_) / (average_ - min_);
        }
        else if (random < tau_ && current_ >= average_) {
            std::uniform_real_distribution<> uniform(0.1, 1);
            return uniform(gen);
        }
        else {
            return object_;
        }
    }

    void update() override
    {
        auto object = get_function("object");
        object->update();
        object_ = object->generate();

        auto min = get_function("min");
        min->update();
        min_ = min->generate();

        auto average = get_function("average");
        average->update();
        average_ = average->generate();

        auto current = get_function("current");
        current->update();
        current_ = current->generate();

        auto tau = get_function("tau");
        tau->update();
        tau_ = tau->generate();
    }

    unsigned int number_of_parameters() const override
    {
        return 0;
    }

private:

/// The value of the parameter.
    Object object_;
/// The value of the min of fitness.
    Object min_;
/// The value of the average of fitness.
    Object average_;
/// The value of the fitness of the current individual.
    Object current_;
/// The value of the tau.
    Object tau_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<IsadeFFunction>(*this);
    }
};

}

#endif // ISADE_F_FUNCTION_H
