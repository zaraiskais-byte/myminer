#include "qrcodegen.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <caesar/http_rpc.hpp>
#include <caesar/node.hpp>

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running.store(false);
}
}  // namespace

int main(int argc, char** argv) {
    try {
        std::uint16_t p2p_port = 18555;
        std::uint16_t rpc_port = 8443;
        std::string data_dir = "data-web";
        std::string peer_addr;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];

            if (arg == "--help") {
                std::cout << "Usage: caesar_web [options]\n\n"
                          << "Options:\n"
                          << "  --port PORT       P2P port (default 18444)\n"
                          << "  --rpc-port PORT   HTTP RPC port (default 8332)\n"
                          << "  --data PATH       Data directory (default data-web)\n"
                          << "  --peer HOST:PORT  Connect to a peer at startup (retries)\n"
                          << "  --help\n";
                return 0;
            } else if (arg == "--port") {
                if (i + 1 >= argc) throw std::runtime_error("--port requires value");
                p2p_port = static_cast<std::uint16_t>(std::stoul(argv[++i]));
            } else if (arg == "--rpc-port") {
                if (i + 1 >= argc) throw std::runtime_error("--rpc-port requires value");
                rpc_port = static_cast<std::uint16_t>(std::stoul(argv[++i]));
            } else if (arg == "--data") {
                if (i + 1 >= argc) throw std::runtime_error("--data requires value");
                data_dir = argv[++i];
            } else if (arg == "--peer") {
                if (i + 1 >= argc) throw std::runtime_error("--peer requires host:port");
                peer_addr = argv[++i];
            } else {
                throw std::runtime_error("unknown argument: " + arg);
            }
        }

        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        std::cout << "=== Caesar CZR Web Wallet ===\n";
        std::cout << "P2P port: " << p2p_port << "\n";
        std::cout << "RPC port: " << rpc_port << "\n";
        std::cout << "Data dir: " << data_dir << "\n";

        caesar::CaesarNode node(data_dir, p2p_port, 1);
        node.start();

        // --peer connection thread (deferred until after node.start)
        if (!peer_addr.empty()) {
            auto colon = peer_addr.find(':');
            if (colon == std::string::npos) {
                std::cerr << "[peer] invalid format, expected host:port\n";
            } else {
                std::string ph = peer_addr.substr(0, colon);
                std::uint16_t pp = 0;
                try { pp = static_cast<std::uint16_t>(std::stoi(peer_addr.substr(colon+1))); }
                catch (...) { std::cerr << "[peer] invalid port\n"; }
                if (pp != 0) {
                    std::cout << "[peer] will connect to " << ph << ":" << pp << "\n";
                    std::thread([&node, ph, pp]() {
                        while (true) {
                            // If we already have peers, sleep and re-check
                            if (node.peer_count() > 0) {
                                std::this_thread::sleep_for(std::chrono::seconds(5));
                                continue;
                            }
                            try {
                                std::uint64_t peer_id = node.connect_to_peer(ph, pp);
                                std::cout << "[peer] connected to " << ph << ":" << pp
                                          << " (id=" << peer_id << ")" << std::endl;
                                std::this_thread::sleep_for(std::chrono::seconds(2));
                                try {
                                    node.request_sync_from_peer(peer_id);
                                    std::cout << "[peer] sync requested from peer " << peer_id << std::endl;
                                } catch (const std::exception& e) {
                                    std::cerr << "[peer] sync request failed: " << e.what() << std::endl;
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "[peer] reconnect attempt failed: " << e.what() << std::endl;
                                std::this_thread::sleep_for(std::chrono::seconds(10));
                            }
                        }
                    }).detach();
                }
            }
        }

        caesar::HttpRpcServer rpc(node, rpc_port, data_dir);
        rpc.start();

        std::cout << "Node height: " << node.height() << "\n";
        std::cout << "\n";
        std::cout << "  Open in browser: http://127.0.0.1:" << rpc_port << "\n";
        std::cout << "\n";
        std::cout << "Press Ctrl+C to stop.\n";

        while (g_running.load() && node.running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        rpc.stop();
        node.stop();

        std::cout << "\nStopped.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
