#ifndef SDE_F_CONTROL_UPDATE_H
#define SDE_F_CONTROL_UPDATE_H

#include <memory>
#include <vector>
#include "ControlUpdate.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;

/**
@brief SdeFControlUpdate updates internal states by
the procedure of F value in the SDE algorithm.

@par The configuration
SdeFControlUpdate has no extra configurations.@n
See setup() for the details.
*/
class SdeFControlUpdate : public ControlUpdate
{
public:

/// The type of the object
    using Object = double;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<SdeFControlUpdate> clone() const
    {
        return std::dynamic_pointer_cast<SdeFControlUpdate>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is SdeFControlUpdate,@n
its configuration should be
- JSON configuration
@code
"SomeThing": {
    "classname" : "SdeFControlUpdate"
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override;

    void update(std::shared_ptr<Repository> repos,
        std::shared_ptr<BaseControlParameter> parameter,
        std::shared_ptr<BaseFunction> function) const override;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<SdeFControlUpdate>(*this);
    }
};

}

#endif // SDE_F_CONTROL_UPDATE_H
