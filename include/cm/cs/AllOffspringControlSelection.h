#ifndef ALL_OFFSPRING_CONTROL_SELECTION_H
#define ALL_OFFSPRING_CONTROL_SELECTION_H

#include <memory>
#include <cstddef>
#include <string>
#include <stdexcept>
#include "ControlSelection.h"
#include "cm/ControlledObject.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Parameters.h"
#include "cm/cp/BaseControlParameter.h"
#include "cm/cp/ControlParameter.h"
#include "cm/cf/func/BaseFunction.h"
#include "cm/cf/func/Function.h"
#include "Population.h"

namespace adef {

/**
@brief AllOffspringControlSelection updates internal states by recording every
parent/offspring trial outcome, regardless of whether offspring improves over
parent.

This allows downstream scoring functions such as SuccessScoringFunction to
observe both successful (improved) and failed (non-improved) trials.

@par Requirement
Take out parameters from Parameters:
- type @c std::size_t and name "target_index"
.
record parameters into Function:
- name "object"
.

@par The configuration
AllOffspringControlSelection has no extra configurations.@n
See setup() for the details.
*/
template<typename T>
class AllOffspringControlSelection : public ControlSelection
{
public:

/// The type of the object
    using Object = typename ControlledObject<T>::Object;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<AllOffspringControlSelection> clone() const
    {
        return std::dynamic_pointer_cast<AllOffspringControlSelection>(clone_impl());
    }
/**
@brief Set up the internal states.

If ::RealControlAllOffspringControlSelection is used,@n
its configuration should be
- JSON configuration
@code
"ControlSelection": {
    "classname" : "RealControlAllOffspringControlSelection"
}
@endcode
.
*/
    void setup(const Configuration& /*config*/, const PrototypeManager& /*pm*/) override
    {
    }

    void select(std::shared_ptr<Repository> repos,
                std::shared_ptr<BaseControlParameter> parameter,
                std::shared_ptr<BaseFunction> function) const override
    {
        if (!repos || !repos->parameters() || !repos->population() || !repos->offspring()) {
            throw std::invalid_argument(
                "AllOffspringControlSelection received an incomplete Repository");
        }

        auto param = std::dynamic_pointer_cast<ControlParameter<Object>>(parameter);
        if (!param) {
            throw std::runtime_error(
                "AllOffspringControlSelection received an incompatible ControlParameter");
        }
        if (!function) {
            throw std::runtime_error(
                "AllOffspringControlSelection received a null BaseFunction");
        }

        auto index = repos->parameters()->take_out<std::size_t>("target_index");
        auto parent = repos->population()->at(index);
        auto offspring = repos->offspring()->at(index);

        auto succ = function->record({param->load(index)},
                                     parent, offspring, "object");
        if (!succ) {
            throw std::runtime_error(
                     "No functions accept parameters \"object\" "
                     "in the \"" + function->function_name() + "\"");
        }
    }

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<AllOffspringControlSelection>(*this);
    }
};

/**
@brief IntegerAllOffspringControlSelection is the AllOffspringControlSelection that
controls the integer number.
*/
using IntegerAllOffspringControlSelection = AllOffspringControlSelection<int>;
/**
@brief RealAllOffspringControlSelection is the AllOffspringControlSelection that
controls the real number.
*/
using RealAllOffspringControlSelection = AllOffspringControlSelection<double>;
/**
@brief IntegerControlAllOffspringControlSelection is the AllOffspringControlSelection that
controls the ::IntegerFunction.
*/
using IntegerControlAllOffspringControlSelection = AllOffspringControlSelection<std::shared_ptr<IntegerFunction>>;
/**
@brief RealControlAllOffspringControlSelection is the AllOffspringControlSelection that
controls the ::RealFunction.
*/
using RealControlAllOffspringControlSelection = AllOffspringControlSelection<std::shared_ptr<RealFunction>>;

}

#endif // ALL_OFFSPRING_CONTROL_SELECTION_H
