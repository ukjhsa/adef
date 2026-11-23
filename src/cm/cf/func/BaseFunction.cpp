#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include "cm/cf/func/BaseFunction.h"
#include "Repository.h"
#include "Random.h"

namespace adef {

BaseFunction::BaseFunction() : name_("root"), functions_()
{
}

BaseFunction::BaseFunction(const BaseFunction & rhs) : Prototype(rhs), name_(rhs.name_)
{
    functions_.reserve(rhs.functions_.size());
    for (auto&& func : rhs.functions_) {
        if (func) { functions_.push_back(func->clone()); }
        else { functions_.push_back(nullptr); }
    }
}

void BaseFunction::init(std::shared_ptr<Repository> repos)
{
    random_ = repos->random();
    for (auto& func : functions_) {
        if (func) {
            func->init(repos);
        }
    }
}

void BaseFunction::add_function(BaseFunctionPtr function)
{
    functions_.push_back(function);
}

BaseFunction::BaseFunctionPtr BaseFunction::get_function(const std::string & name) const
{
    auto it = std::find_if(
        functions_.begin(),
        functions_.end(),
        [&](const BaseFunctionPtr& target) {
        if (target->function_name() == name) { return true; }
        else { return false; }
    });
    if (it != functions_.end()) {
        return *it;
    }
    else {
        return nullptr;
    }
}

BaseFunction::BaseFunctionPtrList BaseFunction::get_all_functions() const
{
    return functions_;
}

}
