#include "caesar/economics.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

using namespace caesar;

namespace {

EconomicsPolicy policy() {
    EconomicsPolicy p;
    p.max_supply = 21000000;
    p.initial_reward = 50;
    p.minimum_fee = 1;
    p.reward_interval = 1;
    p.halving_interval = 210000;
    p.fixed_max_supply = true;
    p.fee_burning = false;
    return p;
}

void test_policy() {
    auto p = policy();
    std::string error;

    assert(Economics::validate_policy(p, error));
    assert(error.empty());

    auto bad = p;
    bad.max_supply = 0;
    assert(!Economics::validate_policy(bad, error));

    bad = p;
    bad.initial_reward = 0;
    assert(!Economics::validate_policy(bad, error));

    bad = p;
    bad.reward_interval = 0;
    assert(!Economics::validate_policy(bad, error));

    bad = p;
    bad.halving_interval = 0;
    assert(!Economics::validate_policy(bad, error));

    bad = p;
    bad.fixed_max_supply = false;
    assert(!Economics::validate_policy(bad, error));
}

void test_czr_protocol_policy_matches_coinbase_constants() {
    const auto p = czr_economics_policy();

    assert(p.max_supply == 21000000ULL * 100000000ULL);
    assert(p.initial_reward == 50ULL * 100000000ULL);
    assert(p.halving_interval == 210000ULL);
    assert(p.fixed_max_supply);
    assert(!p.fee_burning);

    assert(economics_block_reward(p, 1) ==
           50ULL * 100000000ULL);

    assert(economics_block_reward(
               p,
               210000) ==
           25ULL * 100000000ULL);

    assert(economics_block_reward(
               p,
               420000) ==
           1250000000ULL);
}

void test_reward() {
    const auto p = policy();

    assert(Economics::block_reward(p, 0) == 50);
    assert(Economics::block_reward(p, 209999) == 50);
    assert(Economics::block_reward(p, 210000) == 25);
    assert(Economics::block_reward(p, 420000) == 12);
}

void test_block_economics() {
    const auto p = policy();

    EconomicsState previous;
    EconomicsState next;

    BlockEconomics block;
    block.height = 0;
    block.reward = 50;
    block.issuance = 50;
    block.fees = 3;
    block.burned = 0;

    std::string error;

    assert(Economics::validate_block_economics(
        p, previous, block, next, error));

    assert(next.total_issued == 50);
    assert(next.total_fees == 3);
    assert(next.total_burned == 0);
    assert(next.circulating_supply == 50);
}

void test_invalid_reward() {
    const auto p = policy();

    EconomicsState previous;
    EconomicsState next;

    BlockEconomics block;
    block.height = 0;
    block.reward = 51;
    block.issuance = 51;

    std::string error;

    assert(!Economics::validate_block_economics(
        p, previous, block, next, error));

    assert(error == "invalid block reward");
}

void test_previous_state_above_supply_limit() {
    const auto p = policy();

    EconomicsState previous;
    previous.total_issued = p.max_supply + 1;
    previous.total_burned = 0;
    previous.circulating_supply = previous.total_issued;

    EconomicsState next;
    BlockEconomics block;
    block.height = 0;
    block.reward = 50;
    block.issuance = 50;

    std::string error;

    assert(!Economics::validate_block_economics(
        p, previous, block, next, error));

    assert(error == "previous economic state is invalid");
}

void test_supply_limit_without_underflow() {
    auto p = policy();
    p.max_supply = 50;

    EconomicsState previous;
    previous.total_issued = 50;
    previous.total_burned = 0;
    previous.circulating_supply = 50;

    EconomicsState next;

    BlockEconomics block;
    block.height = 0;
    block.reward = 50;
    block.issuance = 50;

    std::string error;

    assert(!Economics::validate_block_economics(
        p, previous, block, next, error));

    assert(error == "block issuance exceeds remaining supply");
}

void test_supply_limit_respected() {
    auto p = policy();

    EconomicsState state;
    state.total_issued = p.max_supply;
    state.total_burned = 0;
    state.circulating_supply = p.max_supply;

    assert(Economics::supply_limit_respected(p, state));

    state.circulating_supply = p.max_supply + 1;

    assert(!Economics::supply_limit_respected(p, state));
}

void test_inconsistent_supply_state() {
    const auto p = policy();

    EconomicsState state;
    state.total_issued = 100;
    state.total_burned = 10;
    state.circulating_supply = 100;

    assert(!Economics::supply_limit_respected(p, state));
}

void test_burn_disabled() {
    const auto p = policy();

    EconomicsState previous;
    EconomicsState next;

    BlockEconomics block;
    block.height = 0;
    block.reward = 50;
    block.issuance = 50;
    block.burned = 1;

    std::string error;

    assert(!Economics::validate_block_economics(
        p, previous, block, next, error));

    assert(error == "fee burning is disabled");
}

void test_burn_enabled() {
    auto p = policy();
    p.fee_burning = true;

    EconomicsState previous;
    EconomicsState next;

    BlockEconomics block;
    block.height = 0;
    block.reward = 50;
    block.issuance = 50;
    block.fees = 3;
    block.burned = 2;

    std::string error;

    assert(Economics::validate_block_economics(
        p, previous, block, next, error));

    assert(next.total_issued == 50);
    assert(next.total_fees == 3);
    assert(next.total_burned == 2);
    assert(next.circulating_supply == 48);
}

void test_burn_exceeds_issuance() {
    auto p = policy();
    p.fee_burning = true;

    EconomicsState previous;
    EconomicsState next;

    BlockEconomics block;
    block.height = 0;
    block.reward = 50;
    block.issuance = 50;
    block.burned = 51;

    std::string error;

    assert(!Economics::validate_block_economics(
        p, previous, block, next, error));

    assert(error == "burn exceeds issued value");
}

void test_fee_overflow() {
    const auto p = policy();

    EconomicsState previous;
    previous.total_issued = 0;
    previous.total_burned = 0;
    previous.circulating_supply = 0;
    previous.total_fees =
        std::numeric_limits<std::uint64_t>::max();

    EconomicsState next;

    BlockEconomics block;
    block.height = 0;
    block.reward = 50;
    block.issuance = 50;
    block.fees = 1;

    std::string error;

    assert(!Economics::validate_block_economics(
        p, previous, block, next, error));

    assert(error == "fee accounting overflow");
}

void test_burn_overflow() {
    const auto p = policy();

    EconomicsState previous;
    previous.total_issued = p.max_supply;
    previous.total_burned = 0;
    previous.circulating_supply = p.max_supply;

    EconomicsState next;

    BlockEconomics block;
    block.height = 0;
    block.reward = 50;
    block.issuance = 0;
    block.burned =
        std::numeric_limits<std::uint64_t>::max();

    std::string error;

    assert(!Economics::validate_block_economics(
        p, previous, block, next, error));

    assert(error == "issuance must equal block reward");
}

void test_policy_id() {
    const auto p = policy();

    const auto id = Economics::policy_id(p);

    assert(id.find("CZR-ECONOMICS-V1:") == 0);
    assert(id.find("21000000") != std::string::npos);
}

}

int main() {
    test_policy();
    test_czr_protocol_policy_matches_coinbase_constants();
    test_reward();
    test_block_economics();
    test_invalid_reward();
    test_previous_state_above_supply_limit();
    test_supply_limit_without_underflow();
    test_supply_limit_respected();
    test_inconsistent_supply_state();
    test_burn_disabled();
    test_burn_enabled();
    test_burn_exceeds_issuance();
    test_fee_overflow();
    test_burn_overflow();
    test_policy_id();

    std::cout << "CAESAR_ECONOMICS_SAFETY_TEST=PASS\n";
    return 0;
}
