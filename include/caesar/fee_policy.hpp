#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace caesar {

struct FeePolicy {
    std::uint32_t founder_fee_bps = 0;
    std::uint64_t minimum_network_fee = 0;
    std::uint64_t minimum_founder_fee = 0;
    std::string founder_treasury;
};

struct FeeBreakdown {
    std::uint64_t transaction_amount = 0;
    std::uint64_t network_fee = 0;
    std::uint64_t founder_fee = 0;
    std::uint64_t total_fee = 0;
};

inline constexpr std::uint32_t CZR_BPS_DENOMINATOR = 10000;

inline constexpr FeePolicy czr_fee_policy() {
    return FeePolicy{
        10,
        0,
        0,
        "CAESAR_FOUNDER_TREASURY_V1"
    };
}

class FeePolicyEngine {
public:
    static bool validate_policy(
        const FeePolicy& policy,
        std::string& error);

    static std::uint64_t calculate_founder_fee(
        const FeePolicy& policy,
        std::uint64_t transaction_amount);

    static bool calculate(
        const FeePolicy& policy,
        std::uint64_t transaction_amount,
        std::uint64_t network_fee,
        FeeBreakdown& result,
        std::string& error);

    static bool validate_breakdown(
        const FeePolicy& policy,
        const FeeBreakdown& breakdown,
        std::string& error);
};

}
