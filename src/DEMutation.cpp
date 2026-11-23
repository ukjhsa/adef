#include <memory>
#include <vector>
#include <cstdlib>
#include "DEMutation.h"
#include "DEVariantX.h"
#include "DEVariantY.h"
#include "cm/ControlMechanism.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Individual.h"
#include "Population.h"
#include "Statistics.h"
#include "Random.h"

namespace adef {

DEMutation::DEMutation(const DEMutation& rhs)
{
    if (rhs.variant_x_) { variant_x_ = rhs.variant_x_->clone(); }
    if (rhs.variant_y_) { variant_y_ = rhs.variant_y_->clone(); }
    if (rhs.f_) { f_ = rhs.f_->clone(); }
}

void DEMutation::setup(const Configuration& config,
                       const PrototypeManager& pm)
{
    variant_x_ = make_and_setup_type<DEVariantX>("DEVariantX", config, pm);
    variant_y_ = make_and_setup_type<DEVariantY>("DEVariantY", config, pm);
    f_ = make_and_setup_type<ControlMechanism<double>>("F", config, pm);
}

void DEMutation::init(std::shared_ptr<Repository> repos)
{
    repos->parameters()->store("f", f_);
    if (f_) { f_->init(repos); }

    random_ = repos->random();
}

void DEMutation::mutate(std::shared_ptr<Repository> repos) const
{
    auto f = std::dynamic_pointer_cast<ControlMechanism<double>>(f_);
    std::vector<double> stat_F;

    auto parent = repos->population();
    auto children = repos->offspring();

    std::size_t pop_size = parent->population_size();
    for (std::size_t target_index = 0; target_index < pop_size; target_index++) {

        repos->parameters()->store("target_index", target_index);

        std::vector<std::size_t> used_indexes{target_index};

        auto rnd_indexes = exclusive_random_number_generator(
                                used_indexes, variant_x_->number_of_indexes(),
                                0, parent->population_size());
        auto base_vector = variant_x_->base_vector(
                                            target_index, rnd_indexes, parent);

        rnd_indexes = exclusive_random_number_generator(
                                used_indexes, variant_y_->number_of_indexes(),
                                0, parent->population_size());
        auto difference_vector = variant_y_->difference_vector(rnd_indexes,
                                                               parent);

        f->update(repos);
        auto scaling_factor = f->generate(repos);

        stat_F.push_back(scaling_factor);

        *children->at(target_index) = *mutate(base_vector,
                                              scaling_factor,
                                              difference_vector);
        // Here can NOT call f->select(repos) because offspring is incomplete
        // see DEEnvironmentalSelection::select(repos)
    }
    repos->statistics()->store_statistics_all_individuals(Statistics::Item::f, stat_F);
}

std::shared_ptr<Individual> DEMutation::mutate(
        std::shared_ptr<const Individual> base_vector,
        double F,
        const std::vector<std::shared_ptr<Individual>>& difference_vector) const
{
    auto mutant_vector = base_vector->clone();
    for (auto& diff : difference_vector) {
        *mutant_vector += (F * *diff);
    }
    return mutant_vector;
}

std::vector<std::size_t> DEMutation::exclusive_random_number_generator(
                                       std::vector<std::size_t>& used_numbers,
                                       unsigned int number_of_result,
                                       unsigned int min_range,
                                       unsigned int max_range) const
{
    std::vector<std::size_t> result(number_of_result);
    for (auto& res : result) {

        std::size_t rnd = 0;
        for (std::size_t counter = 0; counter < used_numbers.size(); ) {
            rnd = random_->random() % (max_range - min_range) + min_range;
            for (auto used : used_numbers) {
                if (rnd == used) {
                    counter = 0;
                    break;
                }
                ++counter;
            }
        }
        res = rnd;
        used_numbers.push_back(rnd);
    }

    return result;
}

}
