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
    bool has_port{false};
    std::uint16_t port{0};
    bool has_data{false};
    std::string data_dir;
    bool has_peer{false};
    std::string peer_host;
    std::uint16_t peer_port{0};
};

constexpr std::uint16_t MAINNET_PORT = 18444;
constexpr std::uint16_t TESTNET_PORT = 18445;

const char* MAINNET_DATA_DIR = "data";
const char* TESTNET_DATA_DIR = "data-testnet";

void print_usage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " [--mine] [--testnet] [--port <n>] [--data <path>] [--peer <h:p>]\n"
              << "  --mine           Mine exactly one block then exit.\n"
              << "  --testnet        Run on Caesar CZR Testnet (network id 2).\n"
              << "  --port <n>       Override the P2P listening port.\n"
              << "  --data <path>    Override the data directory.\n"
              << "  --peer <h:p>     Connect to a peer at host:port after start.\n";
}

bool parse_cli(int argc, char** argv, CliOptions& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--mine") {
            out.mine_one_block = true;
        } else if (arg == "--testnet") {
            out.testnet = true;
        } else if (arg == "--port") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value after --port\n";
                return false;
            }
            try {
                const int v = std::stoi(argv[++i]);
                if (v <= 0 || v > 65535) {
                    std::cerr << "Invalid port: " << v << "\n";
                    return false;
                }
                out.has_port = true;
                out.port = static_cast<std::uint16_t>(v);
            } catch (...) {
                std::cerr << "Invalid port value\n";
                return false;
            }
        } else if (arg == "--data") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value after --data\n";
                return false;
            }
            out.has_data = true;
            out.data_dir = argv[++i];
        } else if (arg == "--peer") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value after --peer\n";
                return false;
            }
            const std::string pv = argv[++i];
            const auto colon = pv.find(':');
            if (colon == std::string::npos || colon == 0 ||
                colon + 1 >= pv.size()) {
                std::cerr << "Peer format must be host:port\n";
                return false;
            }
            try {
                const int p = std::stoi(pv.substr(colon + 1));
                if (p <= 0 || p > 65535) {
                    std::cerr << "Invalid peer port\n";
                    return false;
                }
                out.has_peer = true;
                out.peer_host = pv.substr(0, colon);
                out.peer_port = static_cast<std::uint16_t>(p);
            } catch (...) {
                std::cerr << "Invalid peer port value\n";
                return false;
            }
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

        std::uint16_t port = opts.testnet ? TESTNET_PORT : MAINNET_PORT;
        if (opts.has_port) {
            port = opts.port;
        }

        const std::uint32_t network =
            opts.testnet ? caesar::NETWORK_TESTNET : caesar::NETWORK_MAINNET;

        std::string data_dir_storage =
            opts.testnet ? std::string(TESTNET_DATA_DIR)
                         : std::string(MAINNET_DATA_DIR);
        if (opts.has_data) {
            data_dir_storage = opts.data_dir;
        }
        const char* data_dir = data_dir_storage.c_str();

        std::cout << "=== Caesar CZR Node ===\n";
        std::cout << "Network: " << (opts.testnet ? "testnet" : "mainnet") << " (id=" << network
                  << ")\n";
        std::cout << "Port: " << port << "\n";
        std::cout << "Data: " << data_dir << "\n";

        caesar::CaesarNode node(data_dir, port, network);
        node.start();

        if (opts.has_peer) {
            std::cout << "Connecting to peer " << opts.peer_host
                      << ":" << opts.peer_port << "...\n" << std::flush;
            try {
                const auto peer_id =
                    node.connect_to_peer(opts.peer_host, opts.peer_port);
                std::cout << "Connected. peer_id=" << peer_id
                          << "\n" << std::flush;
            } catch (const std::exception& e) {
                std::cerr << "Peer connect failed: " << e.what()
                          << "\n" << std::flush;
            }
            // Give the peer manager a moment to register the peer.
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "Node: RUNNING\n" << std::flush;
        std::cout << "Blockchain height: " << node.height() << "\n" << std::flush;
        std::cout << "Peers: " << node.peer_count() << "\n" << std::flush;

        if (opts.mine_one_block) {
            std::cout << "Mining one block...\n";

            node.mine_one_block("CAESAR_MINER_CZR1", 1000000);

            std::cout << "Mined successfully.\n";
            std::cout << "New blockchain height: " << node.height() << "\n";
        }

        std::cout << "Storage: " << data_dir << "/blockchain.dat\n";

        if (!opts.mine_one_block) {
            std::cout << "Node is running. Press Ctrl+C to stop.\n";

            while (node.running()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        node.stop();
        std::cout << "Node: STOPPED\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Caesar node error: " << e.what() << "\n";
        return 1;
    }
}
