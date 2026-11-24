#ifndef RANDOM_H
#define RANDOM_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "Prototype.h"

namespace adef {

/**
@brief Random provides an extensible, instance-isolated pseudo-random number
generator abstraction compatible with C++ `<random>` distributions.

Random satisfies the C++17 `UniformRandomBitGenerator` concept (`result_type`,
`min()`, `max()`, and `operator()()`), delegating draws through the virtual
next() hook so alternative PRNG engines or deterministic test doubles can be
substituted polymorphically without modifying callers.

By default, Random uses `std::mt19937`.

@par The configuration
Random has optional configuration:
- member
    - name: "seed"
    - value: non-negative integer, or `"auto"` / `"random"` / `null` (or
      omitted) to generate a non-deterministic seed.
.
See setup() for details.
*/
class Random : public Prototype
{
public:

/// Unsigned 32-bit output type required by UniformRandomBitGenerator.
    using result_type = std::uint32_t;
/// Seed value type.
    using SeedType = std::uint32_t;

/**
@brief Construct a Random generator seeded deterministically with 0.
*/
    Random();

/**
@brief Construct a Random generator seeded deterministically with @p seed_value.
*/
    explicit Random(SeedType seed_value);

/**
@brief Copy constructor that faithfully copies the seed and current engine state.
*/
    Random(const Random& rhs);

    virtual ~Random();
    Random& operator=(const Random& rhs) = delete;

/**
@brief Minimum value potentially returned by operator()() (URBG requirement).
*/
    static constexpr result_type min() noexcept
    {
        return std::numeric_limits<result_type>::min();
    }

/**
@brief Maximum value potentially returned by operator()() (URBG requirement).
*/
    static constexpr result_type max() noexcept
    {
        return std::numeric_limits<result_type>::max();
    }

/**
@brief Generate the next raw pseudo-random word (URBG requirement).
*/
    result_type operator()()
    {
        return next();
    }

/**
@brief Clone the current Random instance, preserving its exact engine state.
@sa clone_impl()
*/
    std::shared_ptr<Random> clone() const
    {
        return std::dynamic_pointer_cast<Random>(clone_impl());
    }

/**
@brief Set up the internal states from configuration.

Random supports the following JSON configurations:
@code
"Random" : {
    "classname" : "Random",
    "seed" : 0
}
@endcode
or auto-seeded mode (`"seed"` omitted, `null`, `"auto"`, or `"random"`):
@code
"Random" : {
    "classname" : "Random",
    "seed" : "auto"
}
@endcode
*/
    virtual void setup(const Configuration& config,
                       const PrototypeManager& pm) override;

/**
@brief Return the effective seed currently used by this generator.
*/
    SeedType seed() const noexcept { return seed_; }

/**
@brief Return whether the current seed was auto-generated.
*/
    bool is_auto_seeded() const noexcept { return auto_seeded_; }

/**
@brief Re-seed the engine with a deterministic seed.
*/
    void seed(SeedType seed_value);

/**
@brief Re-seed the engine with a newly generated non-deterministic seed.
@return The effective auto-generated seed.
*/
    SeedType auto_seed();

/**
@brief Generate a non-deterministic seed from OS entropy and clocks.
*/
    static SeedType generate_seed();

/**
@brief Deterministically derive a distinct stream seed from a master seed and
stream index.
*/
    static SeedType derive_seed(SeedType master_seed,
                                std::size_t stream_index) noexcept;

/**
@brief Create a new Random instance of the same dynamic type seeded with
@p new_seed.
*/
    virtual std::shared_ptr<Random> spawn(SeedType new_seed) const;

/**
@brief Deterministically derive an independent per-run Random instance of the
same dynamic type for @p stream_index.
*/
    virtual std::shared_ptr<Random> derive(std::size_t stream_index) const;

/**
@brief Polymorphic PRNG draw hook returning a uniformly distributed 32-bit word.
*/
    virtual result_type next();

/**
@brief Generate a variate using any C++ `<random>` distribution.
*/
    template<typename Distribution>
    typename std::decay_t<Distribution>::result_type
    generate(Distribution&& distribution)
    {
        static_assert(
            !std::is_const_v<std::remove_reference_t<Distribution>>,
            "Random::generate requires a non-const distribution.");
        return distribution(*this);
    }

/**
@brief Generate an unbiased uniform integer in the closed interval
`[min_val, max_val]`.
@exception std::invalid_argument if @p min_val > @p max_val.
*/
    template<typename IntType = std::size_t>
    IntType uniform_int(IntType min_val, IntType max_val)
    {
        static_assert(std::is_integral<IntType>::value &&
                          !std::is_same<IntType, bool>::value,
                      "Random::uniform_int requires a non-bool integral type.");
        if (min_val > max_val) {
            throw std::invalid_argument(
                "Invalid range for Random::uniform_int: min_val > max_val.");
        }
        std::uniform_int_distribution<IntType> dist(min_val, max_val);
        return generate(dist);
    }

/**
@brief Generate an unbiased uniform index in the half-open interval
`[0, bound)`.
@exception std::invalid_argument if @p bound == 0.
*/
    std::size_t uniform_index(std::size_t bound);

/**
@brief Sample @p count mutually exclusive indices uniformly from
`[min_inclusive, max_exclusive)` while avoiding all indices in @p excluded.

Newly sampled indices are appended to @p excluded.

@exception std::invalid_argument if @p min_inclusive > @p max_exclusive or if
@p count exceeds the number of available indices.
*/
    std::vector<std::size_t> sample_exclusive_indices(
        std::size_t count,
        std::size_t min_inclusive,
        std::size_t max_exclusive,
        std::vector<std::size_t>& excluded);

/**
@brief Sample @p count mutually exclusive indices uniformly from
`[min_inclusive, max_exclusive)`.

@exception std::invalid_argument if @p min_inclusive > @p max_exclusive or if
@p count exceeds the number of available indices.
*/
    std::vector<std::size_t> sample_exclusive_indices(
        std::size_t count,
        std::size_t min_inclusive,
        std::size_t max_exclusive);

protected:

/**
@brief Tag type for derived classes that own a custom PRNG engine and do not
need the default `std::mt19937` instance allocated by Random.
*/
    struct CustomEngineTag {};

/**
@brief Construct a Random base without allocating the default `std::mt19937`
engine.
*/
    explicit Random(CustomEngineTag, SeedType seed_value = 0);

/**
@brief Polymorphic hook called by seed() and auto_seed() to seed the underlying
PRNG engine.
*/
    virtual void seed_engine(SeedType seed_value);

    SeedType seed_;
    bool auto_seeded_;

private:

    std::unique_ptr<std::mt19937> generator_;

    virtual std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<Random>(*this);
    }
};

/**
@brief Adapter template exposing any C++ `<random>` PRNG engine through the
polymorphic Random interface.
@tparam Engine Standard `<random>` engine type (e.g., `std::mt19937_64`,
`std::minstd_rand`).
*/
template<typename Engine>
class EngineRandom : public Random
{
private:

    using AdaptedEngine = std::conditional_t<
        Engine::min() == 0 &&
            Engine::max() == std::numeric_limits<result_type>::max(),
        Engine,
        std::independent_bits_engine<Engine, 32, result_type>>;

public:

    EngineRandom() :
        Random(CustomEngineTag{}),
        engine_(static_cast<typename AdaptedEngine::result_type>(seed_))
    {
    }

    explicit EngineRandom(SeedType seed_value) :
        Random(CustomEngineTag{}, seed_value),
        engine_(static_cast<typename AdaptedEngine::result_type>(seed_value))
    {
    }

    EngineRandom(const EngineRandom& rhs) :
        Random(rhs),
        engine_(rhs.engine_)
    {
    }

    ~EngineRandom() override = default;
    EngineRandom& operator=(const EngineRandom& rhs) = delete;

    std::shared_ptr<EngineRandom> clone() const
    {
        return std::dynamic_pointer_cast<EngineRandom>(clone_impl());
    }

    result_type next() override
    {
        return static_cast<result_type>(engine_());
    }

protected:

    void seed_engine(SeedType seed_value) override
    {
        engine_.seed(static_cast<typename AdaptedEngine::result_type>(seed_value));
    }

private:

    AdaptedEngine engine_;

    std::shared_ptr<Prototype> clone_impl() const override
    {
        return std::make_shared<EngineRandom>(*this);
    }
};

/// Default Mersenne Twister 32-bit Random alias.
using Mt19937Random = Random;
/// Mersenne Twister 64-bit Random engine.
using Mt19937_64Random = EngineRandom<std::mt19937_64>;
/// Minimal standard linear congruential Random engine.
using MinstdRandRandom = EngineRandom<std::minstd_rand>;

} // namespace adef

#endif // RANDOM_H
