#pragma once

#include <cstdint>
#include <string>

#include <caesar/block.hpp>

namespace caesar {

/*
 * Protocol-level network identifiers.
 *
 * These are carried in the P2P hello handshake (P2PHello::network_id)
 * and identify which chain a peer expects to be on. Two nodes on
 * different networks must reject each other at the handshake stage.
 */
inline constexpr std::uint32_t NETWORK_MAINNET = 1;
inline constexpr std::uint32_t NETWORK_TESTNET = 2;

/*
 * The recipient string embedded in the genesis coinbase transaction.
 *
 * It is part of the protocol-visible identity of the chain: any
 * change to it produces a different genesis block and therefore a
 * different genesis hash. Do not change it on a live network.
 */
inline constexpr const char* GENESIS_BURN_RECIPIENT =
    "CAESAR_GENESIS_BURN";

/*
 * The recipient string for the Testnet genesis. Deliberately different
 * from Mainnet's so the two chains have different genesis hashes even
 * though every other genesis field is identical. Do not change either
 * recipient on a live network: changing genesis is a hard fork.
 */
inline constexpr const char* TESTNET_BURN_RECIPIENT =
    "CAESAR_TESTNET_GENESIS_BURN";

/*
 * Builds the canonical genesis block for Caesar CZR.
 *
 * This function is the single source of truth for how the genesis
 * block is constructed. It replaces duplicated genesis construction
 * that previously lived inline in CaesarNode::ensure_chain().
 *
 * Construction rules:
 *   - version       = 1
 *   - height        = 0
 *   - previous_hash = all zeros
 *   - timestamp     = 0
 *   - nonce         = 0
 *   - difficulty    = 0
 *   - exactly one transaction:
 *       * no inputs
 *       * one output of 1 unit to GENESIS_BURN_RECIPIENT
 *       * no witness
 *   - merkle and witness roots are updated from the transaction set
 *
 * The block is intentionally cheap to compute and performs no proof
 * of work, since the genesis is validated structurally rather than
 * by PoW.
 */
inline Block build_canonical_genesis(
    const std::string& burn_recipient) {

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
            burn_recipient
        });

    genesis.transactions.push_back(tx);
    genesis.update_merkle_root();

    return genesis;
}

/*
 * Canonical genesis block for the Mainnet.
 * Uses the Mainnet burn recipient. Kept as a no-arg overload so
 * existing callers continue to work unchanged.
 */
inline Block build_canonical_genesis() {
    return build_canonical_genesis(GENESIS_BURN_RECIPIENT);
}

/*
 * Canonical genesis block for the Testnet.
 *
 * Structurally identical to Mainnet except for the burn recipient,
 * which produces a distinct genesis hash. This is what makes the two
 * chains incompatible: a Mainnet node rejects a Testnet chain at the
 * genesis-identity check and vice versa.
 */
inline Block build_testnet_genesis() {
    return build_canonical_genesis(TESTNET_BURN_RECIPIENT);
}

/*
 * Returns the hash of the canonical genesis block.
 *
 * This value is deterministic: every build of Caesar CZR produces
 * the same hash as long as build_canonical_genesis() is not changed.
 * It is the anchor that future Mainnet/TESTNET nodes will use to
 * verify that a peer's chain starts from the same protocol identity.
 */
inline Hash256 canonical_genesis_hash() {
    return build_canonical_genesis().hash();
}

/*
 * Returns the hash of the canonical Testnet genesis block.
 * Deterministic, like canonical_genesis_hash(), and distinct from it
 * because the burn recipient differs.
 */
inline Hash256 testnet_genesis_hash() {
    return build_testnet_genesis().hash();
}


/*
 * Canonical genesis hash for Mainnet.
 *
 * Computed once from build_canonical_genesis().hash() and pinned here.
 * Any Mainnet node MUST accept only chains whose first block hashes to
 * this value. If build_canonical_genesis() is ever changed on a live
 * network, this constant will no longer match and every node will
 * reject the new chain, which is the intended behaviour: changing
 * genesis is a hard fork.
 *
 * Source of the bytes: printed by CaesarGenesisCanonicalTest in the
 * commit that introduced this constant:
 *
 *   7e026bb394aff5047e026130bfe0a14d6fbaa971691be66305715f48f612a5bc
 */
inline const Hash256 GENESIS_HASH_MAINNET = {
    0x7e, 0x02, 0x6b, 0xb3, 0x94, 0xaf, 0xf5, 0x04,
    0x7e, 0x02, 0x61, 0x30, 0xbf, 0xe0, 0xa1, 0x4d,
    0x6f, 0xba, 0xa9, 0x71, 0x69, 0x1b, 0xe6, 0x63,
    0x05, 0x71, 0x5f, 0x48, 0xf6, 0x12, 0xa5, 0xbc
};

/*
 * Canonical genesis hash for the Testnet.
 *
 * Computed once from build_testnet_genesis().hash() and pinned here.
 * Distinct from GENESIS_HASH_MAINNET because the burn recipient is
 * different. A Mainnet node and a Testnet node will reject each
 * other's chains at the genesis-identity check.
 *
 * Source of the bytes: printed by CaesarGenesisCanonicalTest in the
 * commit that introduced this constant:
 *
 *   ba67ed0363858fa4505fd0f6ffea1362f6e79b59ed4d0dd93323e4b549e38ec3
 */
inline const Hash256 GENESIS_HASH_TESTNET = {
    0xba, 0x67, 0xed, 0x03, 0x63, 0x85, 0x8f, 0xa4,
    0x50, 0x5f, 0xd0, 0xf6, 0xff, 0xea, 0x13, 0x62,
    0xf6, 0xe7, 0x9b, 0x59, 0xed, 0x4d, 0x0d, 0xd9,
    0x33, 0x23, 0xe4, 0xb5, 0x49, 0xe3, 0x8e, 0xc3
};

/*
 * Returns a pointer to the pinned genesis hash for a network, or
 * nullptr if the network has no pinned genesis yet.
 *
 * TESTNET is currently not defined, so it returns nullptr. This is
 * deliberate: nodes without a pinned genesis behave exactly as
 * before, so existing tests and internal chains keep working. Mainnet
 * nodes pass GENESIS_HASH_MAINNET explicitly and reject any chain
 * whose first block does not match.
 */
inline const Hash256* genesis_hash_for_network(
    std::uint32_t network) noexcept {

    if (network == NETWORK_MAINNET)
        return &GENESIS_HASH_MAINNET;

    if (network == NETWORK_TESTNET)
        return &GENESIS_HASH_TESTNET;

    return nullptr;
}

/*
 * Network-identity check for an incoming chain.
 *
 * Distinct from validate_genesis_canonical(), which verifies the
 * structural shape of a genesis block (height zero, zero previous
 * hash, difficulty zero, basic validity). This function verifies that
 * the genesis is the SPECIFIC genesis expected by the network we are
 * running on. It is the check that prevents a peer from feeding us an
 * entirely different chain that happens to be structurally valid.
 *
 * Returns true when expected is nullptr (no network pinned yet) so
 * callers can treat the check as opt-in during testnet rollout.
 */
inline bool validate_genesis_network_identity(
    const Block& genesis,
    const Hash256* expected) noexcept {

    if (expected == nullptr)
        return true;

    return genesis.hash() == *expected;
}

} // namespace caesar
