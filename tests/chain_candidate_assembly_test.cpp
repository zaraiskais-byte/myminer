#include <caesar/chain_replacement.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

caesar::Block make_block(
    std::uint64_t height,
    const caesar::Hash256& previous,
    std::uint8_t marker) {

    caesar::Block block;
    block.header.version = 1;
    block.header.height = height;
    block.header.previous_hash = previous;
    block.header.timestamp = height;
    block.header.difficulty = 0;
    block.header.nonce = marker;

    block.header.merkle_root.fill(marker);
    block.header.witness_root.fill(marker);

    return block;
}

} // namespace

int main() {
    using namespace caesar;

    Block genesis;
    genesis.header.version = 1;
    genesis.header.height = 0;
    genesis.header.previous_hash.fill(0);
    genesis.header.timestamp = 0;
    genesis.header.difficulty = 0;
    genesis.header.nonce = 1;
    genesis.header.merkle_root.fill(1);
    genesis.header.witness_root.fill(1);

    Block a1 = make_block(1, genesis.hash(), 2);
    Block a2 = make_block(2, a1.hash(), 3);

    std::vector<Block> current{genesis, a1, a2};

    // Fork begins after genesis.
    Block b1 = make_block(1, genesis.hash(), 4);
    Block b2 = make_block(2, b1.hash(), 5);

    std::vector<BlockHeader> headers{
        b1.header,
        b2.header
    };

    std::vector<Block> blocks{
        b1,
        b2
    };

    const auto candidate =
        assemble_candidate_chain(current, headers, blocks);

    assert(candidate.has_value());
    assert(candidate->size() == 3);
    assert((*candidate)[0].hash() == genesis.hash());
    assert((*candidate)[1].hash() == b1.hash());
    assert((*candidate)[2].hash() == b2.hash());

    // Wrong block order/hash must be rejected.
    std::vector<Block> wrong_blocks{
        b2,
        b1
    };

    assert(
        !assemble_candidate_chain(
            current,
            headers,
            wrong_blocks));

    // A fork whose first header does not point into the local chain
    // must also be rejected.
    Block foreign_parent = make_block(99, Hash256{}, 9);
    Block foreign_child = make_block(
        100,
        foreign_parent.hash(),
        10);

    assert(
        !assemble_candidate_chain(
            current,
            {foreign_child.header},
            {foreign_child}));

    std::cout << "chain candidate assembly test passed\n";
    return 0;
}
