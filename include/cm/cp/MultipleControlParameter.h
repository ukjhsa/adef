#ifndef MULTIPLE_CONTROL_PARAMETER_H
#define MULTIPLE_CONTROL_PARAMETER_H

#include <algorithm>
#include <memory>
#include <cstddef>
#include <vector>
#include "ControlParameter.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "cm/cf/func/Function.h"

namespace adef {

/**
@brief MultipleControlParameter manages multiple objects.
@tparam T The type of the object.

@par The configuration
MultipleControlParameter has extra configurations:
- member
    - name: "number_of_objects"
    - value: <tt>unsigned int</tt>
- member
    - name: "initial_value"
    - value: nothing or number configurations or
             object configurations which is the class derived from
             Function.
.
See setup() for the details.
*/
template<typename T, typename Enable = void>
class MultipleControlParameter : public ControlParameter<T>
{
public:

/// The type of the object.
    using Object = typename ControlParameter<T>::Object;

    MultipleControlParameter() = default;
    MultipleControlParameter(const MultipleControlParameter& rhs) :
        ControlParameter<T>(rhs),
        objects_(rhs.objects_),
        already_generated_(rhs.already_generated_),
        initial_function_(rhs.initial_function_ ? rhs.initial_function_->clone() : nullptr)
    {
    }

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<MultipleControlParameter> clone() const
    {
        return std::dynamic_pointer_cast<MultipleControlParameter>(clone_impl());
    }
/**
@brief Set up the internal states.

If ::RealMultipleControlParameter is used and has the following configuration:
- number_of_objects: 100
- initial_value: 0.5
.
its configuration should be
- JSON configuration
@code
"ControlParameter" : {
    "classname" : "RealMultipleControlParameter",
    "number_of_objects" : 100,
    "initial_value" : 0.5
}
@endcode
.
If ::RealMultipleControlParameter is used and has the following configuration:
- number_of_objects: 100
- initial_value: RealNormalDisFunction of mean 0.0 and
                 standard deviation 1.0
.
its configuration should be
- JSON configuration
@code
"ControlParameter" : {
    "classname" : "RealMultipleControlParameter",
    "number_of_objects" : 100,
    "initial_value" : {
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
}
@endcode
.
If ::RealMultipleControlParameter is used and has the following configuration:
- number_of_objects: 100
.
its configuration should be
- JSON configuration
@code
"ControlParameter" : {
    "classname" : "RealMultipleControlParameter",
    "number_of_objects" : 100
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        auto size = config.get_uint_value("number_of_objects");
        objects_.resize(size);
        already_generated_.resize(size, false);

        auto init_config = config.get_config("initial_value");
        // initial value is arithmetic
        if (init_config.is_number()) {
            objects_.assign(size, init_config.get_value<Object>());
            initial_function_.reset();
        }
        // initial value is generated from Function
        else if (init_config.is_object()) {
            initial_function_ = make_and_setup_type<Function<Object>>(init_config, pm);
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
            for (auto& object : objects_) {
                initial_function_->update();
                object = initial_function_->generate();
            }
        }
    }
/**
@name object access
*/
///@{
    void save(const Object& object, std::size_t index) override
    {
        objects_.at(index) = object;
        already_generated_.at(index) = true;
    }

    Object load(std::size_t index) const override
    {
        return objects_.at(index);
    }
///@}

/**
@name object quantity
*/
///@{
    std::size_t number_of_objects() const override
    {
        return objects_.size();
    }
///@}

    bool is_already_generated(std::size_t index) const override
    {
        return already_generated_.at(index);
    }

    void reset_already_generated(std::size_t index) override
    {
        already_generated_.at(index) = false;
    }

    void reset_all_already_generated() override
    {
        std::fill(already_generated_.begin(), already_generated_.end(), false);
    }

private:

/// The list of stored objects.
    std::vector<Object> objects_;
/// The flag of already generated
    std::vector<bool> already_generated_;
/// Optional Function used to initialize objects_ during init().
    std::shared_ptr<Function<Object>> initial_function_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<MultipleControlParameter>(*this);
    }
};

/**
@brief MultipleControlParameter template specialization for non-arithmetic type.
@tparam T The type of the object is the non-arithmetic type.

This template specialization does NOT have the member configuration
named "initial_value"

@par The configuration
MultipleControlParameter template specialization has extra configurations:
- member
    - name: "number_of_objects"
    - value: <tt>unsigned int</tt>
.
See setup() for the details.

@sa MultipleControlParameter.
*/
template<typename T>
class MultipleControlParameter<T, std::enable_if_t<!std::is_arithmetic<T>::value>>
    : public ControlParameter<T>
{
public:

/// The type of the object.
    using Object = typename ControlParameter<T>::Object;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<MultipleControlParameter> clone() const
    {
        return std::dynamic_pointer_cast<MultipleControlParameter>(clone_impl());
    }
/**
@brief Set up the internal states.

If ::RealControlMultipleControlParameter is used and
has the following configuration:
- number_of_objects: 100
.
its configuration should be
- JSON configuration
@code
"ControlParameter" : {
    "classname" : "RealControlMultipleControlParameter",
    "number_of_objects" : 100
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        auto size = config.get_uint_value("number_of_objects");
        objects_.resize(size);
        already_generated_.resize(size, false);
    }

/**
@name object access
*/
///@{
    void save(const Object& object, std::size_t index) override
    {
        objects_.at(index) = object;
        already_generated_.at(index) = true;
    }

    Object load(std::size_t index) const override
    {
        return objects_.at(index);
    }
///@}

/**
@name object quantity
*/
///@{
    std::size_t number_of_objects() const override
    {
        return objects_.size();
    }
///@}

    bool is_already_generated(std::size_t index) const override
    {
        return already_generated_.at(index);
    }

    void reset_already_generated(std::size_t index) override
    {
        already_generated_.at(index) = false;
    }

    void reset_all_already_generated() override
    {
        std::fill(already_generated_.begin(), already_generated_.end(), false);
    }

private:

/// The list of stored objects.
    std::vector<Object> objects_;
/// The flag of already generated
    std::vector<bool> already_generated_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<MultipleControlParameter>(*this);
    }
};

/**
@brief IntegerMultipleControlParameter is the MultipleControlParameter that
controls the integer number.
*/
using IntegerMultipleControlParameter = MultipleControlParameter<int>;
/**
@brief RealMultipleControlParameter is the MultipleControlParameter that
controls the real number.
*/
using RealMultipleControlParameter = MultipleControlParameter<double>;
/**
@brief IntegerControlMultipleControlParameter is the MultipleControlParameter that
controls the ::IntegerFunction.
*/
using IntegerControlMultipleControlParameter = MultipleControlParameter<std::shared_ptr<IntegerFunction>>;
/**
@brief RealControlMultipleControlParameter is the MultipleControlParameter that
controls the ::RealFunction.
*/
using RealControlMultipleControlParameter = MultipleControlParameter<std::shared_ptr<RealFunction>>;

}

#endif // MULTIPLE_CONTROL_PARAMETER_H
