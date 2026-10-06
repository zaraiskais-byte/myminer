#include <cassert>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#define private public
#include <caesar/p2p_relay.hpp>
#undef private

#include <caesar/blockchain_storage.hpp>
#include <caesar/p2p_server.hpp>
#include <caesar/p2p_sync_protocol.hpp>

using namespace caesar;

int main() {
    const auto path =
        std::filesystem::temp_directory_path() /
        "caesar_p2p_pending_sync_abort_test.dat";

    std::error_code ec;
    std::filesystem::remove(path, ec);

    P2PServer server;
    BlockchainStorage storage(path);
    auto storage_mutex = std::make_shared<std::mutex>();

    P2PRelay relay(server, storage, storage_mutex);

    constexpr std::uint64_t peer_id = 1;

    const auto session_id = relay.make_sync_session_id();

    P2PRelay::PendingChainSync pending;
    pending.session_id = session_id;
    pending.headers.resize(1);
    pending.blocks.resize(1);
    pending.received.assign(1, false);

    relay.pending_chain_syncs_.emplace(peer_id, std::move(pending));

    SyncBlocksMessage abort;
    abort.session_id = session_id;

    const auto payload = abort.serialize_binary();

    relay.handle_sync_blocks(peer_id, payload);

    assert(
        relay.pending_chain_syncs_.find(peer_id) ==
        relay.pending_chain_syncs_.end());

    std::filesystem::remove(path, ec);

    return 0;
}
