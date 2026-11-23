#ifndef UNIFORM_DIS_FUNCTION_H
#define UNIFORM_DIS_FUNCTION_H

#include <memory>
#include <vector>
#include <string>
#include <random>
#include "Function.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Individual.h"

namespace adef {

/**
@brief UniformDisFunction generates random numbers according to
the uniform distribution.
@tparam T The type of the object. It must be arithmetic type.

UniformDisFunction has the following feature:
- generate(): return the random number
according to the uniform distribution.
- record(): record parameters into the lower bound and/or upper bound component.
- update(): update values of the lower bound and upper bound.

@par Requirement
record parameters into Function:
- name "lower_bound"
- name "upper_bound"
.

ADEF supports many kinds of UniformDisFunction:
- IntegerUniformDisFunction.
- RealUniformDisFunction.

@par The configuration
UniformDisFunction has extra configurations:
- member
    - name: "lower_bound"
    - value: object configuration which is the class derived from
             Function.
- member
    - name: "upper_bound"
    - value: object configuration which is the class derived from
             Function.
.
See setup() for the details.
*/
template<typename T>
class UniformDisFunction : public Function<T>
{
    static_assert(std::is_arithmetic<T>::value,
                  "UniformDisFunction contains only arithmetic type");
public:

/// The type of the object.
    using Object = typename Function<T>::Object;

/**
@brief The default constructor.

The default value of the lower bound is 0, upper bound is 1.
*/
    UniformDisFunction() :
        lower_bound_(0), upper_bound_(1)
    {
    }
/**
@brief The copy constructor.
*/
    UniformDisFunction(const UniformDisFunction& rhs) :
        Function<T>(rhs),
        lower_bound_(rhs.lower_bound_), upper_bound_(rhs.upper_bound_)
    {
    }

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<UniformDisFunction> clone() const
    {
        return std::dynamic_pointer_cast<UniformDisFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the ::RealUniformDisFunction and
has the following configuration:
- lower_bound: ::RealConstantFunction of value 0.0
- upper_bound: ::RealConstantFunction of value 1.0
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealUniformDisFunction",
    "lower_bound" : {
        "classname" : "RealConstantFunction",
        "value" : 0.0
    },
    "upper_bound" : {
        "classname" : "RealConstantFunction",
        "value" : 1.0
    }
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        auto lower_bound_config = config.get_config("lower_bound");
        auto lower_bound = make_and_setup_type<BaseFunction>(lower_bound_config, pm);
        lower_bound->set_function_name("lower_bound");
        Function<T>::add_function(lower_bound);

        auto upper_bound_config = config.get_config("upper_bound");
        auto upper_bound = make_and_setup_type<BaseFunction>(upper_bound_config, pm);
        upper_bound->set_function_name("upper_bound");
        Function<T>::add_function(upper_bound);

        lower_bound_ = 0;
        upper_bound_ = 1;
    }

    Object generate() override
    {
        return generate_impl<>();
    }

    void update() override
    {
        auto lower_bound = Function<T>::get_function("lower_bound");
        lower_bound->update();
        lower_bound_ = lower_bound->generate();

        auto upper_bound = Function<T>::get_function("upper_bound");
        upper_bound->update();
        upper_bound_ = upper_bound->generate();
    }

    unsigned int number_of_parameters() const override
    {
        return 0;
    }

private:

/// The value of the lower bound.
    Object lower_bound_;
/// The value of the upper bound.
    Object upper_bound_;

private:

    template<typename U = Object>
    U generate_impl(
        std::enable_if_t<std::is_integral<U>::value>* = nullptr)
    {
        std::uniform_int_distribution<> uniform(lower_bound_, upper_bound_);
        return BaseFunction::random_->generate(uniform);
    }

    template<typename U = Object>
    U generate_impl(
        std::enable_if_t<std::is_floating_point<U>::value>* = nullptr)
    {
        std::uniform_real_distribution<> uniform(lower_bound_, upper_bound_);
        return BaseFunction::random_->generate(uniform);
    }

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<UniformDisFunction>(*this);
    }
};

/**
@brief IntegerUniformDisFunction is the UniformDisFunction that
controls the integer number.
*/
using IntegerUniformDisFunction = UniformDisFunction<int>;
/**
@brief RealUniformDisFunction is the UniformDisFunction that
controls the real number.
*/
using RealUniformDisFunction = UniformDisFunction<double>;

}

#endif // UNIFORM_DIS_FUNCTION_H
