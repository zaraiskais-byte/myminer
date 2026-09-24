#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include <caesar/chain_replacement.hpp>

namespace {

caesar::Block make_block(std::uint64_t height) {
    caesar::Block block;
    block.header.version = 1;
    block.header.height = height;
    block.header.difficulty = 0;
    return block;
}

} // namespace

int main() {
    std::vector<caesar::Block> canonical;
    canonical.push_back(make_block(0));
    canonical.push_back(make_block(1));

    caesar::UTXOSet canonical_utxo;

    caesar::ChainReplacementPlan plan;

    plan.chain.push_back(make_block(0));
    plan.chain.push_back(make_block(1));
    plan.chain.push_back(make_block(2));

    // Give the prepared state a distinct UTXO count.
    const caesar::OutPoint marker{
        caesar::Hash256{},
        7
    };

    caesar::TransactionOutput output;
    output.amount = 123;

    assert(plan.utxo.add(marker, output));
    assert(plan.utxo.size() == 1);

    caesar::commit_chain_replacement(
        canonical,
        canonical_utxo,
        std::move(plan));

    // Both pieces of canonical state changed together.
    assert(canonical.size() == 3);
    assert(canonical[2].header.height == 2);
    assert(canonical_utxo.size() == 1);
    assert(canonical_utxo.contains(marker));

    std::cout
        << "Canonical chain replaced atomically: PASS\n"
        << "Canonical UTXO replaced with matching prepared state: PASS\n"
        << "CHAIN REPLACEMENT TEST PASSED\n";

    return 0;
}
