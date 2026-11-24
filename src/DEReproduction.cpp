#include <memory>
#include "DEReproduction.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Repository.h"
#include "Mutation.h"
#include "Crossover.h"
#include "Repair.h"

namespace adef {

void DEReproduction::setup(const Configuration& config,
                           const PrototypeManager& pm)
{
}

void DEReproduction::init(std::shared_ptr<Repository> repos)
{
}

void DEReproduction::reproduce(std::shared_ptr<Repository> repos) const
{
    {
        auto scope = repos->enter_read_only_population_scope();
        // mutation
        repos->mutation()->mutate(repos);
        // crossover
        repos->crossover()->crossover(repos);
    }
    // repair
    repos->repair()->repair(repos);
}

}
