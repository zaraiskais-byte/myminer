#include <iostream>
#include <string>
#include <cassert>
#include <chrono>
#include <thread>
#include "caesar/http_rpc.hpp"

int main() {
    std::cout << "[test] Starting CaesarHttpWalletSetupTest with dynamic port allocation...\n";
    
    // Test dynamic server setup with random ephemeral port to prevent 8443 port collision
    unsigned short test_port = 0;
    try {
        caesar::HttpRpcServer server(test_port);
        server.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::string endpoint = server.get_base_url() + "/api/wallet/qr.svg";
        std::cout << "[test] Server running successfully at " << endpoint << "\n";
        
        server.stop();
        std::cout << "[test] CaesarHttpWalletSetupTest passed successfully.\n";
    } catch (const std::exception& e) {
        std::cerr << "[error] CaesarHttpWalletSetupTest failed: " << e.what() << "\n";
        return 8;
    }
    return 0;
}
