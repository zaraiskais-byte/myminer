#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/chain_replacement.hpp>

using namespace caesar;

static Block make_genesis() {
    Block genesis;

    genesis.header.version = 1;
    genesis.header.height = 0;
    genesis.header.previous_hash = {};
    genesis.header.timestamp = 0;
    genesis.header.nonce = 0;
    genesis.header.difficulty = 0;

    Transaction tx;
    tx.outputs.push_back(
        TransactionOutput{
            1,
            "CAESAR_GENESIS_BURN"
        });

    genesis.transactions.push_back(tx);
    genesis.update_merkle_root();

    return genesis;
}

int main() {
    std::vector<Block> full_chain;
    full_chain.push_back(make_genesis());

    Mempool mempool;

    for (std::uint64_t height = 1; height <= 3; ++height) {
        Block block =
            BlockBuilder::build(
                full_chain.back(),
                mempool,
                "CZ1_TEST_MINER",
                height * 120,
                CZR_INITIAL_MINING_DIFFICULTY);

        if (!BlockBuilder::mine(block, 0, 500000)) {
            std::cerr << "failed to mine test block "
                      << height << '\n';
            return 1;
        }

        full_chain.push_back(block);
    }

    const std::vector<Block> current(
        full_chain.begin(),
        full_chain.begin() + 3);

    const std::vector<Block>& candidate = full_chain;

    const auto before_current = current;

    auto plan =
        prepare_chain_replacement(
            current,
            candidate);

    assert(plan.has_value());

    assert(plan->chain.size() == candidate.size());
    assert(plan->utxo.size() ==
           rebuild_utxo_set(candidate).size());

    // Preparation must not mutate the current canonical chain.
    assert(current.size() == before_current.size());

    for (std::size_t i = 0; i < current.size(); ++i) {
        assert(current[i].hash() == before_current[i].hash());
    }

    /*
     * A corrupted candidate must be rejected during preparation.
     */
    {
        auto invalid = candidate;
        ++invalid[2].header.nonce;

        const auto current_before = current;

        auto rejected =
            prepare_chain_replacement(
                current,
                invalid);

        assert(!rejected.has_value());

        assert(current.size() == current_before.size());

        for (std::size_t i = 0; i < current.size(); ++i) {
            assert(current[i].hash() ==
                   current_before[i].hash());
        }
    }

    /*
     * A candidate with equal/lower work must not replace current.
     */
    {
        auto rejected =
            prepare_chain_replacement(
                current,
                current);

        assert(!rejected.has_value());
    }

    std::cout
        << "Valid higher-work candidate prepared: PASS\n"
        << "Current canonical chain unchanged during preparation: PASS\n"
        << "Invalid candidate rejected: PASS\n"
        << "Equal-work candidate rejected: PASS\n"
        << "CHAIN REPLACEMENT PREPARATION TEST PASSED\n";

    return 0;
}
