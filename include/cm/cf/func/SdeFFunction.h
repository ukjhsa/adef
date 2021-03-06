#ifndef SDE_F_FUNCTION_H
#define SDE_F_FUNCTION_H

#include <memory>
#include <vector>
#include <string>
#include <any>
#include "Function.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Individual;

/**
@brief JdeFFunction generates numbers according to
the procedure of the F value in the SDE algorithm.

JdeFFunction has the following feature:
- generate(): return the number generated by
the procedure of the F value in the SDE algorithm.
- record(): record parameters into the rand component.
- update(): update values of rand.

@par Requirement
record parameters into Function:
- the size of parameters is equal to the value of "number_of_parameters"
.

ADEF supports many kinds of SdeFFunction:
- SdeFFunction.

@par The configuration
SdeFFunction has extra configurations:
- member
    - name: "rand"
    - value: object configuration which is the class derived from
             Function.
- member
    - name: "number_of_parameters"
    - value: <tt>unsigned int</tt>
.
See setup() for the details.
*/
class SdeFFunction : public Function<double>
{
public:

    using T = double;
/// @copydoc Function::Object
    using Object = typename Function<T>::Object;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<SdeFFunction> clone() const
    {
        return std::dynamic_pointer_cast<SdeFFunction>(clone_impl());
    }
/**
@brief Set up the internal states.

If @em SomeThing is the SdeFFunction and
has the following configuration:
- rand: ::RealNormalDisFunction of
        mean ::RealConstantFunction of object 0.0
        and stddev ::RealConstantFunction of object 0.5
- number_of_parameters: 3
.
its configuration should be
- JSON configuration
@code
"SomeThing" : {
    "classname" : "SdeFFunction",
    "rand" : {
        "classname" : "RealNormalDisFunction",
        "mean" : {
            "classname" : "RealConstantFunction",
            "object" : 0.0
        },
        "stddev" : {
            "classname" : "RealConstantFunction",
            "object" : 0.5
        }
    },
    "number_of_parameters" : 3
}
@endcode
.
*/
    void setup(const Configuration& config, const PrototypeManager& pm) override;

    Object generate() override;

    bool record(const std::vector<std::any>& params,
                const std::string& name = "") override;

    bool record(const std::vector<std::any>& params,
                std::shared_ptr<const Individual> parent,
                std::shared_ptr<const Individual> offspring,
                const std::string& name = "") override;

    void update() override;

    unsigned int number_of_parameters() const override;

private:

/// The list of parameters
    std::vector<Object> parameters_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<SdeFFunction>(*this);
    }
};

}

#endif // SDE_F_FUNCTION_H
