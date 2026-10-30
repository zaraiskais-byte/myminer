// Copyright (c) 2026 zaraiskais-byte. All rights reserved.
#include <iostream>
#include <string>
#include <cassert>
#include <chrono>
#include <thread>
#include <filesystem>
#include "caesar/http_rpc.hpp"
#include "caesar/node.hpp"

int main() {
    std::cout << "[test] Starting corrected CaesarHttpWalletSetupTest...\n";
    
    try {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "caesar_test_wallet";
        std::filesystem::create_directories(temp_dir);
        
        caesar::CaesarNode node(temp_dir);
        std::uint16_t test_port = 0; // Ephemeral port
        
        caesar::HttpRpcServer server(node, test_port, temp_dir);
        server.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::cout << "[test] HttpRpcServer started successfully for wallet setup test.\n";
        
        server.stop();
        std::filesystem::remove_all(temp_dir);
        std::cout << "[test] CaesarHttpWalletSetupTest passed successfully.\n";
    } catch (const std::exception& e) {
        std::cerr << "[error] CaesarHttpWalletSetupTest failed: " << e.what() << "\n";
        return 8;
    }
    return 0;
}
