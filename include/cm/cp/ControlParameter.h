#ifndef CONTROL_PARAMETER_H
#define CONTROL_PARAMETER_H

#include <memory>
#include <cstddef>
#include "BaseControlParameter.h"
#include "cm/ControlledObject.h"

namespace adef {

class Configuration;
class PrototypeManager;

/**
@brief ControlParameter is the abstract base class of object storage.
@tparam T The type of the object.

ControlParameter stores objects that are generated from Function.

ControlParameter defines the following interfaces:
- save(): store the object.
- load(): load the object.
- number_of_objects(): return the number of objects.
- is_already_generated(): return the flag of already generated.
- reset_already_generated(): reset the flag of already generated.

The flag of already generated is used to inform the ControlMechanism
can generate new object from Function.

Now ADEF support many kinds of ControlParameter:
- SingleControlParameter
    - ::IntegerSingleControlParameter
    - ::RealSingleControlParameter
    - ::IntegerControlSingleControlParameter
    - ::RealControlSingleControlParameter
- MultipleControlParameter
    - ::IntegerMultipleControlParameter
    - ::RealMultipleControlParameter
    - ::IntegerControlMultipleControlParameter
    - ::RealControlMultipleControlParameter

@par The configuration
ControlParameter has no extra configurations.@n
See setup() for the details.
*/
template<typename T>
class ControlParameter : public BaseControlParameter
{
public:

/// The type of the object.
    using Object = typename ControlledObject<T>::Object;

    virtual ~ControlParameter() = default;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<ControlParameter> clone() const
    {
        return std::dynamic_pointer_cast<ControlParameter>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em Class is derived from ControlParameter, its configuration should be
- JSON configuration
@code
"ControlParameter": {
    "classname" : "Class"
}
@endcode
.
*/
    virtual void setup(const Configuration& config,
                       const PrototypeManager& pm) override = 0;
/**
@name object access
*/
///@{
/**
@brief Save the object at position @a index.
@param object The object to be stored.
@param index The index of object to be stored. It has default value 0.
*/
    virtual void save(const Object& object, std::size_t index) = 0;
/**
@brief Load the object at position @a index.
@param index The index of object to be loaded. It has default value 0.
*/
    virtual Object load(std::size_t index) const = 0;
///@}

/**
@name object quantity
*/
///@{
/**
@brief Return the number of objects in storage.
*/
    virtual std::size_t number_of_objects() const = 0;
///@}

/**
@brief Return the flag of already generated.

Use this flag to check whether the object is already generated
with response to the @a index.
*/
    virtual bool is_already_generated(std::size_t index) const = 0;
/**
@brief Reset the flag of already generated.

Call this function to inform the ControlMechanism can generate new object from
Function with response to the @a index.
*/
    virtual void reset_already_generated(std::size_t index) = 0;
/**
@brief Reset the already-generated flags for all managed objects.
*/
    virtual void reset_all_already_generated()
    {
        const auto count = number_of_objects();
        for (std::size_t idx = 0; idx < count; ++idx) {
            reset_already_generated(idx);
        }
    }

private:

    virtual std::shared_ptr<Prototype> clone_impl() const override = 0;
};

}

#endif // CONTROL_PARAMETER_H
