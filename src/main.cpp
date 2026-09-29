#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <caesar/network_params.hpp>
#include <caesar/node.hpp>

namespace {

struct CliOptions {
    bool mine_one_block{false};
    bool testnet{false};
};

constexpr std::uint16_t MAINNET_PORT = 18444;
constexpr std::uint16_t TESTNET_PORT = 18445;

const char* MAINNET_DATA_DIR = "data";
const char* TESTNET_DATA_DIR = "data-testnet";

void print_usage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " [--mine] [--testnet]\n"
        << "  --mine     Mine exactly one block then exit.\n"
        << "  --testnet  Run on Caesar CZR Testnet (network id 2).\n"
        << "             Without this flag the node runs on Mainnet.\n";
}

bool parse_cli(int argc, char** argv, CliOptions& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--mine") {
            out.mine_one_block = true;
        } else if (arg == "--testnet") {
            out.testnet = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    try {
        CliOptions opts;
        if (!parse_cli(argc, argv, opts))
            return 2;

        const std::uint16_t port =
            opts.testnet ? TESTNET_PORT : MAINNET_PORT;

        const std::uint32_t network =
            opts.testnet ? caesar::NETWORK_TESTNET
                         : caesar::NETWORK_MAINNET;

        const char* data_dir =
            opts.testnet ? TESTNET_DATA_DIR : MAINNET_DATA_DIR;

        std::cout << "=== Caesar CZR Node ===\n";
        std::cout << "Network: "
                  << (opts.testnet ? "testnet" : "mainnet")
                  << " (id=" << network << ")\n";
        std::cout << "Port: " << port << "\n";
        std::cout << "Data: " << data_dir << "\n";

        caesar::CaesarNode node(data_dir, port, network);
        node.start();

        std::cout << "Node: RUNNING\n";
        std::cout << "Blockchain height: "
                  << node.height() << "\n";
        std::cout << "Peers: "
                  << node.peer_count() << "\n";

        if (opts.mine_one_block) {
            std::cout << "Mining one block...\n";

            node.mine_one_block(
                "CAESAR_MINER_CZR1",
                1000000);

            std::cout << "Mined successfully.\n";
            std::cout << "New blockchain height: "
                      << node.height() << "\n";
        }

        std::cout << "Storage: "
                  << data_dir << "/blockchain.dat\n";

        if (!opts.mine_one_block) {
            std::cout
                << "Node is running. Press Ctrl+C to stop.\n";

            while (node.running()) {
                std::this_thread::sleep_for(
                    std::chrono::seconds(1));
            }
        }

        node.stop();
        std::cout << "Node: STOPPED\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr
            << "Caesar node error: "
            << e.what()
            << "\n";
        return 1;
    }
}
