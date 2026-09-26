#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <caesar/block_builder.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/chain_validator.hpp>
#include <caesar/coinbase.hpp>
#include <caesar/consensus.hpp>
#include <caesar/mempool.hpp>
#include <caesar/transaction.hpp>

using namespace caesar;

namespace {

Block make_genesis() {
    Block genesis;
    genesis.header.version = 1;
    genesis.header.height = 0;
    genesis.header.previous_hash = {};
    genesis.header.timestamp = 0;
    genesis.header.nonce = 0;
    genesis.header.difficulty = 0;

    Transaction tx;
    tx.outputs.push_back(
        TransactionOutput{1, "CAESAR_GENESIS_BURN"});
    genesis.transactions.push_back(tx);
    genesis.update_merkle_root();
    return genesis;
}

Block mine_next_block(
    const std::vector<Block>& chain,
    const Mempool& mempool,
    std::uint64_t ts) {

    const std::uint32_t difficulty =
        expected_next_difficulty(chain);

    Block block = BlockBuilder::build(
        chain.back(), mempool, "MINER",
        ts, difficulty, 0, nullptr);

    if (!BlockBuilder::mine(block, 0, 100000000))
        throw std::runtime_error("mining failed");
    return block;
}

} // namespace

int main() {

    const auto path =
        std::filesystem::temp_directory_path() /
        "caesar_storage_utxo_rejection_test.dat";

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.string() + ".tmp", ec);

    std::cout << "[utxo-reject] building chain...\n";

    std::vector<Block> chain;
    chain.push_back(make_genesis());

    Mempool mempool;

    for (int i = 1; i <= 11; ++i) {
        chain.push_back(mine_next_block(
            chain, mempool,
            static_cast<std::uint64_t>(i) * 120));
    }

    assert(chain.size() == 12);
    std::cout << "[utxo-reject] chain height="
              << chain.back().header.height << "\n";

    BlockchainStorage storage(path);
    storage.save(chain);

    const std::uint32_t next_difficulty =
        expected_next_difficulty(chain);

    Block bad_block = BlockBuilder::build(
        chain.back(), mempool, "MINER",
        12 * 120, next_difficulty, 0, nullptr);

    Transaction bad_tx;
    bad_tx.version = 1;

    TransactionInput bad_input;
    for (auto& b : bad_input.previous_txid) b = 0xAA;
    bad_input.output_index = 0;
    bad_tx.inputs.push_back(bad_input);

    TransactionOutput bad_out;
    bad_out.amount = 1;
    bad_out.recipient = "ATTACKER";
    bad_tx.outputs.push_back(bad_out);

    // Deliberately no witness; witness-count check will fail in consensus.
    bad_block.transactions.push_back(bad_tx);
    bad_block.update_merkle_root();

    if (!BlockBuilder::mine(bad_block, 0, 100000000))
        throw std::runtime_error("mining bad block failed");

    // --- Test 1: weak path accepts ---
    const bool weak_accepts =
        bad_block.validate_against_chain(chain);
    std::cout << "[utxo-reject] validate_against_chain = "
              << (weak_accepts ? "TRUE" : "FALSE") << "\n";
    assert(weak_accepts && "weak path should accept");

    // --- Test 2: strong path rejects ---
    const UTXOSet utxos = rebuild_utxo_set(chain);
    const bool strong_accepts =
        validate_block_consensus(bad_block, chain, utxos);
    std::cout << "[utxo-reject] validate_block_consensus = "
              << (strong_accepts ? "TRUE" : "FALSE") << "\n";
    assert(!strong_accepts && "strong path must reject");

    // --- Test 3: storage.append rejects (proves the fix) ---
    bool rejected = false;
    try {
        storage.append(bad_block);
    } catch (const std::exception& e) {
        std::cout << "[utxo-reject] append threw: "
                  << e.what() << "\n";
        rejected = true;
    }
    assert(rejected && "storage.append must reject");

    // --- Test 4: storage unchanged ---
    const auto after = storage.load();
    assert(after.size() == chain.size());
    assert(after.back().hash() == chain.back().hash());
    std::cout << "[utxo-reject] storage height unchanged="
              << (after.size() - 1) << "\n";

    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.string() + ".tmp", ec);

    std::cout << "CaesarStorageUtxoRejectionTest: PASS\n";
    return 0;
}
