/*
 * GetMempool / GetTransaction protocol test.
 *
 * P2PRelay now answers two more previously-declared-only message
 * types:
 *
 *   GetMempool     -> N Transaction frames, one per mempool entry
 *   GetTransaction -> one Transaction frame or no reply
 *
 * The relay does not own a mempool. The node exposes read-only views
 * via two callbacks:
 *
 *   set_mempool_snapshot_callback()
 *   set_mempool_tx_callback()
 *
 * This test drives handle_get_mempool and handle_get_transaction
 * against a real TCP pair with a fixed set of fake transactions.
 */

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

// Testing-only access to P2PRelay private members.
#define private public
#include <caesar/p2p_relay.hpp>
#undef private

#include <caesar/blockchain_storage.hpp>
#include <caesar/p2p_server.hpp>
#include <caesar/transaction.hpp>

using namespace caesar;

namespace {

constexpr std::uint16_t PORT = 39502;

Transaction make_fake(uint64_t tag) {
    Transaction tx;
    tx.version = 1;

    TransactionInput in;
    in.previous_txid[0] = static_cast<uint8_t>(tag);
    in.output_index = static_cast<uint32_t>(tag);
    tx.inputs.push_back(in);

    TransactionOutput out;
    out.amount = 1 + tag;
    out.recipient = "TEST";
    tx.outputs.push_back(out);

    return tx;
}

std::filesystem::path fresh_storage() {
    const auto p = std::filesystem::temp_directory_path() / "caesar_p2p_mempool_request_test.dat";
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return p;
}

} // namespace

int main() {
    const auto storage_path = fresh_storage();

    P2PServer server;
    BlockchainStorage storage(storage_path);
    auto storage_mutex = std::make_shared<std::mutex>();

    P2PRelay relay(server, storage, storage_mutex);

    // Fake mempool: three transactions.
    std::vector<Transaction> pool = {make_fake(1), make_fake(2), make_fake(3)};

    relay.set_mempool_snapshot_callback([&]() { return pool; });

    relay.set_mempool_tx_callback([&](const Hash256& txid) -> std::optional<Transaction> {
        for (const auto& tx : pool)
            if (tx.txid() == txid)
                return tx;
        return std::nullopt;
    });

    // Real TCP pair.
    P2PTcpSocket listener;
    listener.listen_on(PORT);

    std::vector<std::uint8_t> first_tx_hash; // hash of first pool tx
    std::vector<Transaction> received;
    bool timeout_hit = false;

    std::thread client([&]() {
        P2PConnection c;
        c.connect_to("127.0.0.1", PORT);
        c.set_timeouts(500);

        // --- Receive the GetMempool response: 3 Transaction frames ---
        try {
            for (int i = 0; i < 3; ++i) {
                const P2PFrame f = c.receive_frame();
                assert(f.type == P2PMessageType::Transaction);
                received.push_back(Transaction::deserialize_full(f.payload));
            }
        } catch (...) {
            timeout_hit = true;
        }

        // --- Receive the GetTransaction response: 1 Transaction frame ---
        try {
            const P2PFrame f = c.receive_frame();
            assert(f.type == P2PMessageType::Transaction);
            received.push_back(Transaction::deserialize_full(f.payload));
        } catch (...) {
            timeout_hit = true;
        }

        // --- Wait for the unknown-txid case, expect timeout ---
        try {
            (void)c.receive_frame();
            // If we get here, unexpected frame arrived.
            assert(false);
        } catch (...) {
            // expected
        }
    });

    P2PConnection accepted(listener.accept_connection());
    const std::uint64_t peer_id = server.peers().add_peer(std::move(accepted), "127.0.0.1", PORT);

    // Case 1: GetMempool -> 3 Transaction frames.
    relay.handle_get_mempool(peer_id, {});

    // Give the client thread time to read the frames before the next
    // handler runs, to keep the socket buffer clean.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Case 2: GetTransaction with a known txid -> 1 Transaction frame.
    const Hash256 known_txid = pool[0].txid();
    std::vector<std::uint8_t> known_payload(known_txid.begin(), known_txid.end());
    relay.handle_get_transaction(peer_id, known_payload);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Case 3: GetTransaction with unknown txid -> no reply.
    Hash256 unknown_txid{};
    unknown_txid[0] = 0xFF;
    std::vector<std::uint8_t> unknown_payload(unknown_txid.begin(), unknown_txid.end());
    relay.handle_get_transaction(peer_id, unknown_payload);

    client.join();

    assert(!timeout_hit);
    assert(received.size() == 4);

    // First three are the snapshot, in the pool's order.
    assert(received[0].txid() == pool[0].txid());
    assert(received[1].txid() == pool[1].txid());
    assert(received[2].txid() == pool[2].txid());

    // Fourth is the specifically requested tx.
    assert(received[3].txid() == pool[0].txid());

    std::cout << "[getmempool] 3 txs returned: OK\n";
    std::cout << "[gettransaction] known txid returned: OK\n";
    std::cout << "[gettransaction] unknown txid ignored: OK\n";

    // Case 4: malformed GetMempool payload -> throws.
    {
        bool threw = false;
        try {
            relay.handle_get_mempool(peer_id, {0x01});
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
    }
    std::cout << "[getmempool] malformed payload rejected: OK\n";

    // Case 5: malformed GetTransaction payload -> throws.
    {
        bool threw = false;
        try {
            relay.handle_get_transaction(peer_id, {0x01, 0x02});
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
    }
    std::cout << "[gettransaction] malformed payload rejected: OK\n";

    // Case 6: no callbacks registered -> silent (must not throw).
    {
        P2PServer s2;
        BlockchainStorage st2(storage_path.string() + ".2");
        auto m2 = std::make_shared<std::mutex>();
        P2PRelay r2(s2, st2, m2);

        r2.handle_get_mempool(peer_id, {});
        r2.handle_get_transaction(peer_id, unknown_payload);
    }
    std::cout << "[no-callback] silent: OK\n";

    server.peers().remove_peer(peer_id);
    std::filesystem::remove(storage_path);

    std::cout << "CaesarP2PMempoolRequestTest: PASS\n";
    return 0;
}
