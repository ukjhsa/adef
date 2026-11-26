#include <memory>
#include <vector>
#include <map>
#include <string>
#include <limits>
#include <cmath>
#include <iterator>
#include <algorithm>
#include <fstream>
#include <ostream>
#include "Statistics.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Population.h"
#include "Problem.h"
#include "Individual.h"

namespace adef {

Statistics::Statistics() :
    success_error_(0.00001),
    best_fitness_so_far_(std::numeric_limits<double>::max()),
    number_of_fitness_evaluations_(0)
{
    map_item_to_name_[Item::parents] = "parents";
    map_item_to_name_[Item::offspring] = "offspring";
    map_item_to_name_[Item::cr] = "cr";
    map_item_to_name_[Item::f] = "f";
}

void Statistics::setup(const Configuration& config, const PrototypeManager& pm)
{
    (void)pm;
    auto success_error_config = config.get_config("success_error");
    success_error_ = success_error_config.is_null() ?
                        0.00001 : success_error_config.get_double_value();

    names_of_items_.clear();
    auto item_config = config.get_config("item");
    auto item_config_size = item_config.get_array_size();
    for (decltype(item_config_size) idx = 0; idx < item_config_size; ++idx) {
        auto name = item_config.get_string_value(idx);
        names_of_items_.push_back(name);
    }

    statistics_all_individuals_.clear();
    best_fitness_.clear();
    best_fitness_so_far_ = std::numeric_limits<double>::max();
    number_of_fitness_evaluations_ = 0;
}

void Statistics::init(std::shared_ptr<Repository> repos)
{
    (void)repos;
    statistics_all_individuals_.clear();
    best_fitness_.clear();
    best_fitness_so_far_ = std::numeric_limits<double>::max();
    number_of_fitness_evaluations_ = 0;
}

bool Statistics::is_evolve_success(std::shared_ptr<const Repository> repos) const
{
    auto optimal = repos->problem()->optimal_solution();
    if (best_fitness_so_far_ < optimal ||
        std::abs(best_fitness_so_far_-optimal) < success_error_) {
        return true;
    }
    else {
        return false;
    }
}

double Statistics::evolve_error(std::shared_ptr<const Repository> repos) const
{
    if (is_evolve_success(repos)) {
        return 0;
    }
    else {
        return best_fitness_so_far_ - repos->problem()->optimal_solution();
    }
}

void Statistics::store_statistics_all_individuals(
                                         Item item,
                                         const AllIndividualsValueList& values)
{
    // get the name of the item
    std::string name = map_item_to_name_.at(item);
    // check whether the name of the item is in the statistical items
    auto result = std::find(std::begin(names_of_items_),
                            std::end(names_of_items_),
                            name);
    // if found, store it
    if (result != std::end(names_of_items_)) {
        auto search = statistics_all_individuals_.find(item);
        if (search != statistics_all_individuals_.end()) {
            search->second.push_back(values);
        }
        else {
            statistics_all_individuals_[item].push_back(values);
        }
    }
}

void Statistics::output_statistics(const std::string& prefix) const
{
    output_best_fitness(prefix);
    output_all_statistical_items(prefix);
}

void Statistics::output_all_statistical_items(const std::string& prefix) const
{
    for (auto& name : names_of_items_) {
        // find key (item) with specific element (name)
        auto result = std::find_if(std::begin(map_item_to_name_),
                                   std::end(map_item_to_name_),
                                   [&name](const auto &pair) {
                                        return pair.second == name;
                                   });
        // it should always be found
        if (result != std::end(map_item_to_name_)) {
            output_statistical_items(prefix + "_" + name + ".csv",
                        statistics_all_individuals_.at(result->first));
        }
    }
}

void Statistics::output_statistical_items(const std::string& file_name,
                    const GenerationList<AllIndividualsValueList>& data) const
{
    std::ofstream ofs(file_name);
    ofs << "Gen,";
    auto pop_size = data.front().size();
    for (decltype(pop_size) idx = 0; idx < pop_size; ++idx) {
        ofs << "ind" << idx+1 << ",";
    }
    ofs << std::endl;

    auto generation = data.size();
    for (decltype(generation) gen = 0; gen < generation; ++gen) {
        ofs << gen+1 << ",";

        for (decltype(pop_size) idx = 0; idx < pop_size; ++idx) {
            ofs << data.at(gen).at(idx) << ",";
        }
        ofs << std::endl;
    }
}

void Statistics::output_best_fitness(const std::string& prefix) const
{
    std::ofstream ofs(prefix + "_" + "best_fitness" + ".csv");
    ofs << "Gen,best_fitness" << std::endl;

    for (std::size_t gen = 0; gen < best_fitness_.size(); ++gen) {
        ofs << gen+1 << "," << best_fitness_.at(gen) << std::endl;
    }
}

void Statistics::post_reproduction_statistic(std::shared_ptr<const Repository> repos)
{
    take_statistics_population(Item::parents, repos->population());
    take_statistics_population(Item::offspring, repos->offspring());
}

void Statistics::take_statistics(std::shared_ptr<const Repository> repos)
{
    auto pop = repos->population();
    auto pop_size = pop->population_size();

    double best = pop->at(0)->fitness();
    for (std::size_t idx = 1; idx < pop_size; ++idx) {

        auto individual_fitness = pop->at(idx)->fitness();
        if (individual_fitness < best) {
            best = individual_fitness;
        }
    }

    best_fitness_.push_back(best);
    update_best_fitness_so_far(best);
}

void Statistics::take_statistics_population(
                                 Item item,
                                 std::shared_ptr<const Population> population)
{
    auto pop_size = population->population_size();

    AllIndividualsValueList pop_fitness(pop_size);
    for (std::size_t idx = 0; idx < pop_size; ++idx) {

        auto individual_fitness = population->at(idx)->fitness();

        pop_fitness[idx] = individual_fitness;
    }
    store_statistics_all_individuals(item, pop_fitness);
}

void Statistics::update_best_fitness_so_far(double fitness)
{
    if (fitness < best_fitness_so_far_) {
        best_fitness_so_far_ = fitness;
    }
}

}
