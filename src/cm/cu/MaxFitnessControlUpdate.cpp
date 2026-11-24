#include <memory>
#include <string>
#include <stdexcept>
#include "cm/cu/MaxFitnessControlUpdate.h"
#include "cm/cp/BaseControlParameter.h"
#include "cm/cf/func/BaseFunction.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"

namespace adef {

void MaxFitnessControlUpdate::setup(const Configuration& /*config*/,
                                    const PrototypeManager& /*pm*/)
{
}

void MaxFitnessControlUpdate::update(std::shared_ptr<Repository> repos,
                                     std::shared_ptr<BaseControlParameter> /*parameter*/,
                                     std::shared_ptr<BaseFunction> function) const
{
    const auto summary = repos->population_fitness_summary();
    if (summary.count == 0) {
        throw std::out_of_range("Population is empty");
    }
    auto succ = function->record({ summary.max }, "max");
    if (!succ) {
        throw std::runtime_error(
            "No functions accept parameters \"max\" "
            "in the \"" + function->function_name() + "\"");
    }
}

}
