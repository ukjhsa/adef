#ifndef DE_MUTATION_H
#define DE_MUTATION_H

#include <memory>
#include "Mutation.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;
class Individual;
class Random;
class DEVariantX;
class DEVariantY;
class BaseControlMechanism;

/**
@brief DEMutation mutate according to the DE algorithm.

mutate() calls ControlMechanism::update_generation(),
ControlMechanism::update_individual(), and ControlMechanism::generate() of
the parameter @em F.

@par The configuration
DEMutation has extra configurations:
- member
    - name: "DEVariantX"
    - value: object configurations which represents DEVariantX.
- member
    - name: "DEVariantY"
    - value: object configurations which represents DEVariantY.
- member
    - name: "F"
    - value: object configurations which represents ControlMechanism.
.
See setup() for the details.
*/
class DEMutation : public Mutation
{
public:

    DEMutation() = default;
    DEMutation(const DEMutation& rhs);
    DEMutation& operator=(const DEMutation& rhs) = delete;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<DEMutation> clone() const
    {
        return std::dynamic_pointer_cast<DEMutation>(clone_impl());
    }
/**
@brief Set up the internal states.

DEMutation's configuration should be
- JSON configuration
@code
"Mutation" : {
    "classname" : "DEMutation",
    "DEVariantX" : {
        ...skip...
    },
    "DEVariantY" : {
        ...skip...
    },
    "F" : {
        ...skip...
    }
}
@endcode
.
*/
    void setup(const Configuration& config,
               const PrototypeManager& pm) override;

    void init(std::shared_ptr<Repository> repos) override;

    void mutate(std::shared_ptr<Repository> repos) const override;

/**
@brief Mutate into a new individual in DE.
@param base_vector The base vector.
@param F The scaling factor.
@param difference_vector The difference vector.
@return The mutated vector.
*/
    std::shared_ptr<Individual> mutate(
        std::shared_ptr<const Individual> base_vector,
        double F,
        const std::vector<std::shared_ptr<Individual>>& difference_vector) const;

private:

    std::shared_ptr<Random> random_;

/**
@brief The DE variant @e x.
*/
    std::shared_ptr<DEVariantX> variant_x_;
/**
@brief The DE variant @e y.
*/
    std::shared_ptr<DEVariantY> variant_y_;
/**
@brief The scaling factor used in mutation.
*/
    std::shared_ptr<BaseControlMechanism> f_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<DEMutation>(*this);
    }
};

}

#endif
