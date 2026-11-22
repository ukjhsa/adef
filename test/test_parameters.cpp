#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "Parameters.h"

namespace {

bool require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        return false;
    }
    return true;
}

} // namespace

int main()
{
    adef::Parameters params;

    // 1. contains() before and after store()
    if (!require(!params.contains("generation"),
                 "Expected contains(\"generation\") == false initially")) {
        return 1;
    }

    params.store("generation", 42u);
    params.store("target_index", std::size_t{7});
    params.store("label", std::string{"adef"});
    params.store("ptr", std::make_shared<int>(99));

    if (!require(params.contains("generation"),
                 "Expected contains(\"generation\") == true after store")) {
        return 1;
    }

    // 2. Roundtrip take_out() with exact types
    if (!require(params.take_out<unsigned int>("generation") == 42u,
                 "Expected generation == 42u")) {
        return 1;
    }
    if (!require(params.take_out<std::size_t>("target_index") == 7,
                 "Expected target_index == 7")) {
        return 1;
    }
    if (!require(params.take_out<std::string>("label") == "adef",
                 "Expected label == \"adef\"")) {
        return 1;
    }
    if (!require(*params.take_out<std::shared_ptr<int>>("ptr") == 99,
                 "Expected *ptr == 99")) {
        return 1;
    }

    // 3. Missing key throws std::out_of_range
    bool threw_out_of_range = false;
    try {
        (void)params.take_out<unsigned int>("nonexistent");
    }
    catch (const std::out_of_range&) {
        threw_out_of_range = true;
    }
    if (!require(threw_out_of_range,
                 "Expected std::out_of_range when parameter key does not exist")) {
        return 1;
    }

    // 4. Type mismatch throws std::runtime_error (and not std::out_of_range)
    bool threw_type_mismatch = false;
    try {
        (void)params.take_out<int>("generation");
    }
    catch (const std::out_of_range&) {
        std::cerr << "FAIL: Type mismatch threw std::out_of_range instead of std::runtime_error"
                  << std::endl;
        return 1;
    }
    catch (const std::runtime_error&) {
        threw_type_mismatch = true;
    }
    if (!require(threw_type_mismatch,
                 "Expected std::runtime_error when parameter type does not match")) {
        return 1;
    }

    // 5. clone() preserves values and stays independent
    auto cloned = params.clone();
    if (!require(cloned->contains("generation") &&
                 cloned->take_out<unsigned int>("generation") == 42u,
                 "Expected cloned Parameters to retain stored values")) {
        return 1;
    }
    cloned->store("generation", 100u);
    if (!require(params.take_out<unsigned int>("generation") == 42u &&
                 cloned->take_out<unsigned int>("generation") == 100u,
                 "Expected original and cloned Parameters to be independent")) {
        return 1;
    }

    std::cout << "All Parameters tests passed." << std::endl;
    return 0;
}
