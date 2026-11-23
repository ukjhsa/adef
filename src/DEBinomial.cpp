#include <memory>
#include <cstddef>
#include <random>
#include "DEBinomial.h"
#include "Random.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Individual.h"

namespace adef {

void DEBinomial::setup(const Configuration& config, const PrototypeManager& pm)
{
}

void DEBinomial::init(std::shared_ptr<Repository> repos)
{
    random_ = repos->random();
}

std::shared_ptr<Individual> DEBinomial::crossover(
                                std::shared_ptr<const Individual> mutant_vector,
                                std::shared_ptr<const Individual> target_vector,
                                double crossover_rate) const
{
    auto trial = target_vector->clone();

    std::size_t ind_size = mutant_vector->dimension_of_variable();
    if (ind_size == 0) {
        return trial;
    }
    std::size_t rnd = random_->uniform_index(ind_size);
    std::uniform_real_distribution<> uniform(0, 1);
    for (std::size_t idx = 0; idx < ind_size; ++idx) {
        if ((random_->generate(uniform) < crossover_rate) || (rnd == idx)) {
            trial->variables(idx) = mutant_vector->variables(idx);
        }
    }
    return trial;
}

}
