#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/block_builder.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/mempool.hpp>
#include <caesar/p2p_connection.hpp>
#include <caesar/p2p_handshake.hpp>
#include <caesar/p2p_protocol.hpp>
#include <caesar/p2p_relay.hpp>
#include <caesar/p2p_server.hpp>
#include <caesar/serialization.hpp>

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

caesar::Block make_invalid_block(
    const caesar::Block& genesis) {

    caesar::Mempool mempool;

    auto block =
        caesar::BlockBuilder::build(
            genesis,
            mempool,
            "CAESAR_TEST_MINER",
            1,
            caesar::CZR_INITIAL_MINING_DIFFICULTY);

    caesar::Hash256 wrong_previous{};
    wrong_previous[0] = 0xFF;

    block.header.previous_hash = wrong_previous;

    std::uint64_t found_nonce = 0;
    caesar::Hash256 found_hash{};

    const bool mined =
        caesar::mine_pow(
            block.pow_header(),
            block.header.difficulty,
            2170,
            100000,
            found_nonce,
            found_hash);

    if (!mined)
        throw std::runtime_error(
            "failed to mine disconnected block");

    block.header.nonce = found_nonce;

    if (!block.validate_pow())
        throw std::runtime_error(
            "disconnected block has invalid PoW");

    return block;
}

caesar::P2PFrame make_blocks_frame(
    const std::vector<caesar::Block>& blocks) {

    caesar::BinaryWriter writer;

    writer.write_u32(
        static_cast<std::uint32_t>(blocks.size()));

    for (const auto& block : blocks) {
        const auto data =
            block.serialize_full_binary();

        writer.write_u32(
            static_cast<std::uint32_t>(data.size()));

        writer.write_bytes(data);
    }

    caesar::P2PFrame frame;
    frame.type = caesar::P2PMessageType::Blocks;
    frame.payload = writer.data();

    return frame;
}

}

int main() {
    using namespace caesar;

    const auto path =
        std::filesystem::temp_directory_path() /
        "caesar_p2p_relay_atomic_batch_test.dat";

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(
        path.string() + ".tmp",
        ec);

    BlockchainStorage storage(path);
    storage.save({make_genesis()});

    P2PServer server;
    P2PRelay relay(server, storage);

    constexpr std::uint16_t port = 39428;

    server.start(port, "127.0.0.1");
    relay.start();

    P2PConnection client;
    client.connect_to("127.0.0.1", port);

    P2PHello client_hello;
    perform_hello_handshake(
        client,
        client_hello,
        1);

    for (int i = 0; i < 500; ++i) {
        if (server.peer_count() == 1)
            break;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    assert(server.peer_count() == 1);

    const auto genesis = storage.load().back();
    const auto disconnected =
        make_invalid_block(genesis);

    assert(disconnected.header.height == 1);
    assert(
        disconnected.header.previous_hash !=
        genesis.hash());
    assert(disconnected.validate_pow());

    caesar::Block valid_block;
    {
        caesar::Mempool mempool;

        valid_block =
            caesar::BlockBuilder::build(
                genesis,
                mempool,
                "CAESAR_TEST_MINER",
                1,
                caesar::CZR_INITIAL_MINING_DIFFICULTY);

        std::uint64_t found_nonce = 0;
        caesar::Hash256 found_hash{};

        const bool mined =
            caesar::mine_pow(
                valid_block.pow_header(),
                valid_block.header.difficulty,
                2170,
                100000,
                found_nonce,
                found_hash);

        if (!mined)
            throw std::runtime_error(
                "failed to mine valid block");

        valid_block.header.nonce = found_nonce;

        if (!valid_block.validate_pow())
            throw std::runtime_error(
                "valid block has invalid PoW");
    }

    assert(valid_block.header.height == 1);
    assert(
        valid_block.header.previous_hash ==
        genesis.hash());

    const auto local_chain = storage.load();
    const auto local_utxos =
        caesar::rebuild_utxo_set(local_chain);

    const bool local_consensus_ok =
        caesar::validate_block_consensus(
            valid_block,
            local_chain,
            local_utxos);

    std::cerr
        << "[ATOMIC] local valid_block consensus="
        << (local_consensus_ok ? "OK" : "FAIL")
        << " height="
        << valid_block.header.height
        << " difficulty="
        << valid_block.header.difficulty
        << " timestamp="
        << valid_block.header.timestamp
        << std::endl;

    assert(local_consensus_ok);

    assert(disconnected.header.height == 1);
    assert(
        disconnected.header.previous_hash !=
        genesis.hash());

    client.send_frame(
        make_blocks_frame({valid_block, disconnected}));

    bool accepted = false;

    for (int i = 0; i < 500; ++i) {
        const auto chain = storage.load();

        if (chain.size() == 2 &&
            chain.back().header.height == 1 &&
            chain.back().hash() == valid_block.hash()) {
            accepted = true;
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    assert(accepted);

    const auto chain = storage.load();

    assert(chain.size() == 2);
    assert(chain.back().header.height == 1);
    assert(
        chain.back().hash() ==
        valid_block.hash());

    relay.stop();
    server.stop();

    std::filesystem::remove(path, ec);
    std::filesystem::remove(
        path.string() + ".tmp",
        ec);

    return 0;
}
