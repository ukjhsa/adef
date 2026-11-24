#ifndef BASE_CONTROL_MECHANISM_H
#define BASE_CONTROL_MECHANISM_H

#include <memory>
#include "Prototype.h"

namespace adef {

class Repository;

/**
@brief BaseControlMechanism defines interfaces of all ControlMechanism.
*/
class BaseControlMechanism : public Prototype
{
public:

    virtual ~BaseControlMechanism() = default;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<BaseControlMechanism> clone() const
    {
        return std::dynamic_pointer_cast<BaseControlMechanism>(clone_impl());
    }

/**
@brief Initialize this state from other states.
@param repos The Repository to get initialization informations.
*/
    virtual void init(std::shared_ptr<Repository> repos) = 0;
/**
@brief Update internal states by the relation between parent and offspring.
*/
    virtual void select(std::shared_ptr<Repository> repos) = 0;
/**
@brief Update the internal states by the current state.
*/
    virtual void update(std::shared_ptr<Repository> repos) = 0;
/**
@brief Perform generation-level updates before the per-individual loop.
*/
    virtual void update_generation(std::shared_ptr<Repository> /*repos*/)
    {
    }
/**
@brief Perform individual-level updates inside the per-individual loop.

Defaults to forwarding to update() for backward compatibility with custom
subclasses that do not override the two-tier lifecycle.
*/
    virtual void update_individual(std::shared_ptr<Repository> repos)
    {
        update(repos);
    }

private:

    virtual std::shared_ptr<Prototype> clone_impl() const override = 0;
};

}

#endif // BASE_CONTROL_MECHANISM_H
