#include <any>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "cm/cu/SdeFControlUpdate.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Random.h"
#include "cm/cp/ControlParameter.h"
#include "cm/cf/func/Function.h"
#include "Population.h"

namespace adef {

void SdeFControlUpdate::setup(const Configuration& /*config*/,
                              const PrototypeManager& /*pm*/)
{
}

void SdeFControlUpdate::update(std::shared_ptr<Repository> repos,
    std::shared_ptr<BaseControlParameter> parameter,
    std::shared_ptr<BaseFunction> function) const
{
    auto param = std::dynamic_pointer_cast<ControlParameter<Object>>(parameter);
    auto func = std::dynamic_pointer_cast<Function<Object>>(function);

    auto number_param = func->number_of_parameters();
    auto indices = repos->random()->sample_exclusive_indices(
        number_param, 0, repos->population()->population_size());

    std::vector<std::any> record;
    record.reserve(indices.size());
    for (auto index : indices) {
        record.push_back(param->load(index));
    }
    auto succ = func->record(record);
    if (!succ) {
        throw std::runtime_error(
            "No functions accept parameters "
            "in the \"" + function->function_name() + "\"");
    }
}

} // namespace adef
