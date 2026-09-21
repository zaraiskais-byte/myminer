#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/block_builder.hpp>
#include <caesar/blockchain_storage.hpp>
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

caesar::BlockHeader make_header(
    std::uint64_t height,
    const caesar::Hash256& previous_hash) {

    caesar::BlockHeader header;

    header.version = 1;
    header.height = height;
    header.previous_hash = previous_hash;
    header.timestamp = height;
    header.nonce = 0;
    header.difficulty = 0;
    header.merkle_root = {};

    return header;
}

caesar::P2PFrame make_headers_frame(
    const std::vector<caesar::BlockHeader>& headers) {

    caesar::BinaryWriter writer;

    writer.write_u32(
        static_cast<std::uint32_t>(headers.size()));

    for (const auto& header : headers) {
        const auto data =
            header.serialize_binary();

        writer.write_u32(
            static_cast<std::uint32_t>(data.size()));

        writer.write_bytes(data);
    }

    caesar::P2PFrame frame;
    frame.type = caesar::P2PMessageType::Headers;
    frame.payload = writer.data();

    return frame;
}

}

int main() {
    using namespace caesar;

    const auto path =
        std::filesystem::temp_directory_path() /
        "caesar_p2p_relay_headers_validation_test.dat";

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(
        path.string() + ".tmp",
        ec);

    BlockchainStorage storage(path);
    storage.save({make_genesis()});

    P2PServer server;
    P2PRelay relay(server, storage);

    constexpr std::uint16_t port = 39426;

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

    const auto first =
        make_header(
            1,
            genesis.hash());

    Hash256 wrong_previous{};
    wrong_previous[0] = 0xFF;

    const auto second =
        make_header(
            2,
            wrong_previous);

    client.send_frame(
        make_headers_frame({first, second}));

    for (int i = 0; i < 500; ++i) {
        if (server.peer_count() == 0)
            break;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    assert(server.peer_count() == 0);

    const auto chain = storage.load();

    assert(chain.size() == 1);
    assert(chain.front().header.height == 0);

    relay.stop();
    server.stop();

    std::filesystem::remove(path, ec);
    std::filesystem::remove(
        path.string() + ".tmp",
        ec);

    return 0;
}
