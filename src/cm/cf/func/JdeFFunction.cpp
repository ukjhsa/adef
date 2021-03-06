#include <memory>
#include <vector>
#include <string>
#include <random>
#include "cm/cf/func/JdeFFunction.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Individual.h"

namespace adef {

void JdeFFunction::setup(const Configuration& config, const PrototypeManager& pm)
{
    auto object_config = config.get_config("object");
    auto object = make_and_setup_type<BaseFunction>(object_config, pm);
    object->set_function_name("object");
    add_function(object);

    auto lower_bound_config = config.get_config("lower_bound");
    auto lower_bound = make_and_setup_type<BaseFunction>(lower_bound_config, pm);
    lower_bound->set_function_name("lower_bound");
    add_function(lower_bound);

    auto upper_bound_config = config.get_config("upper_bound");
    auto upper_bound = make_and_setup_type<BaseFunction>(upper_bound_config, pm);
    upper_bound->set_function_name("upper_bound");
    add_function(upper_bound);

    auto tau_config = config.get_config("tau");
    auto tau = make_and_setup_type<BaseFunction>(tau_config, pm);
    tau->set_function_name("tau");
    add_function(tau);

    object_ = 0.5;
    lower_bound_ = 0.1;
    upper_bound_ = 0.9;
    tau_ = 0.1;
}

JdeFFunction::Object JdeFFunction::generate()
{
    std::uniform_real_distribution<> uniform;
    if (random_->generate(uniform) < tau_) {
        return lower_bound_ +
               random_->generate(uniform) * upper_bound_;
    }
    else {
        return object_;
    }
}

void JdeFFunction::update()
{
    auto object = get_function("object");
    object->update();
    object_ = object->generate();

    auto lower_bound = get_function("lower_bound");
    lower_bound->update();
    lower_bound_ = lower_bound->generate();

    auto upper_bound = get_function("upper_bound");
    upper_bound->update();
    upper_bound_ = upper_bound->generate();

    auto tau = get_function("tau");
    tau->update();
    tau_ = tau->generate();
}

unsigned int JdeFFunction::number_of_parameters() const
{
    return 0;
}

}
