#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <caesar/node.hpp>

int main(int argc, char** argv) {
    try {
        bool mine_one_block = false;

        if (argc > 1) {
            if (std::string(argv[1]) == "--mine") {
                mine_one_block = true;
            } else {
                std::cerr << "Usage: caesard [--mine]\n";
                return 2;
            }
        }

        std::cout << "=== Caesar CZR Node ===\n";

        caesar::CaesarNode node("data", 18444, 1);
        node.start();

        std::cout << "Node: RUNNING\n";
        std::cout << "Blockchain height: "
                  << node.height() << "\n";
        std::cout << "Peers: "
                  << node.peer_count() << "\n";

        if (mine_one_block) {
            std::cout << "Mining one block...\n";

            node.mine_one_block(
                "CAESAR_MINER_CZR1",
                1000000);

            std::cout << "Mined successfully.\n";
            std::cout << "New blockchain height: "
                      << node.height() << "\n";
        }

        std::cout << "Storage: data/blockchain.dat\n";

        if (!mine_one_block) {
            std::cout << "Node is running. Press Ctrl+C to stop.\n";

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
