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

    std::uint64_t found_nonce = 0;
    caesar::Hash256 found_hash{};

    const bool mined =
        caesar::mine_pow(
            block1.pow_header(),
            block1.header.difficulty,
            2170,
            100000,
            found_nonce,
            found_hash);

    if (!mined)
        throw std::runtime_error(
            "failed to mine block1 in storage test");

    block1.header.nonce = found_nonce;

    if (block1.pow_hash() != found_hash)
        throw std::runtime_error(
            "storage test PoW hash mismatch");

    if (!block1.validate_pow())
        throw std::runtime_error(
            "storage test mined block has invalid PoW");

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

    // Real competing-chain fork choice.
    //
    // Active:
    //   genesis -> A1
    //
    // Candidate:
    //   genesis -> B1 -> B2
    //
    // B1 is deliberately different from A1. B2 gives the candidate
    // strictly greater cumulative PoW work, so replace_chain() must reorg.
    {
        const auto fork_path =
            std::filesystem::temp_directory_path() /
            "caesar_blockchain_reorg_test.dat";

        std::filesystem::remove(fork_path, ec);
        std::filesystem::remove(
            fork_path.string() + ".tmp",
            ec);

        caesar::BlockchainStorage fork_storage(fork_path);

        fork_storage.save({genesis});

        caesar::Mempool fork_mempool;

        auto active_block =
            caesar::BlockBuilder::build(
                genesis,
                fork_mempool,
                "CAESAR_ACTIVE_MINER",
                1,
                caesar::CZR_INITIAL_MINING_DIFFICULTY);

        if (!caesar::BlockBuilder::mine(
                active_block,
                0,
                100000)) {
            throw std::runtime_error(
                "failed to mine active reorg block");
        }

        if (!active_block.validate_pow())
            throw std::runtime_error(
                "active reorg block has invalid PoW");

        fork_storage.append(active_block);

        const auto active_chain = fork_storage.load();
        assert(active_chain.size() == 2);

        auto fork_block1 =
            caesar::BlockBuilder::build(
                genesis,
                fork_mempool,
                "CAESAR_FORK_MINER",
                1,
                caesar::CZR_INITIAL_MINING_DIFFICULTY);

        if (!caesar::BlockBuilder::mine(
                fork_block1,
                0,
                100000)) {
            throw std::runtime_error(
                "failed to mine first fork block");
        }

        if (!fork_block1.validate_pow())
            throw std::runtime_error(
                "first fork block has invalid PoW");

        if (fork_block1.hash() == active_block.hash())
            throw std::runtime_error(
                "fork block unexpectedly equals active block");

        auto fork_block2 =
            caesar::BlockBuilder::build(
                fork_block1,
                fork_mempool,
                "CAESAR_FORK_MINER",
                2,
                caesar::CZR_INITIAL_MINING_DIFFICULTY);

        if (!caesar::BlockBuilder::mine(
                fork_block2,
                0,
                100000)) {
            throw std::runtime_error(
                "failed to mine second fork block");
        }

        if (!fork_block2.validate_pow())
            throw std::runtime_error(
                "second fork block has invalid PoW");

        const std::vector<caesar::Block> candidate{
            genesis,
            fork_block1,
            fork_block2
        };

        const auto before_reorg = fork_storage.load();

        if (!(caesar::calculate_chain_work(candidate) >
              caesar::calculate_chain_work(before_reorg))) {
            throw std::runtime_error(
                "candidate chain does not have greater cumulative work");
        }

        if (!fork_storage.replace_chain(candidate))
            throw std::runtime_error(
                "higher-work candidate failed to replace active chain");

        const auto after_reorg = fork_storage.load();

        assert(after_reorg.size() == 3);
        assert(after_reorg[1].hash() == fork_block1.hash());
        assert(after_reorg[2].hash() == fork_block2.hash());

        // Equal-work candidate must not replace the active chain.
        if (fork_storage.replace_chain(after_reorg))
            throw std::runtime_error(
                "equal-work chain unexpectedly replaced active chain");

        // Lower-work candidate must not replace the active chain.
        if (fork_storage.replace_chain(active_chain))
            throw std::runtime_error(
                "lower-work chain unexpectedly replaced active chain");

        std::filesystem::remove(fork_path, ec);
        std::filesystem::remove(
            fork_path.string() + ".tmp",
            ec);
    }

    std::filesystem::remove(path, ec);
    std::filesystem::remove(
        path.string() + ".tmp",
        ec);

    return 0;
}
