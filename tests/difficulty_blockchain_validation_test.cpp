#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/block_builder.hpp>

using namespace caesar;

bool check(const char* name, bool condition) {
    std::cout << name << " | "
              << (condition ? "PASS" : "FAIL")
              << '\n';
    return condition;
}

int main() {
    bool ok = true;

    std::vector<Block> chain;

    for (std::uint64_t i = 0;
         i < CZR_DIFFICULTY_WINDOW + 1;
         ++i) {

        Block block;
        block.header.version = 1;
        block.header.height = i;
        block.header.timestamp = i * 120;
        block.header.difficulty = 1;

        chain.push_back(block);
    }

    Mempool mempool;

    Block candidate =
        BlockBuilder::build(
            chain.back(),
            mempool,
            "CZ1_TEST_MINER",
            chain.back().header.timestamp + 120,
            1);

    const bool mined =
        BlockBuilder::mine(
            candidate,
            0,
            1000);

    ok &= check(
        "Candidate PoW mined",
        mined);

    ok &= check(
        "Chain-derived difficulty accepted",
        candidate.validate_against_chain_history(chain));

    candidate.header.difficulty = 2;

    ok &= check(
        "Chain-derived wrong difficulty rejected",
        !candidate.validate_against_chain_history(chain));

    candidate.header.difficulty = 0;

    std::uint64_t timestamp = 0;
    for (std::size_t i = 0; i < chain.size(); ++i) {
        timestamp += (i >= chain.size() - 6) ? 240 : 120;
        chain[i].header.timestamp = timestamp;
    }

    ok &= check(
        "Majority slow history requires lower difficulty",
        candidate.validate_against_chain_history(chain));

    // Regression: validating a historical block inside a chain
    // must use that block's own difficulty window, not the final tip.
    std::vector<Block> long_chain;

    for (std::size_t i = 0; i < 20; ++i) {
        Block block;
        block.header.version = 1;
        block.header.height = i;
        block.header.timestamp = i * 120;
        block.header.difficulty = 10;

        if (i > 0)
            block.header.previous_hash =
                long_chain.back().hash();

        long_chain.push_back(block);
    }

    // Keep the history immediately before block 12 fast.
    // Then make only the later tip history slow.
    for (std::size_t i = 13; i < long_chain.size(); ++i)
        long_chain[i].header.timestamp =
            long_chain[i - 1].header.timestamp + 240;

    ok &= check(
        "Historical block uses its own difficulty window",
        validate_block_position_with_chain(
            long_chain, 12));

    std::cout << "Overall: "
              << (ok ? "PASS" : "FAIL")
              << '\n';

    return ok ? 0 : 1;
}
