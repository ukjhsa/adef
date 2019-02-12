#ifndef NORMAL_DIS_FUNCTION_H
#define NORMAL_DIS_FUNCTION_H

#include <memory>
#include <vector>
#include <string>
#include <cstdlib>
#include <random>
#include "Function.h"
#include "Any.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Individual.h"

namespace adef {

/**
@brief NormalDisFunction generates random numbers
according to the Normal distribution.

NormalDisFunction has the following feature:
- generate(): return the random number generated by the Normal distribution.
- record(): record parameters into the mean and/or standard deviation component.
- update(): update values of the mean and standard deviation.

@par Requirement
record parameters into Function:
- name "mean"
- name "stddev"
.

ADEF supports many kinds of NormalDisFunction:
- ::RealNormalDisFunction.

@par The configuration
NormalDisFunction has extra configurations:
- member
    - name: "mean"
    - value: object configuration which is the class derived from
             Function.
- member
    - name: "stddev"
    - value: object configuration which is the class derived from
             Function.
.
See setup() for the details.
*/
template<typename T = double, typename G = std::mt19937>
class NormalDisFunction : public Function<T>
{
public:

/// @copydoc Function::Object
    using Object = typename Function<T>::Object;

/**
@brief The default constructor with seed value 1.

The default value of mean is 0, standard deviation is 1.
*/
    NormalDisFunction() :
        generator_(1), mean_(0), stddev_(1)
    {
    }
/**
@brief The constructor with the given seed.
@param seed The seed value of the pseudo-random number generator.
*/
    NormalDisFunction(unsigned int seed) :
        generator_(seed), mean_(0), stddev_(1)
    {
    }
/**
@brief The copy constructor, but the pseudo-random number generator renews.
*/
    NormalDisFunction(const NormalDisFunction& rhs) :
        Function<T>(rhs),
        generator_(random_->random()),
        mean_(rhs.mean_), stddev_(rhs.stddev_)
    {
    }

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<NormalDisFunction> clone() const
    {
        return std::dynamic_pointer_cast<NormalDisFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the ::RealNormalDisFunction and
has the following configuration:
- mean: ::RealConstantFunction of object 0.0
- stddev: ::RealConstantFunction of object 1.0
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealNormalDisFunction",
    "mean" : {
        "classname" : "RealConstantFunction",
        "object" : 0.0
    },
    "stddev" : {
        "classname" : "RealConstantFunction",
        "object" : 1.0
    }
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        auto mean_config = config.get_config("mean");
        auto mean = make_and_setup_type<BaseFunction>(mean_config, pm);
        mean->set_function_name("mean");
        Function<T>::add_function(mean);

        auto stddev_config = config.get_config("stddev");
        auto stddev = make_and_setup_type<BaseFunction>(stddev_config, pm);
        stddev->set_function_name("stddev");
        Function<T>::add_function(stddev);

        mean_ = 0;
        stddev_ = 1;
    }

    Object generate() override
    {
        std::normal_distribution<Object> normal(mean_, stddev_);
        return normal(generator_);
    }

    void update() override
    {
        auto mean = Function<T>::get_function("mean");
        mean->update();
        mean_ = mean->generate();

        auto stddev = Function<T>::get_function("stddev");
        stddev->update();
        stddev_ = stddev->generate();
    }

    unsigned int number_of_parameters() const override
    {
        return 0;
    }

private:

/// The type of the pseudo-random number generator.
    using Generator = G;
/// The pseudo-random number generator.
    Generator generator_;
/// The value of mean.
    Object mean_;
/// The value of standard deviation.
    Object stddev_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<NormalDisFunction>(*this);
    }
};

/**
@brief RealNormalDisFunction is the NormalDisFunction that
controls the real number.
*/
using RealNormalDisFunction = NormalDisFunction<>;

}

#endif // NORMAL_DIS_FUNCTION_H
