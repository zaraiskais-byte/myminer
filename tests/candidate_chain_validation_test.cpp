#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

#include <caesar/block_builder.hpp>

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
    std::vector<Block> chain;
    chain.push_back(make_genesis());

    Mempool mempool;

    /*
     * Bootstrap blocks use the initial mining difficulty.
     * Mine real PoW rather than fabricating nonces.
     */
    for (std::uint64_t height = 1; height <= 3; ++height) {
        Block block =
            BlockBuilder::build(
                chain.back(),
                mempool,
                "CZ1_TEST_MINER",
                height * 120,
                CZR_INITIAL_MINING_DIFFICULTY);

        if (!BlockBuilder::mine(
                block,
                0,
                500000)) {
            std::cerr << "failed to mine test block "
                      << height << '\n';
            return 1;
        }

        chain.push_back(block);
    }

    assert(validate_candidate_chain(chain));

    /*
     * Break the chain link.
     */
    {
        auto invalid = chain;
        invalid[2].header.previous_hash = {};

        assert(!validate_candidate_chain(invalid));
    }

    /*
     * Break PoW.
     */
    {
        auto invalid = chain;
        ++invalid[2].header.nonce;

        assert(!validate_candidate_chain(invalid));
    }

    /*
     * Break the bootstrap difficulty.
     */
    {
        auto invalid = chain;
        invalid[1].header.difficulty =
            CZR_INITIAL_MINING_DIFFICULTY + 1;

        assert(!validate_candidate_chain(invalid));
    }

    /*
     * Break chronological ordering.
     */
    {
        auto invalid = chain;
        invalid[2].header.timestamp =
            invalid[1].header.timestamp - 1;

        assert(!validate_candidate_chain(invalid));
    }

    std::cout << "CandidateChainValidationTest: PASS\n";
    return 0;
}
