#ifndef SINGLE_CONTROL_PARAMETER_H
#define SINGLE_CONTROL_PARAMETER_H

#include <memory>
#include <cstddef>
#include "ControlParameter.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "cm/cf/func/Function.h"

namespace adef {

/**
@brief SingleControlParameter manages single object.
@tparam T The type of the object.

@par The configuration
SingleControlParameter has extra configurations:
- member
    - name: "initial_value"
    - value: nothing or number configurations or
             object configurations which is the class derived from
             Function.
.
See setup() for the details.
*/
template<typename T, typename Enable = void>
class SingleControlParameter : public ControlParameter<T>
{
public:

/// The type of the object.
    using Object = typename ControlParameter<T>::Object;

    SingleControlParameter() : object_{}, already_generated_(false) {}
    SingleControlParameter(const SingleControlParameter& rhs) :
        ControlParameter<T>(rhs),
        object_(rhs.object_),
        already_generated_(rhs.already_generated_),
        initial_function_(rhs.initial_function_ ? rhs.initial_function_->clone() : nullptr)
    {
    }

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<SingleControlParameter> clone() const
    {
        return std::dynamic_pointer_cast<SingleControlParameter>(clone_impl());
    }
/**
@brief Set up the internal states.

If ::RealSingleControlParameter is used and has the following configuration:
- initial_value: 0.5
.
its configuration should be
- JSON configuration
@code
"ControlParameter" : {
    "classname" : "RealSingleControlParameter",
    "initial_value" : 0.5
}
@endcode
.
If ::RealSingleControlParameter is used and has the following configuration:
- initial_value: RealNormalDisFunction of mean 0.0 and
                 standard deviation 1.0
.
its configuration should be
- JSON configuration
@code
"ControlParameter" : {
    "classname" : "RealSingleControlParameter",
    "initial_value" : {
        "classname" : "RealNormalDisFunction",
        "mean" : {
            "classname" : "RealConstantFunction",
            "value" : 0.0
        },
        "stddev" : {
            "classname" : "RealConstantFunction",
            "value" : 1.0
        }
    }
}
@endcode
.
If ::RealSingleControlParameter is used and has no extra configuration.@n
its configuration should be
- JSON configuration
@code
"ControlParameter" : {
    "classname" : "RealSingleControlParameter"
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        auto init_config = config.get_config("initial_value");
        // initial value is arithmetic
        if (init_config.is_number()) {
            object_ = init_config.get_value<Object>();
            initial_function_.reset();
        }
        // initial value is generated from Function
        else if (init_config.is_object()) {
            initial_function_ = make_and_setup_type<Function<Object>>(
                                                            init_config, pm);
        }
        // initial value is default value of Object type
        else {
            initial_function_.reset();
        }
    }

    void init(std::shared_ptr<Repository> repos) override
    {
        if (initial_function_) {
            initial_function_->init(repos);
            initial_function_->update();
            object_ = initial_function_->generate();
        }
    }

/**
@name object access
*/
///@{
/**
@copydoc ControlParameter::save()

It ignores @a index because there is only one object
and set flag of already generated.
*/
    void save(const Object& object, std::size_t index) override
    {
        object_ = object;
        already_generated_ = true;
    }
/**
@copydoc ControlParameter::load()

It ignores @a index because there is only one object.
*/
    Object load(std::size_t index) const override
    {
        return object_;
    }
///@}

/**
@name object quantity
*/
///@{
/**
@copydoc ControlParameter::number_of_objects()

It always returns 1 because there is exactly one object.
*/
    std::size_t number_of_objects() const override
    {
        return 1;
    }
///@}

/**
@copydoc ControlParameter::is_already_generated()

It ignores @a index because there is only one object needed to check.
*/
    bool is_already_generated(std::size_t index) const override
    {
        return already_generated_;
    }
/**
@copydoc ControlParameter::reset_already_generated()

It ignores @a index because there is only one object needed to check.
*/
    void reset_already_generated(std::size_t /*index*/) override
    {
        already_generated_ = false;
    }

    void reset_all_already_generated() override
    {
        already_generated_ = false;
    }

private:

/// The stored object.
    Object object_;
/// The flag of already generated
    bool already_generated_;
/// Optional Function used to initialize object_ during init().
    std::shared_ptr<Function<Object>> initial_function_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<SingleControlParameter>(*this);
    }
};

/**
@brief SingleControlParameter template specialization for non-arithmetic type.
@tparam T The type of the object is the non-arithmetic type.

This template specialization does NOT have the member configuration
named "initial_value"

@par The configuration
SingleControlParameter template specialization has no extra configurations.@n
See setup() for the details.

@sa SingleControlParameter.
*/
template<typename T>
class SingleControlParameter<T, std::enable_if_t<!std::is_arithmetic<T>::value>>
    : public ControlParameter<T>
{
public:

/// The type of the object.
    using Object = typename ControlParameter<T>::Object;

    SingleControlParameter() : already_generated_(false) {}
    SingleControlParameter(const SingleControlParameter& rhs) = default;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<SingleControlParameter> clone() const
    {
        return std::dynamic_pointer_cast<SingleControlParameter>(clone_impl());
    }
/**
@brief Set up the internal states.

If ::RealControlSingleControlParameter is used.@n
its configuration should be
- JSON configuration
@code
"ControlParameter" : {
    "classname" : "RealControlSingleControlParameter"
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
    }
/**
@name object access
*/
///@{
/**
@copydoc ControlParameter::save()

It ignores @a index because there is only one object
and set flag of already generated.
*/
    void save(const Object& object, std::size_t index) override
    {
        object_ = object;
        already_generated_ = true;
    }
/**
@copydoc ControlParameter::load()

It ignores @a index because there is only one object.
*/
    Object load(std::size_t index) const override
    {
        return object_;
    }
///@}

/**
@name object quantity
*/
///@{
/**
@copydoc ControlParameter::number_of_objects()

It always returns 1 because there is exactly one object.
*/
    std::size_t number_of_objects() const override
    {
        return 1;
    }
///@}

/**
@copydoc ControlParameter::is_already_generated()

It ignores @a index because there is only one object needed to check.
*/
    bool is_already_generated(std::size_t index) const override
    {
        return already_generated_;
    }
/**
@copydoc ControlParameter::reset_already_generated()

It ignores @a index because there is only one object needed to check.
*/
    void reset_already_generated(std::size_t /*index*/) override
    {
        already_generated_ = false;
    }

    void reset_all_already_generated() override
    {
        already_generated_ = false;
    }

private:

/// The stored object.
    Object object_;
/// The flag of already generated
    bool already_generated_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<SingleControlParameter>(*this);
    }
};

/**
@brief IntegerSingleControlParameter is the SingleControlParameter that
controls the integer number.
*/
using IntegerSingleControlParameter = SingleControlParameter<int>;
/**
@brief RealSingleControlParameter is the SingleControlParameter that
controls the real number.
*/
using RealSingleControlParameter = SingleControlParameter<double>;
/**
@brief IntegerControlSingleControlParameter is the SingleControlParameter that
controls the ::IntegerFunction.
*/
using IntegerControlSingleControlParameter = SingleControlParameter<std::shared_ptr<IntegerFunction>>;
/**
@brief RealControlSingleControlParameter is the SingleControlParameter that
controls the ::RealFunction.
*/
using RealControlSingleControlParameter = SingleControlParameter<std::shared_ptr<RealFunction>>;

}

#endif // SINGLE_CONTROL_PARAMETER_H
