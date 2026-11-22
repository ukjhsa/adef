#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <memory>
#include <string>
#include <map>
#include <stdexcept>
#include "Prototype.h"
#include "EvolutionaryState.h"

namespace adef {

class Configuration;
class PrototypeManager;
class Repository;

/**
@brief Parameters is the parameters storage with pairs of name-value.

Parameters can be used for passing information to other evolutionary states.

Some feature is similar to boost::any.

@par The configuration
Parameters has no extra configurations.@n
See setup() for the details.
*/
class Parameters : public Prototype, public EvolutionaryState
{
public:

    Parameters() = default;
    Parameters(const Parameters& rhs);
    Parameters& operator=(const Parameters& rhs) = delete;

/**
@brief Clone the current class.
@sa clone_impl()
*/
    std::shared_ptr<Parameters> clone() const
    {
        return std::dynamic_pointer_cast<Parameters>(clone_impl());
    }
/**
@brief Set up the internal states.

Parameters's configuration should be
- JSON configuration
@code
"Parameters" : {
    "classname" : "Parameters"
}
@endcode
.
*/
    void setup(const Configuration& config,
               const PrototypeManager& pm) override;

    void init(std::shared_ptr<Repository> repos) override;

private:

    /**
    @brief BaseParameter is the base class of Parameter that stores a parameter.

    Some feature is similar to boost::any::placeholder.
    */
    class BaseParameter
    {
    public:

        virtual ~BaseParameter() = default;

    /**
    @brief Clone the current class.
    @sa clone_impl()
    */
        std::shared_ptr<BaseParameter> clone() const
        {
            return clone_impl();
        }

    private:

        virtual std::shared_ptr<BaseParameter> clone_impl() const = 0;
    };

    /**
    @brief Parameter stores a parameter of arbitrary type.

    Some feature is similar to boost::any::holder.
    */
    template<typename T>
    class Parameter : public BaseParameter
    {
    public:

    /// The type of the parameter.
        using ValueType = T;

    /// Prohibits the default constructor.
        Parameter() = delete;
        Parameter(const ValueType val) : value(val) {}
    /// Prohibits the copy constructor.
        Parameter(const Parameter& rhs) = delete;
    /// Prohibits the assignment operator.
        Parameter& operator=(const Parameter& rhs) = delete;
    /**
    @brief Clone the current class.
    @sa clone_impl()
    */
        std::shared_ptr<Parameter> clone() const
        {
            return std::static_pointer_cast<Parameter>(clone_impl());
        }

    /// The stored parameter.
        ValueType value;

    private:

        std::shared_ptr<BaseParameter> clone_impl() const override
        {
            return std::make_shared<Parameter>(value);
        }
    };

public:

/**
@brief Store the parameter.
@tparam T The type of the parameter.
@param name The parameter name to store.
@param value The value to store.
@sa store_impl()
*/
    template<typename T>
    void store(const std::string& name, const T& value)
    {
        store_impl(name, std::make_shared<Parameter<T>>(value));
    }

/**
@brief Check whether the parameter exists.
@param name The parameter name to check.
@return @c true if the parameter exists, otherwise @c false.
*/
    bool contains(const std::string& name) const
    {
        return take_out_impl(name) != nullptr;
    }

/**
@brief Take out the parameter.
@tparam T The type of the parameter.
@param name The parameter name to take out.
@exception std::out_of_range The parameter does not exist.
@exception std::runtime_error The stored parameter type does not match @a T.
@sa take_out_impl()
*/
    template<typename T>
    T take_out(const std::string& name) const
    {
        auto base = take_out_impl(name);
        if (!base) {
            throw std::out_of_range("Parameter \"" + name + "\" not found");
        }
        auto param = std::dynamic_pointer_cast<Parameter<T>>(base);
        if (!param) {
            throw std::runtime_error("Parameter \"" + name + "\" type mismatch");
        }
        return param->value;
    }

private:

/**
@brief Store the parameter.
@param name The parameter name to store.
@param value The parameter value to store.
*/
    void store_impl(const std::string& name,
                    std::shared_ptr<BaseParameter> value);
/**
@brief take_out the parameter.
@param name The parameter name to take out.
*/
    std::shared_ptr<BaseParameter> take_out_impl(const std::string& name) const;

private:

/**
@brief The parameters' storage.

The storage is the pairs of name-value
(the parameter name and the value of parameter.)
*/
    std::map<std::string, std::shared_ptr<BaseParameter>> params_;

private:

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<Parameters>(*this);
    }
};

}

#endif
