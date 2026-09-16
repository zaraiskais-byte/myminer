#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <caesar/crypto.hpp>

int main() {
    using namespace caesar;

    std::cout << "=== Caesar CZR Protocol Data Tests ===\n";

    const Hash256 h1 = sha256("transaction-1");
    const Hash256 h2 = sha256("transaction-2");
    const Hash256 h3 = sha256("transaction-3");

    if (h1.size() != 32 || h2.size() != 32 || h3.size() != 32) {
        std::cerr << "[FAIL] Hash size\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Hash256 is 32 bytes\n";

    if (h1 == h2 || h1 == h3 || h2 == h3) {
        std::cerr << "[FAIL] Hash uniqueness\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Hash uniqueness\n";

    const auto root_a = merkle_root({h1, h2, h3});
    const auto root_b = merkle_root({h1, h2, h3});

    if (root_a != root_b) {
        std::cerr << "[FAIL] Merkle determinism\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Merkle determinism\n";

    if (merkle_root({h1, h2}) == merkle_root({h2, h1})) {
        std::cerr << "[FAIL] Merkle order sensitivity\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Merkle order sensitivity\n";

    std::cout << "Merkle root: " << hash_to_hex(root_a) << '\n';
    std::cout << "ALL PROTOCOL DATA TESTS PASSED\n";

    return EXIT_SUCCESS;
}
