#ifndef CONTROL_MECHANISM_H
#define CONTROL_MECHANISM_H

#include <cstddef>
#include <vector>
#include "BaseControlMechanism.h"
#include "cp/ControlParameter.h"
#include "cf/ControlFunction.h"
#include "cr/ControlRange.h"
#include "cs/ControlSelection.h"
#include "cu/ControlUpdate.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Parameters.h"
#include "cf/func/BaseFunction.h"
#include "cf/func/Function.h"

namespace adef {

/**
@brief ControlMechanism represents the mechanism of adjusting the object.
@tparam T The type of the object.

ControlMechanism defines interfaces to adjust the object:
- generate(): generate the new object by the given mechanism.
- select(): update internal states by the relation between parent and offspring.
- update(): update internal states by the current state.

@par Requirement
Take out parameters from Parameters:
- type @c std::size_t and name "target_index" in
generate(), select() and update().
.
For more parameters, see each member classes.

@par The configuration
ControlMechanism has extra configurations:
- member
    - name: "ControlRange"
    - value: object configurations which represents ControlRange.
- member
    - name: "ControlParameter"
    - value: object configurations which represents ControlParameter.
- member
    - name: "ControlFunction"
    - value: object configurations which represents ControlFunction.
- member
    - name: "ControlSelection"
    - value: object configurations which represents ControlSelection.
- member
    - name: "ControlUpdate"
    - value: array configurations
        - element: object configurations which represents ControlUpdate.
.
See setup() for the details.
*/
template<typename T>
class ControlMechanism : public BaseControlMechanism
{
public:

/// The type of the object.
    using Object = typename ControlledObject<T>::Object;

    ControlMechanism() = default;
    ControlMechanism(const ControlMechanism& rhs) : BaseControlMechanism(rhs)
    {
        if (rhs.range_) { range_ = rhs.range_->clone(); }
        if (rhs.parameter_) { parameter_ = rhs.parameter_->clone(); }
        if (rhs.function_) { function_ = rhs.function_->clone(); }
        if (rhs.selection_) { selection_ = rhs.selection_->clone(); }

        updates_.reserve(rhs.updates_.size());
        for (auto&& update : rhs.updates_) {
            if (update) { updates_.push_back(update->clone()); }
            else { updates_.push_back(nullptr); }
        }
    }
    virtual ~ControlMechanism() = default;
    ControlMechanism& operator=(const ControlMechanism& rhs) = delete;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<ControlMechanism> clone() const
    {
        return std::dynamic_pointer_cast<ControlMechanism>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the ::RealControlMechanism and
has the following configuration:
- ControlRange: ::RealControlRange
- ControlParameter: ::RealMultipleControlParameter
- ControlFunction: SingleControlFunction
- ControlSelection: NonInfoControlSelection
- ControlUpdate: GenerationControlUpdate
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealControlMechanism",
    "ControlRange" : {
        "classname" : "RealControlRange",
        ...skip...
    },
    "ControlParameter" : {
        "classname" : "RealMultipleControlParameter",
        ...skip...
    },
    "ControlFunction" : {
        "classname" : "SingleControlFunction",
        "Function" : {
            ...skip...
        }
    },
    "ControlSelection" : {
        "classname" : "NonInfoControlSelection"
    },
    "ControlUpdate" : [
        {
            "classname" : "GenerationControlUpdate"
        }
    ]
}
@endcode
.
*/
    virtual void setup(const Configuration& config,
                       const PrototypeManager& pm) override
    {
        range_ = make_and_setup_type<ControlRange<Object>>("ControlRange",
                                                           config, pm);
        parameter_ = make_and_setup_type<ControlParameter<Object>>(
                                                            "ControlParameter",
                                                            config, pm);
        function_ = make_and_setup_type<ControlFunction>("ControlFunction",
                                                         config, pm);
        selection_ = make_and_setup_type<ControlSelection>("ControlSelection",
                                                           config, pm);

        auto updates_config = config.get_config("ControlUpdate");
        auto updates_size = updates_config.get_array_size();
        updates_.reserve(updates_size);
        for (decltype(updates_size) idx = 0; idx < updates_size; ++idx) {
            auto update_config = updates_config.get_config(idx);
            auto update = make_and_setup_type<ControlUpdate>(update_config, pm);
            updates_.push_back(update);
        }
    }
/**
@brief Initialize this state from other states.
@param repos The Repository to get initialization informations.
*/
    virtual void init(std::shared_ptr<Repository> repos) override
    {
        if (parameter_) { parameter_->init(repos); }
        if (function_) { function_->init(repos); }
    }
/**
@brief Generate the new object by the given mechanism.
*/
    virtual Object generate(std::shared_ptr<Repository> repos)
    {
        auto index = repos->parameters()->take_out<std::size_t>("target_index");

        if (parameter_->is_already_generated(index))
        {
            return parameter_->load(index);
        }
        else {
            auto func = std::dynamic_pointer_cast<Function<Object>>(function_->at(index));
            auto object = func->generate();
            while (!range_->is_valid(object)) {
                object = func->generate();
            }
            parameter_->save(object, index);
            return object;
        }
    }
/**
@brief Update internal states by the relation between parent and offspring.
*/
    virtual void select(std::shared_ptr<Repository> repos) override
    {
        auto index = repos->parameters()->take_out<std::size_t>("target_index");
        selection_->select(repos, parameter_, function_->at(index));
    }
/**
@brief Update the internal states by the current state.
*/
    virtual void update(std::shared_ptr<Repository> repos) override
    {
        auto index = repos->parameters()->take_out<std::size_t>("target_index");
        for (auto& update : updates_) {
            update->update(repos, parameter_, function_->at(index));
        }

        function_->at(index)->update();
        parameter_->reset_already_generated(index);
    }

protected:

/// The type of pointer to the ControlRange.
    using CRPtr = std::shared_ptr<ControlRange<Object>>;
/// The range.
    CRPtr range_;

/// The type of pointer to the ControlParameter.
    using CPPtr = std::shared_ptr<ControlParameter<Object>>;
/// The parameter.
    CPPtr parameter_;

/// The type of pointer to the ControlFunction.
    using CFPtr = std::shared_ptr<ControlFunction>;
/// The function.
    CFPtr function_;

/// The type of pointer to the ControlSelection.
    using CSPtr = std::shared_ptr<ControlSelection>;
/// The function.
    CSPtr selection_;

/// The type of pointer to the ControlUpdate.
    using CUPtr = std::shared_ptr<ControlUpdate>;
/// The function.
    std::vector<CUPtr> updates_;

private:

    virtual std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<ControlMechanism>(*this);
    }
};

/**
@brief IntegerControlMechanism is the ControlMechanism that
controls the integer number.
*/
using IntegerControlMechanism = ControlMechanism<int>;
/**
@brief RealControlMechanism is the ControlMechanism that
controls the real number.
*/
using RealControlMechanism = ControlMechanism<double>;
/**
@brief IntegerControlControlMechanism is the ControlMechanism that
controls the ::IntegerFunction.
*/
using IntegerControlControlMechanism = ControlMechanism<std::shared_ptr<IntegerFunction>>;
/**
@brief RealControlControlMechanism is the ControlMechanism that
controls the ::RealFunction.
*/
using RealControlControlMechanism = ControlMechanism<std::shared_ptr<RealFunction>>;

}

#endif
