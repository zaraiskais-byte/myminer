#include <caesar/fee_policy.hpp>

#include <limits>

namespace caesar {

bool FeePolicyEngine::validate_policy(
    const FeePolicy& policy,
    std::string& error) {

    error.clear();

    if (policy.founder_fee_bps >= CZR_BPS_DENOMINATOR) {
        error = "founder fee rate must be below 100 percent";
        return false;
    }

    if (policy.founder_treasury.empty()) {
        error = "founder treasury is empty";
        return false;
    }

    return true;
}

std::uint64_t FeePolicyEngine::calculate_founder_fee(
    const FeePolicy& policy,
    std::uint64_t transaction_amount) {

    if (transaction_amount == 0 ||
        policy.founder_fee_bps == 0) {
        return 0;
    }

    if (transaction_amount >
        std::numeric_limits<std::uint64_t>::max() /
            policy.founder_fee_bps) {

        throw std::overflow_error(
            "founder fee multiplication overflow");
    }

    const std::uint64_t product =
        transaction_amount *
        policy.founder_fee_bps;

    std::uint64_t fee =
        product / CZR_BPS_DENOMINATOR;

    if (fee == 0 &&
        policy.minimum_founder_fee != 0) {
        fee = policy.minimum_founder_fee;
    }

    return fee;
}

bool FeePolicyEngine::calculate(
    const FeePolicy& policy,
    std::uint64_t transaction_amount,
    std::uint64_t network_fee,
    FeeBreakdown& result,
    std::string& error) {

    if (!validate_policy(policy, error))
        return false;

    if (network_fee < policy.minimum_network_fee) {
        error = "network fee below minimum";
        return false;
    }

    std::uint64_t founder_fee = 0;

    try {
        founder_fee =
            calculate_founder_fee(
                policy,
                transaction_amount);
    } catch (const std::overflow_error& ex) {
        error = ex.what();
        return false;
    }

    if (founder_fee > transaction_amount) {
        error = "founder fee exceeds transaction amount";
        return false;
    }

    if (network_fee >
        std::numeric_limits<std::uint64_t>::max() -
            founder_fee) {

        error = "total fee overflow";
        return false;
    }

    result.transaction_amount = transaction_amount;
    result.network_fee = network_fee;
    result.founder_fee = founder_fee;
    result.total_fee = network_fee + founder_fee;

    return true;
}

bool FeePolicyEngine::validate_breakdown(
    const FeePolicy& policy,
    const FeeBreakdown& breakdown,
    std::string& error) {

    FeeBreakdown expected;

    if (!calculate(
            policy,
            breakdown.transaction_amount,
            breakdown.network_fee,
            expected,
            error)) {
        return false;
    }

    if (expected.founder_fee !=
        breakdown.founder_fee) {
        error = "invalid founder fee";
        return false;
    }

    if (expected.total_fee !=
        breakdown.total_fee) {
        error = "invalid total fee";
        return false;
    }

    return true;
}

}
