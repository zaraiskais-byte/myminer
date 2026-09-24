#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/chain_replacement.hpp>
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

std::vector<caesar::Block> make_chain(
    std::initializer_list<std::uint32_t> difficulties) {

    std::vector<caesar::Block> chain;
    chain.reserve(difficulties.size());

    std::uint64_t height = 0;

    for (const auto difficulty : difficulties) {
        chain.push_back(make_block(height++, difficulty));
    }

    return chain;
}

} // namespace

int main() {
    /*
     * Chain replacement must select by cumulative proof-of-work,
     * never by height alone.
     */

    // Equal-work candidate: must not replace the current chain.
    {
        const auto current = make_chain({0, 12, 12});
        const auto candidate = make_chain({0, 12, 12});

        assert(!caesar::has_more_work(candidate, current));
        assert(!caesar::prepare_chain_replacement(
            current,
            candidate));
    }

    // A longer chain can still have less cumulative work.
    {
        const auto current = make_chain({0, 14, 14});
        const auto candidate = make_chain({0, 12, 12, 12, 12});

        assert(candidate.size() > current.size());
        assert(!caesar::has_more_work(candidate, current));
        assert(!caesar::prepare_chain_replacement(
            current,
            candidate));
    }

    // A shorter chain with greater cumulative work is eligible
    // at the chain-selection layer.
    {
        const auto current = make_chain({0, 12, 12, 12});
        const auto candidate = make_chain({0, 14, 14});

        assert(candidate.size() < current.size());
        assert(caesar::has_more_work(candidate, current));
    }

    std::cout
        << "Equal-work replacement rejected: PASS\n"
        << "Longer lower-work replacement rejected: PASS\n"
        << "Shorter higher-work chain recognized: PASS\n"
        << "CHAIN REPLACEMENT REJECTION TEST PASSED\n";

    return 0;
}
