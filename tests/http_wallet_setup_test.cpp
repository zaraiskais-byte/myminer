#include <caesar/http_rpc.hpp>
#include <caesar/node.hpp>

#include <httplib.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<unsigned char> read_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("failed to open test wallet");

    return std::vector<unsigned char>(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}

void write_bytes(const std::filesystem::path& path,
                 const std::vector<unsigned char>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("failed to create test wallet");

    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));

    if (!out)
        throw std::runtime_error("failed to write test wallet");
}

} // namespace

int main() {
    const auto base =
        std::filesystem::temp_directory_path() /
        ("caesar_http_wallet_setup_test_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));

    std::filesystem::create_directories(base);

    try {
        const auto wallet_path = base / "wallet.pem";

        // Deliberately large enough to enter PersistentWallet's
        // deferred/encrypted-file path, but not a valid wallet.
        std::vector<unsigned char> corrupt_wallet(128);
        for (std::size_t i = 0; i < corrupt_wallet.size(); ++i)
            corrupt_wallet[i] = static_cast<unsigned char>(i ^ 0xA5);

        write_bytes(wallet_path, corrupt_wallet);

        const auto before = read_bytes(wallet_path);

        // The HTTP server only needs a valid node reference; the node
        // itself does not need to be started for /api/auth/setup.
        caesar::CaesarNode node(base, 19555, 1);
        caesar::HttpRpcServer rpc(node, 19556, base);
        rpc.start();

        if (!rpc.running())
            throw std::runtime_error("HTTP RPC server failed to start");

        httplib::Client client("127.0.0.1", 19556);
        auto response = client.Post(
            "/api/auth/setup",
            R"({"pin":"1234"})",
            "application/json");

        rpc.stop();

        if (!response)
            throw std::runtime_error("HTTP setup request failed");

        if (response->status != 400)
            throw std::runtime_error(
                "expected HTTP 400 when setup encounters a "
                "pre-existing unavailable wallet");

        const auto after = read_bytes(wallet_path);

        if (after != before)
            throw std::runtime_error(
                "setup modified the pre-existing unavailable wallet");

        if (std::filesystem::exists(base / "pin.hash"))
            throw std::runtime_error(
                "setup created pin.hash despite refusing replacement");

        if (std::filesystem::exists(base / "session.txt"))
            throw std::runtime_error(
                "setup created session.txt despite refusing replacement");

        std::filesystem::remove_all(base);
        return 0;
    } catch (...) {
        std::filesystem::remove_all(base);
        throw;
    }
}
