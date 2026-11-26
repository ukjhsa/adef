#include <memory>
#include <stdexcept>
#include "cm/cu/AverageFitnessControlUpdate.h"
#include "cm/cp/BaseControlParameter.h"
#include "cm/cf/func/BaseFunction.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"

namespace adef {

void AverageFitnessControlUpdate::setup(const Configuration& /*config*/,
                                        const PrototypeManager& /*pm*/)
{
}

void AverageFitnessControlUpdate::update(std::shared_ptr<Repository> repos,
    std::shared_ptr<BaseControlParameter> /*parameter*/,
    std::shared_ptr<BaseFunction> function) const
{
    const double average_fitness = repos->population_fitness_summary().average;
    auto succ = function->record({ average_fitness }, "average");
    if (!succ) {
        throw std::runtime_error(
            "No functions accept parameters \"average\" "
            "in the \"" + function->function_name() + "\"");
    }
}

}
