/*
 * Regression test for the Headers wire format bug.
 *
 * P2PRelay::handle_headers() used to compare the wire-format size
 * against a hardcoded 84 (Bitcoin's header size). Caesar's BlockHeader
 * serializes to 128 bytes, so every received Headers message was
 * rejected with "invalid block header size", which caused
 * CaesarChainSyncTest and CaesarP2PForkReorgTest to fail in CI.
 *
 * This test builds a valid 128-byte BlockHeader, wraps it in the exact
 * wire format the sender produces, feeds it through handle_headers(),
 * and asserts that the failure is NOT a size rejection.
 *
 * On the buggy code the test would fail with
 *   "REGRESSION: 128-byte header was rejected by size check"
 * On the fixed code it passes.
 */

// Standard library first (unaffected by the private->public macro).
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

// Testing-only access to P2PRelay::handle_headers.
#define private public
#include <caesar/p2p_relay.hpp>
#undef private

#include <caesar/block.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/p2p_server.hpp>
#include <caesar/serialization.hpp>

using namespace caesar;

int main() {
    // 1. Build a BlockHeader with valid basic fields.
    BlockHeader header;
    header.version = 1;
    header.height = 1;
    header.previous_hash = {};
    header.merkle_root = {};
    header.witness_root = {};
    header.timestamp = 120;
    header.nonce = 0;
    header.difficulty = 0; // trivially satisfiable PoW

    const auto header_bytes = header.serialize_binary();

    std::cout << "[headers-size] serialized BlockHeader = " << header_bytes.size() << " bytes\n";
    assert(header_bytes.size() == 128);

    // 2. Build the exact wire payload handle_headers() expects:
    //    [u32 count][u32 size][size bytes of header]
    BinaryWriter writer;
    writer.write_u32(1);
    writer.write_u32(static_cast<std::uint32_t>(header_bytes.size()));
    writer.write_bytes(header_bytes);

    const auto payload = writer.data();

    // 3. Construct P2PRelay with a dummy server + storage.
    const auto tmp_path =
        std::filesystem::temp_directory_path() / "caesar_headers_size_regression_test.dat";

    std::error_code ec;
    std::filesystem::remove(tmp_path, ec);

    P2PServer server;
    BlockchainStorage storage(tmp_path);
    auto storage_mutex = std::make_shared<std::mutex>();

    P2PRelay relay(server, storage, storage_mutex);

    // 4. Feed the payload through handle_headers().
    //    On the fixed code, it passes the size check and fails later
    //    because the dummy storage has no chain.
    //    On the buggy code, it fails immediately with a size error.
    std::string message;
    try {
        relay.handle_headers(1, payload);
    } catch (const std::exception& e) {
        message = e.what();
        std::cout << "[headers-size] caught: " << message << "\n";
    }

    // 5. Assert the failure was NOT a size rejection.
    const bool size_rejected = message.find("invalid block header size") != std::string::npos;

    if (size_rejected) {
        std::cerr << "REGRESSION: 128-byte header was rejected "
                     "by size check\n";
        return 1;
    }

    // 6. Sanity check: the failure must be the missing chain.
    const bool chain_missing = message.find("local chain") != std::string::npos;

    std::cout << "[headers-size] rejected for missing chain: " << (chain_missing ? "yes" : "no")
              << "\n";

    assert(chain_missing && "expected the dummy storage to be empty");

    std::filesystem::remove(tmp_path, ec);

    std::cout << "CaesarHeadersSizeRegressionTest: PASS\n";
    return 0;
}
