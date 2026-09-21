#include <iostream>
#include <vector>
#include <caesar/block.hpp>
#include <caesar/block_builder.hpp>
#include <caesar/mempool.hpp>
#include <caesar/utxo.hpp>
#include <caesar/transaction.hpp>

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

    const std::uint32_t difficulty =
        CZR_INITIAL_MINING_DIFFICULTY;

    Block block = BlockBuilder::build(
        genesis,
        mempool,
        "CAESAR_SYNC_TEST_MINER",
        1000000,
        difficulty);

    std::cout << "BLOCK_BUILT\n" << std::flush;

    if (!BlockBuilder::mine(block, 0, 1000000)) {
        std::cout << "MINE_FAILED\n";
        return 2;
    }

    std::cout << "BLOCK_MINED\n";
    std::cout << "HEIGHT=" << block.header.height << "\n";
    std::cout << "DIFFICULTY=" << block.header.difficulty << "\n";

    std::cout << "POW="
              << (block.validate_pow() ? "OK" : "FAIL")
              << "\n";

    std::cout << "BASIC="
              << (block.validate_basic() ? "OK" : "FAIL")
              << "\n";

    std::cout << "LINK="
              << (validate_block_link(genesis, block) ? "OK" : "FAIL")
              << "\n";

    UTXOSet previous_utxos;

    try {
        auto next_utxos =
            apply_block_transactions(
                block,
                previous_utxos);

        (void)next_utxos;
        std::cout << "UTXO=OK\n";
    } catch (const std::exception& e) {
        std::cout << "UTXO=FAIL\n";
        std::cout << "UTXO_ERROR="
                  << e.what()
                  << "\n";
    }

    std::cout << "ISSUANCE="
              << (validate_total_coinbase_issuance(
                      block,
                      chain)
                      ? "OK"
                      : "FAIL")
              << "\n";

    std::cout << "CONSENSUS="
              << (validate_block_consensus(
                      block,
                      chain,
                      previous_utxos)
                      ? "OK"
                      : "FAIL")
              << "\n";

    std::cout << "END\n";
}
