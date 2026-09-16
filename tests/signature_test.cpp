#include <cstdlib>
#include <iostream>
#include <string>

#include <caesar/signature.hpp>

using namespace caesar;

int main() {
    std::cout << "=== Caesar CZR Signature Tests ===\n";

    try {
        KeyPair keys = generate_keypair();

        const std::string message =
            "Caesar CZR transaction authorization";

        const auto signature =
            sign_message(keys.private_key, message);

        if (signature.empty()) {
            std::cerr << "[FAIL] Empty signature\n";
            return EXIT_FAILURE;
        }
        std::cout << "[PASS] Signature generated\n";

        if (!verify_signature(
                keys.public_key,
                message,
                signature)) {

            std::cerr << "[FAIL] Valid signature rejected\n";
            return EXIT_FAILURE;
        }
        std::cout << "[PASS] Valid signature verified\n";

        if (verify_signature(
                keys.public_key,
                message + " modified",
                signature)) {

            std::cerr << "[FAIL] Modified message accepted\n";
            return EXIT_FAILURE;
        }
        std::cout << "[PASS] Modified message rejected\n";

        std::cout << "ALL SIGNATURE TESTS PASSED\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
