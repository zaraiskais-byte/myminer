#include <chrono>
#include <cstdint>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

#include <caesar/node.hpp>

namespace {

volatile std::sig_atomic_t g_shutdown_requested = 0;

void handle_shutdown_signal(int) {
    g_shutdown_requested = 1;
}

bool parse_uint16(
    const std::string& text,
    std::uint16_t& value) {

    try {
        const unsigned long parsed =
            std::stoul(text);

        if (parsed > 65535UL) {
            return false;
        }

        value =
            static_cast<std::uint16_t>(parsed);

        return true;
    } catch (...) {
        return false;
    }
}

bool parse_uint32(
    const std::string& text,
    std::uint32_t& value) {

    try {
        const unsigned long parsed =
            std::stoul(text);

        if (parsed >
            static_cast<unsigned long>(
                std::numeric_limits<std::uint32_t>::max())) {
            return false;
        }

        value =
            static_cast<std::uint32_t>(parsed);

        return true;
    } catch (...) {
        return false;
    }
}

}

int main(int argc, char** argv) {
    try {
        bool mine_one_block = false;
        std::uint32_t mine_after_ms = 0;
        bool mine_after_delay = false;
        std::filesystem::path data_dir = "data";
        std::uint16_t p2p_port = 18444;
        std::uint32_t network_id = 1;
        std::string connect_host;
        std::uint16_t connect_port = 0;
        bool connect_to_peer = false;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];

            if (arg == "--mine") {
                mine_one_block = true;
                continue;
            }

            if (arg == "--mine-after-ms") {
                if (++i >= argc) {
                    std::cerr
                        << "Missing value for --mine-after-ms\n";
                    return 2;
                }

                if (!parse_uint32(
                        argv[i],
                        mine_after_ms)) {
                    std::cerr
                        << "Invalid --mine-after-ms value\n";
                    return 2;
                }

                mine_after_delay = true;
                continue;
            }

            if (arg == "--data-dir") {
                if (++i >= argc) {
                    std::cerr
                        << "Missing value for --data-dir\n";
                    return 2;
                }

                data_dir = argv[i];
                continue;
            }

            if (arg == "--port") {
                if (++i >= argc) {
                    std::cerr
                        << "Missing value for --port\n";
                    return 2;
                }

                if (!parse_uint16(
                        argv[i],
                        p2p_port)) {
                    std::cerr
                        << "Invalid --port value\n";
                    return 2;
                }

                continue;
            }

            if (arg == "--connect") {
                if (i + 2 >= argc) {
                    std::cerr
                        << "Missing HOST PORT for --connect\\n";
                    return 2;
                }

                connect_host = argv[++i];

                if (!parse_uint16(
                        argv[++i],
                        connect_port)) {
                    std::cerr
                        << "Invalid --connect port value\\n";
                    return 2;
                }

                connect_to_peer = true;
                continue;
            }

            if (arg == "--network-id") {
                if (++i >= argc) {
                    std::cerr
                        << "Missing value for --network-id\n";
                    return 2;
                }

                if (!parse_uint32(
                        argv[i],
                        network_id)) {
                    std::cerr
                        << "Invalid --network-id value\n";
                    return 2;
                }

                continue;
            }

            std::cerr
                << "Usage: caesard [--mine]"
                << " [--mine-after-ms N]"
                << " [--data-dir PATH]"
                << " [--port PORT]"
                << " [--connect HOST PORT]"
                << " [--network-id ID]\n";

            return 2;
        }

        std::signal(
            SIGINT,
            handle_shutdown_signal);

        std::signal(
            SIGTERM,
            handle_shutdown_signal);

        std::cout
            << "=== Caesar CZR Node ===\n";

        caesar::CaesarNode node(
            data_dir,
            p2p_port,
            network_id);

        node.start();

        std::cout
            << "Node: RUNNING\n";

        std::cout
            << "Node: READY\n";

        std::cout
            << "P2P port: "
            << p2p_port
            << "\n";

        std::cout
            << "Blockchain height: "
            << node.height()
            << "\n";

        std::cout
            << "Peers: "
            << node.peer_count()
            << "\n";

        std::cout
            << "Storage: "
            << (data_dir / "blockchain.dat").string()
            << "\n";

        std::cout.flush();

        if (connect_to_peer) {
            std::cout
                << "Connecting to peer: "
                << connect_host
                << ":"
                << connect_port
                << "\\n";

            const auto peer_id =
                node.connect_to_peer(
                    connect_host,
                    connect_port);

            if (peer_id == 0) {
                throw std::runtime_error(
                    "peer connection failed");
            }

            std::cout
                << "Connected to peer. "
                << "Peer ID: "
                << peer_id
                << "\\n";

            std::cout
                << "Peers: "
                << node.peer_count()
                << "\\n";

            std::cout.flush();
        }

        if (mine_one_block || mine_after_delay) {
            if (mine_after_delay) {
                std::cout
                    << "Mining scheduled after "
                    << mine_after_ms
                    << " ms...\n";

                std::cout.flush();

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(
                        mine_after_ms));

                if (!node.running() ||
                    g_shutdown_requested) {
                    throw std::runtime_error(
                        "mining cancelled before start");
                }
            }

            std::cout
                << "Mining one block...\n";

            node.mine_one_block(
                "CAESAR_MINER_CZR1",
                1000000);

            std::cout
                << "Mined successfully.\n";

            std::cout
                << "New blockchain height: "
                << node.height()
                << "\n";

            if (mine_after_delay) {
                std::cout
                    << "Node is running. "
                    << "Press Ctrl+C to stop.\n";

                std::cout.flush();

                while (
                    node.running() &&
                    !g_shutdown_requested) {

                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(100));
                }
            }
        } else {
            std::cout
                << "Node is running. "
                << "Press Ctrl+C to stop.\n";

            std::cout.flush();

            while (
                node.running() &&
                !g_shutdown_requested) {

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100));
            }
        }

        node.stop();

        std::cout
            << "Node: STOPPED\n";

        std::cout.flush();

        return 0;

    } catch (const std::exception& e) {
        std::cerr
            << "Caesar node error: "
            << e.what()
            << "\n";

        return 1;
    }
}
