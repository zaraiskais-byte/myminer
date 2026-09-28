/*
 * Reorg work-selection test.
 *
 * The existing rejection test already covers two basic rules:
 *   - equal work does not replace
 *   - a longer chain with less cumulative work does not replace
 *
 * What it does not cover:
 *   - exact chainwork arithmetic (2^difficulty per block)
 *   - a SHORTER fork with HIGHER work actually winning
 *   - sequential reorgs A -> B -> C, with the invariant that the
 *     work ordering is a strict total order across candidates
 *   - empty-chain edge cases in has_more_work
 *   - atomic commit preserving the genesis and swapping UTXO
 *
 * This test uses synthetic blocks with explicit difficulty, so it
 * runs in well under a second and does not require mining. It works
 * purely at the selection layer: has_more_work, cumulative_chain_work,
 * and commit_chain_replacement.
 */

#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include <caesar/chain_replacement.hpp>
#include <caesar/chain_work.hpp>

using namespace caesar;

namespace {

Block make_block(std::uint64_t height, std::uint32_t difficulty) {
    Block b;
    b.header.version = 1;
    b.header.height = height;
    b.header.difficulty = difficulty;
    return b;
}

std::vector<Block> make_chain(
    std::initializer_list<std::uint32_t> difficulties) {

    std::vector<Block> chain;
    chain.reserve(difficulties.size());

    std::uint64_t height = 0;
    for (auto d : difficulties)
        chain.push_back(make_block(height++, d));

    return chain;
}

} // namespace

int main() {

    // -----------------------------------------------------------------
    // 1. Exact chainwork arithmetic: each block contributes 2^d.
    // -----------------------------------------------------------------
    {
        // Single block at difficulty 0: 2^0 = 1.
        const auto c = make_chain({0});
        assert(cumulative_chain_work(c) == ChainWork(1));
    }
    {
        // 2^0 + 2^1 = 3.
        const auto c = make_chain({0, 1});
        assert(cumulative_chain_work(c) == ChainWork(3));
    }
    {
        // 2^10 * 3 = 3072.
        const auto c = make_chain({10, 10, 10});
        assert(cumulative_chain_work(c) == ChainWork(3072));
    }
    {
        // 2^0 + 2^10 = 1025.
        const auto c = make_chain({0, 10});
        assert(cumulative_chain_work(c) == ChainWork(1025));
    }
    std::cout << "[reorg-work] chainwork arithmetic: OK\n";

    // -----------------------------------------------------------------
    // 2. Shorter fork with higher work wins.
    // -----------------------------------------------------------------
    {
        // Canonical: 4 blocks at diff 12 -> 4 * 4096 = 16384.
        const auto canonical = make_chain({12, 12, 12, 12});

        // Fork: 3 blocks at diff 13 -> 3 * 8192 = 24576.
        const auto fork = make_chain({13, 13, 13});

        assert(fork.size() < canonical.size());
        assert(has_more_work(fork, canonical));
        assert(!has_more_work(canonical, fork));
    }
    std::cout << "[reorg-work] shorter-higher-work fork: OK\n";

    // -----------------------------------------------------------------
    // 3. Sequential reorgs form a strict total order on work.
    // -----------------------------------------------------------------
    {
        const auto a = make_chain({10, 10});       // 2048
        const auto b = make_chain({11, 11});       // 4096
        const auto c = make_chain({11, 11, 10});   // 5120

        assert(has_more_work(b, a));
        assert(has_more_work(c, b));
        assert(has_more_work(c, a));

        assert(!has_more_work(a, b));
        assert(!has_more_work(a, c));
        assert(!has_more_work(b, c));
    }
    std::cout << "[reorg-work] sequential reorgs: OK\n";

    // -----------------------------------------------------------------
    // 4. Equal work never replaces, in either direction.
    // -----------------------------------------------------------------
    {
        const auto a = make_chain({12, 12, 12});
        const auto b = make_chain({12, 12, 12});

        assert(!has_more_work(a, b));
        assert(!has_more_work(b, a));
    }
    std::cout << "[reorg-work] equal-work no-replace: OK\n";

    // -----------------------------------------------------------------
    // 5. Empty chain edge cases.
    // -----------------------------------------------------------------
    {
        const auto nonempty = make_chain({10});
        const std::vector<Block> empty;

        // Any non-empty candidate beats an empty current.
        assert(has_more_work(nonempty, empty));

        // An empty candidate never wins.
        assert(!has_more_work(empty, nonempty));
        assert(!has_more_work(empty, empty));
    }
    std::cout << "[reorg-work] empty chain edges: OK\n";

    // -----------------------------------------------------------------
    // 6. Atomic commit: chain + UTXO swap together, genesis preserved.
    // -----------------------------------------------------------------
    {
        // Canonical: genesis + one block.
        std::vector<Block> canonical;
        canonical.push_back(make_block(0, 0));
        canonical.push_back(make_block(1, 10));
        const Hash256 genesis_before = canonical.front().hash();

        UTXOSet canonical_utxo;

        // Prepare a plan that extends to three blocks.
        ChainReplacementPlan plan;
        plan.chain = make_chain({0, 10, 10});

        // Distinct marker so we can detect the swap.
        const OutPoint marker{Hash256{}, 42};
        TransactionOutput out;
        out.amount = 999;
        assert(plan.utxo.add(marker, out));

        commit_chain_replacement(
            canonical, canonical_utxo, std::move(plan));

        assert(canonical.size() == 3);
        assert(canonical.front().hash() == genesis_before);
        assert(canonical_utxo.size() == 1);
        assert(canonical_utxo.contains(marker));
    }
    std::cout
        << "[reorg-work] atomic commit + genesis preserved: OK\n";

    std::cout << "CaesarReorgWorkSelectionTest: PASS\n";
    return 0;
}
