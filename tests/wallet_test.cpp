#include <cstdlib>
#include <iostream>
#include <string>

#include <caesar/wallet.hpp>

using namespace caesar;

int main() {
    std::cout << "=== Caesar CZR Wallet Tests ===\n";

    try {
        Wallet alice;
        Wallet bob;

        if (!alice.valid()) {
            std::cerr << "[FAIL] Alice wallet invalid\n";
            return EXIT_FAILURE;
        }
        std::cout << "[PASS] Alice wallet generated\n";

        if (!bob.valid()) {
            std::cerr << "[FAIL] Bob wallet invalid\n";
            return EXIT_FAILURE;
        }
        std::cout << "[PASS] Bob wallet generated\n";

        const std::string alice_key =
            alice.public_key();

        const std::string bob_key =
            bob.public_key();

        if (alice_key.size() != 64 ||
            bob_key.size() != 64) {

            std::cerr << "[FAIL] Ed25519 public key size\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Real Ed25519 public keys extracted\n";

        if (alice_key == bob_key) {
            std::cerr << "[FAIL] Duplicate public keys\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Public keys are unique\n";

        const std::string alice_address =
            alice.address();

        const std::string bob_address =
            bob.address();

        if (alice_address == bob_address) {
            std::cerr << "[FAIL] Duplicate addresses\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Wallet addresses are unique\n";

        if (!is_valid_address(alice_address) ||
            !is_valid_address(bob_address)) {

            std::cerr << "[FAIL] Address validation\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Wallet addresses validated\n";

        if (alice_address !=
            address_from_public_key(alice_key)) {

            std::cerr << "[FAIL] Alice address derivation\n";
            return EXIT_FAILURE;
        }

        if (bob_address !=
            address_from_public_key(bob_key)) {

            std::cerr << "[FAIL] Bob address derivation\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Address is derived from real public key\n";

        std::cout << "Alice address: "
                  << alice_address << '\n';

        std::cout << "Bob address: "
                  << bob_address << '\n';

        std::cout << "ALL WALLET TESTS PASSED\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
