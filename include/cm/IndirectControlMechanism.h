#ifndef INDIRECT_CONTROL_MECHANISM_H
#define INDIRECT_CONTROL_MECHANISM_H

#include <cstddef>
#include <vector>
#include "ControlMechanism.h"
#include "cp/ControlParameter.h"
#include "cr/ControlRange.h"
#include "cs/ControlSelection.h"
#include "cu/ControlUpdate.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Parameters.h"
#include "cf/func/Function.h"

namespace adef {

/**
@brief IndirectControlMechanism adjusts the object by
contained ControlMechanism with Object type %Function.

IndirectControlMechanism has no ControlFunction. It is obtained by
contained ControlMechanism with Object type %Function.

The difference between this and ControlMechanism:
- generate(): the function is obtained by contained ControlMechanism.
- select(): in addition to function,
            it calls select() of contained ControlMechanism.
- update(): in addition to function,
            it calls update() of contained ControlMechanism.

@par Requirement
Take out parameters from Parameters:
- type @c std::size_t and name "target_index" in
generate(), and update().
.

Now ADEF support many kinds of IndirectControlMechanism:
- ::IntegerIndirectControlMechanism
- ::RealIndirectControlMechanism

@par The configuration
IndirectControlMechanism has configurations:
- member
    - name: "ControlRange"
    - value: object configurations which represents ControlRange.
- member
    - name: "ControlParameter"
    - value: object configurations which represents ControlParameter.
- member
    - name: "ControlSelection"
    - value: object configurations which represents ControlSelection.
- member
    - name: "ControlUpdate"
    - value: array configurations
        - element: object configurations which represents ControlUpdate.
- member
    - name: "ControlMechanism"
    - value: object configurations which represents ControlMechanism
             with Object type %Function.
.
See setup() for the details.
*/
template<typename T>
class IndirectControlMechanism : public ControlMechanism<T>
{
public:

/// The type of the object.
    using Object = typename ControlMechanism<T>::Object;

    IndirectControlMechanism() = default;
    IndirectControlMechanism(const IndirectControlMechanism& rhs) :
        ControlMechanism<T>(rhs), cm_(rhs.cm_ ? rhs.cm_->clone() : nullptr)
    {
    }

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<IndirectControlMechanism> clone() const
    {
        return std::dynamic_pointer_cast<IndirectControlMechanism>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the ::RealIndirectControlMechanism and
has the following configuration:
- ControlRange: ::RealControlRange
- ControlParameter: ::RealSingleControlParameter
- ControlSelection: NonInfoControlSelection
- ControlUpdate: GenerationControlUpdate
- ControlMechanism: ::RealControlControlMechanism
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealIndirectControlMechanism",
    "ControlRange" : {
        "classname" : "RealControlRange",
        ...skip...
    },
    "ControlParameter" : {
        "classname" : "RealSingleControlParameter",
        ...skip...
    },
    "ControlSelection" : {
        "classname" : "NonInfoControlSelection",
        ...skip...
    },
    "ControlUpdate" : [
        {
            "classname" : "GenerationControlUpdate",
            ...skip...
        }
    ],
    "ControlMechanism" : {
        "classname" : "RealControlControlMechanism",
        ...skip...
    }
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        range_ = make_and_setup_type<ControlRange<Object>>("ControlRange",
                                                           config, pm);
        parameter_ = make_and_setup_type<ControlParameter<Object>>(
                                                            "ControlParameter",
                                                            config, pm);
        // ControlFunction does NOT exist.
        // all operators related to function is obtained by cm.
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

        cm_ = make_and_setup_type<typename decltype(cm_)::element_type>(
                                        "ControlMechanism", config, pm);
    }

    void init(std::shared_ptr<Repository> repos) override
    {
        if (parameter_) { parameter_->init(repos); }
        if (cm_) { cm_->init(repos); }
    }

    Object generate(std::shared_ptr<Repository> repos) override
    {
        auto index = repos->parameters()->take_out<std::size_t>("target_index");

        if (parameter_->is_already_generated(index))
        {
            return parameter_->load(index);
        }
        else {
            auto func = cm_->generate(repos);
            auto object = func->generate();
            while (!range_->is_valid(object)) {
                object = func->generate();
            }
            parameter_->save(object, index);
            return object;
        }
    }

    void select(std::shared_ptr<Repository> repos) override
    {
        selection_->select(repos, parameter_, cm_->generate(repos));

        cm_->select(repos);
    }

    void update(std::shared_ptr<Repository> repos) override
    {
        if (cm_) {
            cm_->update(repos);
        }

        auto index = repos->parameters()->take_out<std::size_t>("target_index");
        auto leaf = cm_->generate(repos);
        for (auto& update : updates_) {
            if (update) {
                update->update(repos, parameter_, leaf);
            }
        }

        leaf->update();
        parameter_->reset_already_generated(index);
    }

    void update_generation(std::shared_ptr<Repository> repos) override
    {
        if (cm_) {
            cm_->update_generation(repos);
        }
        if (parameter_ && !this->has_individual_updates()) {
            parameter_->reset_all_already_generated();
        }
    }

    void update_individual(std::shared_ptr<Repository> repos) override
    {
        if (cm_) {
            cm_->update_individual(repos);
        }

        auto index = repos->parameters()->take_out<std::size_t>("target_index");
        const bool outer_has_individual = this->has_individual_updates();
        if (!outer_has_individual && parameter_ && parameter_->is_already_generated(index)) {
            return;
        }

        auto leaf = cm_->generate(repos);
        for (auto& update : updates_) {
            if (update) {
                update->update(repos, parameter_, leaf);
            }
        }

        leaf->update();
        if (outer_has_individual && parameter_) {
            parameter_->reset_already_generated(index);
        }
    }

private:

/// The component of type %Function<Object>
    std::shared_ptr<ControlMechanism<std::shared_ptr<Function<Object>>>> cm_;

    using ControlMechanism<T>::range_;
    using ControlMechanism<T>::parameter_;
    using ControlMechanism<T>::selection_;
    using ControlMechanism<T>::updates_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<IndirectControlMechanism>(*this);
    }
};

/**
@brief IntegerIndirectControlMechanism is the IndirectControlMechanism that
controls the integer number.
*/
using IntegerIndirectControlMechanism = IndirectControlMechanism<int>;
/**
@brief RealIndirectControlMechanism is the IndirectControlMechanism that
controls the real number.
*/
using RealIndirectControlMechanism = IndirectControlMechanism<double>;

}

#endif // INDIRECT_CONTROL_MECHANISM_H
