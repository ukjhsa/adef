#include <memory>
#include <vector>
#include "DECrossover.h"
#include "DEVariantZ.h"
#include "cm/ControlMechanism.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Population.h"
#include "Individual.h"
#include "Parameters.h"
#include "Statistics.h"

namespace adef {

DECrossover::DECrossover(const DECrossover& rhs)
{
    if (rhs.variant_z_) { variant_z_ = rhs.variant_z_->clone(); }
    if (rhs.cr_) { cr_ = rhs.cr_->clone(); }
}

void DECrossover::setup(const Configuration& config, const PrototypeManager& pm)
{
    variant_z_ = make_and_setup_type<DEVariantZ>("DEVariantZ", config, pm);
    cr_ = make_and_setup_type<ControlMechanism<double>>("CR", config, pm);
}

void DECrossover::init(std::shared_ptr<Repository> repos)
{
    variant_z_->init(repos);
    repos->parameters()->store("cr", cr_);
    if (cr_) { cr_->init(repos); }
}

void DECrossover::crossover(std::shared_ptr<Repository> repos) const
{
    auto scope = repos->enter_read_only_population_scope();
    auto cr = std::dynamic_pointer_cast<ControlMechanism<double>>(cr_);
    std::vector<double> stat_CR;

    auto target = repos->population();
    auto trial = repos->offspring();
    std::size_t pop_size = target->population_size();

    for (std::size_t idx = 0; idx < pop_size; ++idx) {

        repos->parameters()->store("target_index", idx);

        cr->update(repos);
        auto crossover_rate = cr->generate(repos);

        stat_CR.push_back(crossover_rate);

        *trial->at(idx) = *crossover(trial->at(idx),
                                     target->at(idx),
                                     crossover_rate);
        // Here can NOT call cr->select(repos) because offspring is incomplete
        // see DEEnvironmentalSelection::select(repos)
    }
    repos->statistics()->store_statistics_all_individuals(Statistics::Item::cr, stat_CR);
}

std::shared_ptr<Individual> DECrossover::crossover(
                                std::shared_ptr<const Individual> mutant_vector,
                                std::shared_ptr<const Individual> target_vector,
                                double crossover_rate) const
{
    return variant_z_->crossover(mutant_vector, target_vector, crossover_rate);
}

}
