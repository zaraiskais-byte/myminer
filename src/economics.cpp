#include "caesar/economics.hpp"

#include <limits>
#include <sstream>

namespace caesar {

bool Economics::validate_policy(
    const EconomicsPolicy& policy,
    std::string& error) {

    if (policy.max_supply == 0) {
        error = "max supply must be non-zero";
        return false;
    }

    if (policy.initial_reward == 0) {
        error = "initial reward must be non-zero";
        return false;
    }

    if (policy.reward_interval == 0) {
        error = "reward interval must be non-zero";
        return false;
    }

    if (policy.halving_interval == 0) {
        error = "halving interval must be non-zero";
        return false;
    }

    if (policy.initial_reward > policy.max_supply) {
        error = "initial reward exceeds max supply";
        return false;
    }

    if (!policy.fixed_max_supply) {
        error = "fixed max supply must be enabled";
        return false;
    }

    return true;
}

std::uint64_t Economics::block_reward(
    const EconomicsPolicy& policy,
    std::uint64_t height) {
    return economics_block_reward(policy, height);
}

bool Economics::supply_limit_respected(
    const EconomicsPolicy& policy,
    const EconomicsState& state) {

    if (state.total_issued > policy.max_supply)
        return false;

    if (state.circulating_supply > policy.max_supply)
        return false;

    if (state.total_burned > state.total_issued)
        return false;

    if (state.circulating_supply !=
        state.total_issued - state.total_burned)
        return false;

    return true;
}

bool Economics::validate_block_economics(
    const EconomicsPolicy& policy,
    const EconomicsState& previous,
    const BlockEconomics& block,
    EconomicsState& next,
    std::string& error) {

    if (!supply_limit_respected(policy, previous)) {
        error = "previous economic state is invalid";
        return false;
    }

    if (block.reward != block.issuance) {
        error = "issuance must equal block reward";
        return false;
    }

    if (block.reward !=
        block_reward(policy, block.height)) {
        error = "invalid block reward";
        return false;
    }

    if (block.issuance >
        policy.max_supply - previous.total_issued) {
        error = "block issuance exceeds remaining supply";
        return false;
    }

    if (block.fees >
        std::numeric_limits<std::uint64_t>::max() -
            previous.total_fees) {
        error = "fee accounting overflow";
        return false;
    }

    if (block.burned >
        std::numeric_limits<std::uint64_t>::max() -
            previous.total_burned) {
        error = "burn accounting overflow";
        return false;
    }

    if (!policy.fee_burning && block.burned != 0) {
        error = "fee burning is disabled";
        return false;
    }

    if (block.burned >
        previous.total_issued + block.issuance) {
        error = "burn exceeds issued value";
        return false;
    }

    next = previous;

    next.total_issued += block.issuance;
    next.total_fees += block.fees;
    next.total_burned += block.burned;

    if (next.total_issued > policy.max_supply) {
        error = "supply limit exceeded";
        return false;
    }

    if (next.total_burned > next.total_issued) {
        error = "burn exceeds total issuance";
        return false;
    }

    next.circulating_supply =
        next.total_issued - next.total_burned;

    if (!supply_limit_respected(policy, next)) {
        error = "resulting economic state is invalid";
        return false;
    }

    return true;
}

std::string Economics::policy_id(
    const EconomicsPolicy& policy) {

    std::ostringstream out;

    out << "CZR-ECONOMICS-V1:"
        << policy.max_supply << ":"
        << policy.initial_reward << ":"
        << policy.minimum_fee << ":"
        << policy.reward_interval << ":"
        << policy.halving_interval << ":"
        << (policy.fixed_max_supply ? 1 : 0) << ":"
        << (policy.fee_burning ? 1 : 0);

    return out.str();
}

}
