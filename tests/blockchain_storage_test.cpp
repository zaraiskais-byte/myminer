#include <cassert>
#include <filesystem>

#include <caesar/block_builder.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/mempool.hpp>

namespace {

caesar::Block make_genesis() {
    caesar::Block genesis;

    genesis.header.version = 1;
    genesis.header.height = 0;
    genesis.header.previous_hash = {};
    genesis.header.timestamp = 0;
    genesis.header.nonce = 0;
    genesis.header.difficulty = 0;

    caesar::Transaction tx;
    tx.outputs.push_back(
        caesar::TransactionOutput{
            1,
            "CAESAR_GENESIS_BURN"
        });

    genesis.transactions.push_back(tx);
    genesis.update_merkle_root();

    return genesis;
}

}

int main() {
    const auto path =
        std::filesystem::temp_directory_path() /
        "caesar_blockchain_storage_test.dat";

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(
        path.string() + ".tmp",
        ec);

    caesar::BlockchainStorage storage(path);

    const auto genesis = make_genesis();

    storage.save({genesis});

    auto chain = storage.load();

    assert(chain.size() == 1);
    assert(chain.front().header.height == 0);
    assert(caesar::validate_block_chain(chain));

    caesar::Mempool mempool;

    auto block1 =
        caesar::BlockBuilder::build(
            chain.back(),
            mempool,
            "CAESAR_TEST_MINER",
            1,
            caesar::CZR_INITIAL_MINING_DIFFICULTY);

    assert(
        block1.header.difficulty ==
        caesar::CZR_INITIAL_MINING_DIFFICULTY);

    const bool mined = caesar::BlockBuilder::mine(
        block1,
        0,
        1000000);

    if (!mined)
        throw std::runtime_error(
            "failed to mine storage test block");

    if (!block1.validate_pow())
        throw std::runtime_error(
            "storage test block has invalid PoW after mining");

    const caesar::UTXOSet previous_utxos =
        caesar::rebuild_utxo_set(chain);

    assert(
        caesar::validate_block_consensus(
            block1,
            chain,
            previous_utxos));

    assert(block1.validate_pow());

    storage.append(block1);

    chain = storage.load();

    assert(chain.size() == 2);
    assert(chain.back().header.height == 1);
    assert(
        chain.back().header.previous_hash ==
        chain[0].hash());
    assert(caesar::validate_block_chain(chain));

    // Full-chain replacement uses the same atomic write path.
    const auto canonical_before_replace = chain;

    storage.replace(chain);

    const auto replaced = storage.load();

    assert(replaced.size() == canonical_before_replace.size());
    assert(replaced.back().hash() ==
           canonical_before_replace.back().hash());

    // An invalid replacement must fail before touching canonical storage.
    auto invalid_replacement = chain;
    invalid_replacement[1].header.previous_hash = {};

    bool rejected = false;

    try {
        storage.replace(invalid_replacement);
    }
    catch (const std::exception&) {
        rejected = true;
    }

    assert(rejected);

    const auto after_rejected_replace = storage.load();

    assert(after_rejected_replace.size() ==
           canonical_before_replace.size());

    for (std::size_t i = 0;
         i < after_rejected_replace.size();
         ++i) {
        assert(after_rejected_replace[i].hash() ==
               canonical_before_replace[i].hash());
    }

    std::filesystem::remove(path, ec);
    std::filesystem::remove(
        path.string() + ".tmp",
        ec);

    return 0;
}
