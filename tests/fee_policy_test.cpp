#include <caesar/fee_policy.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

using namespace caesar;

static void test_default_policy() {
    const FeePolicy policy = czr_fee_policy();

    assert(policy.founder_fee_bps == 10);
    assert(policy.minimum_network_fee == 0);
    assert(policy.minimum_founder_fee == 0);
    assert(!policy.founder_treasury.empty());

    std::string error;
    assert(FeePolicyEngine::validate_policy(
        policy,
        error));

    std::cout
        << "[PASS] default fee policy\n";
}

static void test_founder_fee_rate() {
    const FeePolicy policy = czr_fee_policy();

    const std::uint64_t amount =
        100ULL * 100000000ULL;

    const std::uint64_t expected =
        10000000ULL;

    assert(
        FeePolicyEngine::calculate_founder_fee(
            policy,
            amount) ==
        expected);

    std::cout
        << "[PASS] founder fee rate\n";
}

static void test_zero_amount() {
    const FeePolicy policy = czr_fee_policy();

    assert(
        FeePolicyEngine::calculate_founder_fee(
            policy,
            0) == 0);

    std::cout
        << "[PASS] zero amount\n";
}

static void test_breakdown() {
    const FeePolicy policy = czr_fee_policy();

    FeeBreakdown result;
    std::string error;

    const std::uint64_t amount =
        100ULL * 100000000ULL;

    const std::uint64_t network_fee =
        500000ULL;

    assert(
        FeePolicyEngine::calculate(
            policy,
            amount,
            network_fee,
            result,
            error));

    assert(
        result.transaction_amount ==
        amount);

    assert(
        result.network_fee ==
        network_fee);

    assert(
        result.founder_fee ==
        10000000ULL);

    assert(
        result.total_fee ==
        10500000ULL);

    std::cout
        << "[PASS] fee breakdown\n";
}

static void test_breakdown_validation() {
    const FeePolicy policy = czr_fee_policy();

    FeeBreakdown result;
    std::string error;

    assert(
        FeePolicyEngine::calculate(
            policy,
            100ULL * 100000000ULL,
            500000ULL,
            result,
            error));

    assert(
        FeePolicyEngine::validate_breakdown(
            policy,
            result,
            error));

    result.founder_fee += 1;

    assert(
        !FeePolicyEngine::validate_breakdown(
            policy,
            result,
            error));

    assert(error == "invalid founder fee");

    std::cout
        << "[PASS] breakdown validation\n";
}

static void test_minimums() {
    FeePolicy policy = czr_fee_policy();

    policy.minimum_network_fee = 1000;
    policy.minimum_founder_fee = 100;

    FeeBreakdown result;
    std::string error;

    assert(
        !FeePolicyEngine::calculate(
            policy,
            100,
            999,
            result,
            error));

    assert(
        error == "network fee below minimum");

    assert(
        FeePolicyEngine::calculate(
            policy,
            100,
            1000,
            result,
            error));

    assert(result.founder_fee == 100);

    std::cout
        << "[PASS] fee minimums\n";
}

static void test_invalid_policy() {
    FeePolicy policy = czr_fee_policy();
    policy.founder_fee_bps =
        CZR_BPS_DENOMINATOR;

    std::string error;

    assert(
        !FeePolicyEngine::validate_policy(
            policy,
            error));

    assert(
        error ==
        "founder fee rate must be below 100 percent");

    policy = czr_fee_policy();
    policy.founder_treasury.clear();

    assert(
        !FeePolicyEngine::validate_policy(
            policy,
            error));

    assert(
        error == "founder treasury is empty");

    std::cout
        << "[PASS] invalid policy rejection\n";
}

static void test_large_value() {
    const FeePolicy policy = czr_fee_policy();

    const std::uint64_t amount =
        std::numeric_limits<std::uint64_t>::max() /
        policy.founder_fee_bps;

    const std::uint64_t fee =
        FeePolicyEngine::calculate_founder_fee(
            policy,
            amount);

    assert(fee > 0);

    std::cout
        << "[PASS] large value arithmetic\n";
}

static void test_zero_rate() {
    FeePolicy policy = czr_fee_policy();
    policy.founder_fee_bps = 0;

    assert(
        FeePolicyEngine::calculate_founder_fee(
            policy,
            1000000000000ULL) == 0);

    std::cout
        << "[PASS] zero founder rate\n";
}

int main() {
    test_default_policy();
    test_founder_fee_rate();
    test_zero_amount();
    test_breakdown();
    test_breakdown_validation();
    test_minimums();
    test_invalid_policy();
    test_large_value();
    test_zero_rate();

    std::cout
        << "All FeePolicy tests passed\n";

    return 0;
}
