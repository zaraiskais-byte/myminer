#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/chain_work.hpp>

namespace {

caesar::Block make_block(
    std::uint64_t height,
    std::uint32_t difficulty) {

    caesar::Block block;
    block.header.version = 1;
    block.header.height = height;
    block.header.difficulty = difficulty;
    return block;
}

}

int main() {
    /*
     * This test intentionally does NOT mine.
     *
     * PoW/consensus validity is tested separately by
     * CaesarCandidateChainValidationTest.
     *
     * This test isolates the chain-selection rule:
     * cumulative proof-of-work, not height.
     */

    std::vector<caesar::Block> chain_a;
    chain_a.push_back(make_block(0, 0));
    chain_a.push_back(make_block(1, 12));
    chain_a.push_back(make_block(2, 12));
    chain_a.push_back(make_block(3, 12));

    std::vector<caesar::Block> chain_b;
    chain_b.push_back(make_block(0, 0));
    chain_b.push_back(make_block(1, 14));
    chain_b.push_back(make_block(2, 14));

    const auto work_a =
        caesar::cumulative_chain_work(chain_a);

    const auto work_b =
        caesar::cumulative_chain_work(chain_b);

    assert(chain_a.size() > chain_b.size());
    assert(work_b > work_a);

    assert(caesar::has_more_work(
        chain_b,
        chain_a));

    assert(!caesar::has_more_work(
        chain_a,
        chain_b));

    assert(!caesar::has_more_work(
        chain_a,
        chain_a));

    std::cout
        << "Longer chain A rejected by work comparison: PASS\n"
        << "Higher-work chain B selected: PASS\n"
        << "CHAINWORK SELECTION TEST PASSED\n";

    return 0;
}
