#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <caesar/block_builder.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/mempool.hpp>

int main(int argc, char** argv) {
    try {
        bool mine_one_block = false;

        if (argc > 1) {
            if (std::string(argv[1]) == "--mine") {
                mine_one_block = true;
            } else {
                std::cerr
                    << "Usage: caesard [--mine]\n";
                return 2;
            }
        }

        std::cout << "=== Caesar CZR Node ===\n";

        const std::filesystem::path data_dir =
            "data";

        std::filesystem::create_directories(
            data_dir);

        const std::filesystem::path chain_file =
            data_dir / "blockchain.dat";

        caesar::BlockchainStorage storage(
            chain_file);

        if (!storage.exists()) {
            std::cout
                << "Blockchain storage: NEW\n";

            caesar::Block genesis;
            genesis.header.version = 1;
            genesis.header.height = 0;
            genesis.header.previous_hash = {};
            genesis.header.timestamp = 0;
            genesis.header.nonce = 0;
            genesis.header.difficulty = 0;

            caesar::Transaction genesis_tx;
            genesis_tx.outputs.push_back(
                caesar::TransactionOutput{
                    1,
                    "CAESAR_GENESIS_BURN"
                });

            genesis.transactions.push_back(
                genesis_tx);

            genesis.update_merkle_root();

            storage.save({genesis});

            std::cout
                << "Genesis created and saved.\n";
        }

        auto chain = storage.load();

        std::cout
            << "Blockchain blocks: "
            << chain.size()
            << "\n";

        std::cout
            << "Chain validation: "
            << (caesar::validate_block_chain(chain)
                    ? "PASS"
                    : "FAIL")
            << "\n";

        if (mine_one_block) {
            if (chain.empty())
                throw std::runtime_error(
                    "cannot mine on empty chain");

            const caesar::Block& previous =
                chain.back();

            const caesar::UTXOSet previous_utxos =
                caesar::rebuild_utxo_set(chain);

            caesar::Mempool mempool;

            auto now =
                std::chrono::duration_cast<
                    std::chrono::seconds>(
                    std::chrono::system_clock::now()
                        .time_since_epoch())
                    .count();

            const auto timestamp =
                static_cast<std::uint64_t>(
                    now < 0 ? 0 : now);

            const std::uint64_t block_timestamp =
                timestamp < previous.header.timestamp
                    ? previous.header.timestamp
                    : timestamp;

            const std::uint32_t mining_difficulty =
                (previous.header.height == 0 &&
                 previous.header.difficulty == 0)
                    ? caesar::CZR_INITIAL_MINING_DIFFICULTY
                    : previous.header.difficulty;

            caesar::Block candidate =
                caesar::BlockBuilder::build(
                    previous,
                    mempool,
                    "CAESAR_MINER_CZR1",
                    block_timestamp,
                    mining_difficulty);

            if (!caesar::BlockBuilder::mine(
                    candidate,
                    0,
                    1000000)) {
                throw std::runtime_error(
                    "mining failed");
            }

            if (!caesar::validate_block_consensus(
                    candidate,
                    chain,
                    previous_utxos)) {
                throw std::runtime_error(
                    "mined block failed consensus");
            }

            storage.append(candidate);
            chain = storage.load();

            std::cout
                << "Mined block: "
                << candidate.header.height
                << "\n";

            std::cout
                << "Nonce: "
                << candidate.header.nonce
                << "\n";

            std::cout
                << "Block hash: "
                << caesar::hash_to_hex(
                       candidate.hash())
                << "\n";

            std::cout
                << "Blockchain blocks: "
                << chain.size()
                << "\n";
        }

        std::cout
            << "Storage file: "
            << storage.path()
            << "\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr
            << "Caesar node error: "
            << e.what()
            << "\n";

        return 1;
    }
}
