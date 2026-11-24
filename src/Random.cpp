#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Random.h"
#include "Configuration.h"
#include "PrototypeManager.h"

namespace adef {

namespace {

// 64-bit SplitMix64 mixing step (strong avalanche across all 64 bits).
std::uint64_t splitmix64(std::uint64_t x) noexcept
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

std::string to_lower_ascii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return text;
}

constexpr std::size_t kSmallExcludedThreshold = 16;

std::unordered_set<std::size_t> collect_in_range(
    const std::vector<std::size_t>& values,
    std::size_t min_inclusive,
    std::size_t max_exclusive)
{
    std::unordered_set<std::size_t> out;
    out.reserve(values.size());
    for (std::size_t val : values) {
        if (val >= min_inclusive && val < max_exclusive) {
            out.insert(val);
        }
    }
    return out;
}

std::size_t count_unique_in_range(
    const std::vector<std::size_t>& values,
    std::size_t min_inclusive,
    std::size_t max_exclusive,
    std::unordered_set<std::size_t>& set_cache)
{
    if (values.size() > kSmallExcludedThreshold) {
        set_cache = collect_in_range(values, min_inclusive, max_exclusive);
        return set_cache.size();
    }
    std::size_t unique_count = 0;
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (*it >= min_inclusive && *it < max_exclusive &&
            std::find(values.begin(), it, *it) == it) {
            ++unique_count;
        }
    }
    return unique_count;
}

} // namespace

Random::Random() :
    seed_(0),
    auto_seeded_(false),
    generator_(std::make_unique<std::mt19937>(0))
{
}

Random::Random(SeedType seed_value) :
    seed_(seed_value),
    auto_seeded_(false),
    generator_(std::make_unique<std::mt19937>(seed_value))
{
}

Random::Random(CustomEngineTag, SeedType seed_value) :
    seed_(seed_value),
    auto_seeded_(false),
    generator_(nullptr)
{
}

Random::Random(const Random& rhs) :
    Prototype(rhs),
    seed_(rhs.seed_),
    auto_seeded_(rhs.auto_seeded_),
    generator_(rhs.generator_ ? std::make_unique<std::mt19937>(*rhs.generator_)
                              : nullptr)
{
}

Random::~Random() = default;

void Random::setup(const Configuration& config,
                   const PrototypeManager& /*pm*/)
{
    auto seed_config = config.get_config("seed");
    if (seed_config.is_null()) {
        auto_seed();
        return;
    }

    if (seed_config.is_string()) {
        const std::string mode = to_lower_ascii(seed_config.get_string_value());
        if (mode == "auto" || mode == "random") {
            auto_seed();
            return;
        }
        throw std::invalid_argument(
            "Invalid Random \"seed\" mode: \"" + seed_config.get_string_value() +
            "\". Expected a non-negative integer, \"auto\", or \"random\".");
    }

    if (seed_config.is_number_integer()) {
        const double numeric_value = seed_config.get_double_value();
        if (numeric_value < 0.0 ||
            numeric_value > static_cast<double>(std::numeric_limits<SeedType>::max())) {
            throw std::invalid_argument(
                "Invalid Random \"seed\": must be a non-negative 32-bit integer.");
        }
        seed(static_cast<SeedType>(seed_config.get_uint_value()));
        return;
    }

    throw std::invalid_argument(
        "Invalid Random \"seed\" type: expected a non-negative integer, "
        "\"auto\", \"random\", or null.");
}

void Random::seed(SeedType seed_value)
{
    seed_ = seed_value;
    auto_seeded_ = false;
    seed_engine(seed_value);
}

Random::SeedType Random::auto_seed()
{
    seed_ = generate_seed();
    auto_seeded_ = true;
    seed_engine(seed_);
    return seed_;
}

Random::SeedType Random::generate_seed()
{
    // Monotonic atomic counter ensures distinct inputs even when called
    // multiple times within the same clock tick or across threads.
    static std::atomic<std::uint64_t> counter{0};

    // Combine two 32-bit OS entropy draws into a 64-bit value, falling back to
    // a fixed non-zero constant (golden ratio) if std::random_device fails.
    std::uint64_t entropy = 0;
    try {
        std::random_device rd;
        entropy = (static_cast<std::uint64_t>(rd()) << 32) ^
                  static_cast<std::uint64_t>(rd());
    }
    catch (...) {
        entropy = 0x9e3779b97f4a7c15ULL;
    }

    // Mix high-resolution monotonic clock, wall-clock time, sequence counter,
    // and thread ID so rapid or concurrent calls do not collide even if
    // std::random_device is low-entropy on a given platform.
    const auto steady_ticks = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto system_ticks = static_cast<std::uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    const auto seq = counter.fetch_add(1, std::memory_order_relaxed) + 1ULL;
    const auto tid = static_cast<std::uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));

    // Three rounds of SplitMix64 diffuse all entropy sources without simple
    // XOR cancellation, then fold 64 bits down to 32 bits.
    std::uint64_t mixed = splitmix64(entropy ^ steady_ticks);
    mixed = splitmix64(mixed ^ system_ticks ^ (seq * 0x9e3779b97f4a7c15ULL));
    mixed = splitmix64(mixed ^ tid);
    return static_cast<SeedType>(mixed ^ (mixed >> 32));
}

Random::SeedType Random::derive_seed(SeedType master_seed,
                                     std::size_t stream_index) noexcept
{
    // Bijective 32-bit mixing (golden ratio stride + lowbias32 finalizer)
    // guarantees distinct seeds across all stream indices for a fixed master seed.
    std::uint32_t h = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(master_seed) +
        (static_cast<std::uint64_t>(stream_index) + 1ULL) * 0x9e3779b9ULL);
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    h ^= h >> 16;
    return h;
}

std::shared_ptr<Random> Random::spawn(SeedType new_seed) const
{
    auto instance = clone();
    instance->seed(new_seed);
    return instance;
}

std::shared_ptr<Random> Random::derive(std::size_t stream_index) const
{
    return spawn(derive_seed(seed_, stream_index));
}

Random::result_type Random::next()
{
    if (!generator_) {
        generator_ = std::make_unique<std::mt19937>(seed_);
    }
    return static_cast<result_type>((*generator_)());
}

void Random::seed_engine(SeedType seed_value)
{
    if (generator_) {
        generator_->seed(seed_value);
    }
    else {
        generator_ = std::make_unique<std::mt19937>(seed_value);
    }
}

std::size_t Random::uniform_index(std::size_t bound)
{
    if (bound == 0) {
        throw std::invalid_argument(
            "Invalid bound for Random::uniform_index: bound must be > 0.");
    }
    return uniform_int<std::size_t>(0, bound - 1);
}

std::vector<std::size_t> Random::sample_exclusive_indices(
    std::size_t count,
    std::size_t min_inclusive,
    std::size_t max_exclusive,
    std::vector<std::size_t>& excluded)
{
    if (min_inclusive > max_exclusive) {
        throw std::invalid_argument(
            "Invalid range for Random::sample_exclusive_indices: "
            "min_inclusive > max_exclusive.");
    }
    if (count == 0) {
        return {};
    }

    const std::size_t range_size = max_exclusive - min_inclusive;
    std::unordered_set<std::size_t> excluded_set;
    const std::size_t used_in_range =
        count_unique_in_range(excluded, min_inclusive, max_exclusive, excluded_set);
    if (count > range_size - used_in_range) {
        throw std::invalid_argument(
            "Requested number of exclusive random indices exceeds available range.");
    }

    auto ensure_excluded_set = [&]() -> std::unordered_set<std::size_t>& {
        if (excluded_set.empty() && !excluded.empty()) {
            excluded_set = collect_in_range(excluded, min_inclusive, max_exclusive);
        }
        return excluded_set;
    };

    auto finish_with_fisher_yates = [&](std::vector<std::size_t>& out) {
        const auto& used = ensure_excluded_set();
        std::vector<std::size_t> pool;
        pool.reserve(range_size - used.size());
        for (std::size_t val = min_inclusive; val < max_exclusive; ++val) {
            if (used.find(val) == used.end()) {
                pool.push_back(val);
            }
        }
        const std::size_t remaining = count - out.size();
        for (std::size_t i = 0; i < remaining; ++i) {
            const std::size_t j = uniform_int<std::size_t>(i, pool.size() - 1);
            std::swap(pool[i], pool[j]);
            out.push_back(pool[i]);
            excluded.push_back(pool[i]);
        }
    };

    std::vector<std::size_t> result;
    result.reserve(count);

    // High fill ratio (>= 50%): sample directly via partial Fisher-Yates.
    if (count + used_in_range >= range_size / 2 + (range_size % 2)) {
        finish_with_fisher_yates(result);
        return result;
    }

    // Low fill ratio: rejection sampling with bounded retries fallback
    // (prevents unbounded retries on repeated collisions while expecting
    // a standard-conforming URBG for underlying <random> distributions).
    const bool use_hash =
        !excluded_set.empty() || (excluded.size() + count > kSmallExcludedThreshold);
    if (use_hash) {
        ensure_excluded_set();
    }

    constexpr std::size_t kMaxRejectionsPerDraw = 64;
    const std::size_t max_attempts =
        (count > std::numeric_limits<std::size_t>::max() / kMaxRejectionsPerDraw)
            ? std::numeric_limits<std::size_t>::max()
            : count * kMaxRejectionsPerDraw;

    for (std::size_t attempts = 0; result.size() < count; ++attempts) {
        if (attempts >= max_attempts) {
            constexpr std::size_t kMaxFallbackPoolSize = 1000000;
            if (range_size <= kMaxFallbackPoolSize) {
                finish_with_fisher_yates(result);
                return result;
            }
            throw std::runtime_error(
                "Random::sample_exclusive_indices failed to find distinct "
                "indices after excessive rejections.");
        }

        const std::size_t candidate =
            uniform_int<std::size_t>(min_inclusive, max_exclusive - 1);
        const bool already_used = use_hash
            ? (excluded_set.find(candidate) != excluded_set.end())
            : (std::find(excluded.begin(), excluded.end(), candidate) != excluded.end());
        if (!already_used) {
            result.push_back(candidate);
            excluded.push_back(candidate);
            if (use_hash) {
                excluded_set.insert(candidate);
            }
        }
    }

    return result;
}

std::vector<std::size_t> Random::sample_exclusive_indices(
    std::size_t count,
    std::size_t min_inclusive,
    std::size_t max_exclusive)
{
    std::vector<std::size_t> excluded;
    return sample_exclusive_indices(count, min_inclusive, max_exclusive, excluded);
}

} // namespace adef
