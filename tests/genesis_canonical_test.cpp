/*
 * Canonical genesis test.
 *
 * Until now the genesis block was constructed inline in
 * CaesarNode::ensure_chain() and validated only structurally. Nothing
 * tied it to a specific protocol identity: any node could invent its
 * own genesis and start a chain that looked valid to the structural
 * checks.
 *
 * network_params.hpp introduces build_canonical_genesis() as the
 * single source of truth, and canonical_genesis_hash() as the anchor
 * that future Mainnet/TESTNET nodes will use.
 *
 * This test verifies:
 *   1. build_canonical_genesis() is deterministic (same hash across
 *      calls).
 *   2. The resulting block passes validate_genesis_canonical().
 *   3. The genesis contains the canonical burn output and nothing
 *      else.
 *   4. The hash bytes are printed so they can be recorded as the
 *      MAINNET / TESTNET anchor constant in a follow-up commit.
 */

#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include <caesar/block.hpp>
#include <caesar/chain_validator.hpp>
#include <caesar/network_params.hpp>

using namespace caesar;

int main() {
    // 1. Determinism.
    const Block g1 = build_canonical_genesis();
    const Block g2 = build_canonical_genesis();
    assert(g1.hash() == g2.hash());

    const Block g3 = build_canonical_genesis();
    assert(g1.hash() == g3.hash());

    // 2. Structural validity.
    assert(validate_genesis_canonical(g1));

    // 3. Canonical content.
    assert(g1.header.version == 1);
    assert(g1.header.height == 0);
    assert(g1.header.previous_hash == Hash256{});
    assert(g1.header.timestamp == 0);
    assert(g1.header.nonce == 0);
    assert(g1.header.difficulty == 0);
    assert(g1.transactions.size() == 1);

    const Transaction& tx = g1.transactions.front();
    assert(tx.inputs.empty());
    assert(tx.witness.empty());
    assert(tx.outputs.size() == 1);
    assert(tx.outputs.front().amount == 1);
    assert(tx.outputs.front().recipient == GENESIS_BURN_RECIPIENT);

    // 4. Print the hash so it can be hardcoded later.
    const Hash256 h = g1.hash();
    std::cout << "[genesis] canonical hash = ";
    for (std::uint8_t b : h) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    }
    std::cout << std::dec << "\n";

    // 5. Network constants are defined and distinct.
    static_assert(NETWORK_MAINNET != NETWORK_TESTNET, "network ids must differ");
    assert(NETWORK_MAINNET == 1);
    assert(NETWORK_TESTNET == 2);

    // 6. canonical_genesis_hash() agrees with build.
    assert(canonical_genesis_hash() == g1.hash());

    // 7. The pinned Mainnet hash matches the current build.
    assert(g1.hash() == GENESIS_HASH_MAINNET);

    // 8. genesis_hash_for_network() returns the right pointer.
    assert(genesis_hash_for_network(NETWORK_MAINNET) != nullptr);
    assert(*genesis_hash_for_network(NETWORK_MAINNET) == g1.hash());
    assert(genesis_hash_for_network(NETWORK_TESTNET) != nullptr);
    assert(*genesis_hash_for_network(NETWORK_TESTNET) == GENESIS_HASH_TESTNET);

    // 9. validate_genesis_network_identity accepts correct hash,
    //    rejects wrong hash, and passes through when unpinned.
    assert(validate_genesis_network_identity(g1, nullptr));
    assert(validate_genesis_network_identity(g1, &GENESIS_HASH_MAINNET));

    // Build a fake genesis with a different recipient.
    Block fake = g1;
    fake.transactions.front().outputs.front().recipient = "FAKE_GENESIS";
    fake.update_merkle_root();
    assert(fake.hash() != GENESIS_HASH_MAINNET);
    assert(!validate_genesis_network_identity(fake, &GENESIS_HASH_MAINNET));

    // 10. Testnet genesis is structurally valid and distinct from
    //     the Mainnet one. The hash is printed so it can be pinned
    //     as GENESIS_HASH_TESTNET in a follow-up commit.
    {
        const Block t = build_testnet_genesis();

        assert(validate_genesis_canonical(t));
        assert(t.hash() != g1.hash());
        assert(t.hash() != GENESIS_HASH_MAINNET);
        assert(t.transactions.size() == 1);
        assert(t.transactions.front().outputs.size() == 1);
        assert(t.transactions.front().outputs.front().recipient == TESTNET_BURN_RECIPIENT);

        std::cout << "[genesis] testnet hash = ";
        for (std::uint8_t b : t.hash()) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
        }
        std::cout << std::dec << "\n";

        assert(testnet_genesis_hash() == t.hash());
        assert(t.hash() == GENESIS_HASH_TESTNET);
    }

    std::cout << "CaesarGenesisCanonicalTest: PASS\n";
    return 0;
}
