#include <iostream>
#include <vector>
#include <caesar/block.hpp>
#include <caesar/block_builder.hpp>
#include <caesar/mempool.hpp>
#include <caesar/utxo.hpp>

using namespace caesar;

int main() {
    std::cout << "START\n" << std::flush;

    Block genesis;
    genesis.header.version = 1;
    genesis.header.height = 0;
    genesis.header.timestamp = 0;
    genesis.header.nonce = 0;
    genesis.header.difficulty = 0;

    Transaction marker;
    marker.version = 1;

    TransactionOutput output;
    output.amount = 1;
    output.recipient = "CAESAR_GENESIS_BURN";

    marker.outputs.push_back(output);
    genesis.transactions.push_back(marker);
    genesis.update_merkle_root();

    std::vector<Block> chain;
    chain.push_back(genesis);

    Mempool mempool;

    Block original = BlockBuilder::build(
        genesis,
        mempool,
        "CAESAR_SYNC_TEST_MINER",
        1000000,
        CZR_INITIAL_MINING_DIFFICULTY);

    std::cout << "BUILT\n";

    if (!BlockBuilder::mine(
            original,
            0,
            1000000)) {
        std::cout << "MINE_FAILED\n";
        return 2;
    }

    std::cout << "MINED\n";
    std::cout << "ORIGINAL_SIZE="
              << original.serialize_full_binary().size()
              << "\n";

    const auto encoded =
        original.serialize_full_binary();

    Block decoded =
        Block::deserialize_full(encoded);

    std::cout << "DECODED\n";

    std::cout << "ORIGINAL_HASH="
              << hash_to_hex(original.hash())
              << "\n";

    std::cout << "DECODED_HASH="
              << hash_to_hex(decoded.hash())
              << "\n";

    std::cout << "ORIGINAL_POW="
              << (original.validate_pow() ? "OK" : "FAIL")
              << "\n";

    std::cout << "DECODED_POW="
              << (decoded.validate_pow() ? "OK" : "FAIL")
              << "\n";

    std::cout << "ORIGINAL_BASIC="
              << (original.validate_basic() ? "OK" : "FAIL")
              << "\n";

    std::cout << "DECODED_BASIC="
              << (decoded.validate_basic() ? "OK" : "FAIL")
              << "\n";

    std::cout << "DECODED_LINK="
              << (validate_block_link(
                      genesis,
                      decoded)
                      ? "OK"
                      : "FAIL")
              << "\n";

    UTXOSet previous_utxos;

    try {
        auto next_utxos =
            apply_block_transactions(
                decoded,
                previous_utxos);

        (void)next_utxos;

        std::cout << "DECODED_UTXO=OK\n";
    }
    catch (const std::exception& e) {
        std::cout << "DECODED_UTXO=FAIL\n";
        std::cout << "DECODED_UTXO_ERROR="
                  << e.what()
                  << "\n";
    }

    std::cout << "DECODED_ISSUANCE="
              << (validate_total_coinbase_issuance(
                      decoded,
                      chain)
                      ? "OK"
                      : "FAIL")
              << "\n";

    std::cout << "DECODED_CONSENSUS="
              << (validate_block_consensus(
                      decoded,
                      chain,
                      previous_utxos)
                      ? "OK"
                      : "FAIL")
              << "\n";

    std::cout << "END\n";
}
