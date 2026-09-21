#pragma once

#include <cstdint>
#include <string>

namespace caesar {

struct EconomicsPolicy {
    std::uint64_t max_supply = 0;
    std::uint64_t initial_reward = 0;
    std::uint64_t minimum_fee = 0;
    std::uint64_t reward_interval = 0;
    std::uint64_t halving_interval = 0;
    bool fixed_max_supply = false;
    bool fee_burning = false;
};

struct EconomicsState {
    std::uint64_t circulating_supply = 0;
    std::uint64_t total_issued = 0;
    std::uint64_t total_fees = 0;
    std::uint64_t total_burned = 0;
};

struct BlockEconomics {
    std::uint64_t height = 0;
    std::uint64_t reward = 0;
    std::uint64_t fees = 0;
    std::uint64_t burned = 0;
    std::uint64_t issuance = 0;
};

inline constexpr EconomicsPolicy czr_economics_policy() {
    return EconomicsPolicy{
        21000000ULL * 100000000ULL,
        50ULL * 100000000ULL,
        0,
        1,
        210000ULL,
        true,
        false
    };
}

inline constexpr std::uint64_t economics_block_reward(
    const EconomicsPolicy& policy,
    std::uint64_t height) {

    if (policy.halving_interval == 0)
        return 0;

    const std::uint64_t halvings =
        height / policy.halving_interval;

    if (halvings >= 64)
        return 0;

    return policy.initial_reward >> halvings;
}

class Economics {
public:
    static bool validate_policy(
        const EconomicsPolicy& policy,
        std::string& error);

    static std::uint64_t block_reward(
        const EconomicsPolicy& policy,
        std::uint64_t height);

    static bool validate_block_economics(
        const EconomicsPolicy& policy,
        const EconomicsState& previous,
        const BlockEconomics& block,
        EconomicsState& next,
        std::string& error);

    static bool supply_limit_respected(
        const EconomicsPolicy& policy,
        const EconomicsState& state);

    static std::string policy_id(
        const EconomicsPolicy& policy);
};

}
