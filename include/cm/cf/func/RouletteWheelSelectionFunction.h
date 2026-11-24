#ifndef ROULETTE_WHEEL_SELECTION_FUNCTION_H
#define ROULETTE_WHEEL_SELECTION_FUNCTION_H

#include <memory>
#include <vector>
#include <string>
#include <random>
#include <list>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <any>
#include <algorithm>
#include "ScoredFunction.h"
#include "Configuration.h"
#include "PrototypeManager.h"
#include "Random.h"
#include "Individual.h"

namespace adef {

/**
@brief RouletteWheelSelectionFunction generates the object according to
the method of roulette wheel selection.

RouletteWheelSelectionFunction has the following feature:
- generate(): select the object according to
              the method of roulette wheel selection.
- record(): change scores.
- update(): update scores.

ADEF supports many kinds of RouletteWheelSelectionFunction:
- ::IntegerRouletteWheelSelectionFunction.
- ::RealRouletteWheelSelectionFunction.
- ::IntegerControlRouletteWheelSelectionFunction.
- ::RealControlRouletteWheelSelectionFunction.

@par The configuration
RouletteWheelSelectionFunction has extra configurations:
- member
    - name: "score_size"
    - value: <tt>unsigned int</tt>
- member:
    - name: "object"
    - value: array configurations
        - value: number configurations or object configurations
                 depends on RouletteWheelSelectionFunction::Object.
.
See setup() for the details.
*/
template<typename T>
class RouletteWheelSelectionFunction : public ScoredFunction<T>
{
public:

/// @copydoc ScoredFunction::Object
    using Object = typename ScoredFunction<T>::Object;

/// @copydoc ScoredFunction::Score
    using Score = typename ScoredFunction<T>::Score;

/**
@brief The default constructor.

The default value of the maximum of score size is 0.
*/
    RouletteWheelSelectionFunction() :
        score_size_(0)
    {
    }
/**
@brief The copy constructor.
*/
    RouletteWheelSelectionFunction(const RouletteWheelSelectionFunction& rhs) :
        ScoredFunction<T>(rhs),
        score_size_(rhs.score_size_),
        valued_objects_(rhs.valued_objects_)
    {
    }

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<RouletteWheelSelectionFunction> clone() const
    {
        return std::dynamic_pointer_cast<RouletteWheelSelectionFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the ::RealRouletteWheelSelectionFunction and
has the following configuration:
- scoring_function: SuccessScoringFunction
- score_size: 50
- object: 0.3, 0.5
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealRouletteWheelSelectionFunction",
    "scoring_function" : {
        "classname" : "SuccessScoringFunction"
    },
    "score_size" : 50,
    "object" : [
        0.3,
        0.5
    ]
}
@endcode
.
If @em SomeThing is the ::RealControlRouletteWheelSelectionFunction and
has the following configuration:
- scoring_function: SuccessScoringFunction
- score_size: 50
- object: ::RealConstantFunction of value 0.5,
          ::RealConstantFunction of value 1.0
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "RealControlRouletteWheelSelectionFunction",
    "scoring_function" : {
        "classname" : "SuccessScoringFunction"
    },
    "score_size" : 50,
    "object" : [
        {
            "classname" : "RealConstantFunction",
            "value" : 0.5
        },
        {
            "classname" : "RealConstantFunction",
            "value" : 1.0
        }
    ]
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override
    {
        ScoredFunction<T>::setup(config, pm);

        score_size_ = config.get_uint_value("score_size");

        auto object_config = config.get_config("object");
        auto object_size = object_config.get_array_size();
        for (decltype(object_size) idx = 0; idx < object_size; ++idx) {

            auto inner_config = object_config.get_config(idx);
            auto object = CO::create(inner_config, pm);

            ValuedObject v;
            v.score_counter_ = 0;
            v.score_ = 0;
            v.object_ = object;
            valued_objects_.push_back(v);
        }
    }

    void init(std::shared_ptr<Repository> repos) override
    {
        BaseFunction::init(repos);
        if constexpr (is_shared_ptr<Object>::value) {
            if constexpr (std::is_convertible_v<typename Object::element_type*, BaseFunction*>) {
                for (auto& valued_object : valued_objects_) {
                    if (valued_object.object_.object) {
                        valued_object.object_.object->init(repos);
                    }
                }
            }
        }
    }

    Object generate() override
    {
        if (valued_objects_.empty()) {
            throw std::logic_error("RouletteWheelSelectionFunction has no objects.");
        }
        Score sum_score = 0;
        for (auto& valued_object : valued_objects_) {

            if (valued_object.score_ < std::numeric_limits<Score>::epsilon()) {
                valued_object.score_ = std::numeric_limits<Score>::epsilon();
            }
            sum_score += valued_object.score_;
        }

        // if sum of score is zero, randomly select
        if (std::abs(sum_score) < std::numeric_limits<Score>::epsilon()) {

            auto index = BaseFunction::random_->uniform_index(valued_objects_.size());
            return valued_objects_.at(index).object_.object;
        }
        else {

            std::uniform_real_distribution<Score> uniform(0, sum_score);
            Score rnd = BaseFunction::random_->generate(uniform);

            Score cumulative_weight = 0;
            for (auto& valued_object : valued_objects_) {

                cumulative_weight += valued_object.score_;
                if (rnd < cumulative_weight ||
                    std::abs(rnd-cumulative_weight) < std::numeric_limits<Score>::epsilon()) {

                    return valued_object.object_.object;
                }
            }
            throw std::logic_error("RouletteWheelSelectionFunction generate error.");
        }
    }

    bool record(const std::vector<std::any>& params,
                const std::string& name = "") override
    {
        return true;
    }

    bool record(const std::vector<std::any>& params,
                std::shared_ptr<const Individual> parent,
                std::shared_ptr<const Individual> offspring,
                const std::string& name = "") override
    {
        // only one param
        auto param = std::any_cast<Object>(params.front());
        auto result = std::find_if(valued_objects_.begin(),
                                   valued_objects_.end(),
                                   [&param](const ValuedObject& object) {
                                       return object.object_.object == param;
                                   });
        if (result == valued_objects_.end()) {
            throw std::logic_error("RouletteWheelSelectionFunction accept wrong record parameter.");
        }

        result->scores_.push_back(scoring_function_->calculate_score(parent, offspring));

        if (result->scores_.size() >= score_size_) {
            result->scores_.pop_front();
        }
        return true;
    }

    void update() override
    {
        for (auto& valued_object : valued_objects_) {

            // update score from score_memory
            valued_object.score_ = 0;
            for (auto& score : valued_object.scores_) {
                valued_object.score_ += score;
            }
            if (valued_object.scores_.size() > 0) {
                valued_object.score_ /= valued_object.scores_.size();
            }
            // clear
            valued_object.scores_.clear();
        }
    }

    unsigned int number_of_parameters() const override
    {
        return 1;
    }

private:

/// The size of score
    unsigned int score_size_;

/// The type of the encapsulated object.
    using CO = ControlledObject<Object>;
/**
@brief ValuedObject contains informations of the object including
its score, and scores.
*/
    struct ValuedObject {

/// The counter of the score.
        unsigned int score_counter_;
/// The list of scores of the object.
        std::list<Score> scores_;
/// The score of the object.
        Score score_;
/// The object.
        CO object_;
    };
/// The list of valued objects.
    std::vector<ValuedObject> valued_objects_;

/// @copydoc ScoredFunction::scoring_function_
    using ScoredFunction<T>::scoring_function_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<RouletteWheelSelectionFunction>(*this);
    }
};

/**
@brief IntegerRouletteWheelSelectionFunction is the
RouletteWheelSelectionFunction that controls the integer number.
*/
using IntegerRouletteWheelSelectionFunction = RouletteWheelSelectionFunction<int>;
/**
@brief RealRouletteWheelSelectionFunction is the
RouletteWheelSelectionFunction that controls the real number.
*/
using RealRouletteWheelSelectionFunction = RouletteWheelSelectionFunction<double>;
/**
@brief IntegerControlRouletteWheelSelectionFunction is the
RouletteWheelSelectionFunction that controls the ::IntegerFunction.
*/
using IntegerControlRouletteWheelSelectionFunction = RouletteWheelSelectionFunction<std::shared_ptr<IntegerFunction>>;
/**
@brief RealControlRouletteWheelSelectionFunction is the
RouletteWheelSelectionFunction that controls the ::RealFunction.
*/
using RealControlRouletteWheelSelectionFunction = RouletteWheelSelectionFunction<std::shared_ptr<RealFunction>>;

}

#endif // ROULETTE_WHEEL_SELECTION_FUNCTION_H
