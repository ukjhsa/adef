#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <any>
#include <stdexcept>
#include "cm/cu/SdeFControlUpdate.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Random.h"
#include "cm/cp/ControlParameter.h"
#include "cm/cf/func/Function.h"
#include "Population.h"

namespace adef {

void SdeFControlUpdate::setup(const Configuration& config, const PrototypeManager& pm)
{
}

void SdeFControlUpdate::update(std::shared_ptr<Repository> repos,
    std::shared_ptr<BaseControlParameter> parameter,
    std::shared_ptr<BaseFunction> function) const
{
    auto param = std::dynamic_pointer_cast<ControlParameter<Object>>(parameter);
    auto func = std::dynamic_pointer_cast<Function<Object>>(function);

    std::vector<std::size_t> temp;
    auto number_param = func->number_of_parameters();
    auto indices = exclusive_random_generator(temp,
        number_param,
        0,
        static_cast<unsigned int>(repos->population()->population_size()),
        repos->random());

    std::vector<std::any> record;
    for (auto& index : indices) {
        record.push_back(param->load(index));
    }
    auto succ = func->record(record);
    if (!succ) {
        throw std::runtime_error(
            "No functions accept parameters "
            "in the \"" + function->function_name() + "\"");
    }
}

std::vector<std::size_t> SdeFControlUpdate::exclusive_random_generator(
    std::vector<std::size_t>& used_numbers,
    unsigned int number_of_result,
    unsigned int min_range,
    unsigned int max_range,
    std::shared_ptr<Random> random) const
{
    if (number_of_result == 0) {
        return {};
    }
    if (max_range <= min_range) {
        throw std::invalid_argument(
            "Invalid range for SdeFControlUpdate::exclusive_random_generator.");
    }
    const std::size_t range_size = max_range - min_range;
    std::size_t used_in_range = 0;
    for (std::size_t idx = min_range; idx < max_range; ++idx) {
        if (std::find(used_numbers.begin(), used_numbers.end(), idx) != used_numbers.end()) {
            ++used_in_range;
        }
    }
    if (number_of_result > range_size - used_in_range) {
        throw std::invalid_argument(
            "Requested number of exclusive random indices exceeds available range.");
    }

    std::vector<std::size_t> result;
    result.reserve(number_of_result);
    for (unsigned int i = 0; i < number_of_result; ++i) {
        std::size_t rnd = 0;
        do {
            rnd = static_cast<std::size_t>(random->random()) % range_size + min_range;
        } while (std::find(used_numbers.begin(), used_numbers.end(), rnd) != used_numbers.end());

        result.push_back(rnd);
        used_numbers.push_back(rnd);
    }
    return result;
}

}
