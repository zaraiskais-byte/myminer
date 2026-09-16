#include <cstdlib>
#include <iostream>
#include <string>

#include <caesar/key_encoding.hpp>
#include <caesar/signature.hpp>

using namespace caesar;

int main() {
    std::cout << "=== Caesar CZR Key Encoding Tests ===\n";

    try {
        KeyPair keys = generate_keypair();

        const std::string encoded =
            public_key_to_hex(keys.public_key);

        if (encoded.size() != 64) {
            std::cerr << "[FAIL] Public key must be 32 bytes / 64 hex chars\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Public key encoded to 64 hex characters\n";

        EVP_PKEY* restored =
            public_key_from_hex(encoded);

        if (!restored) {
            std::cerr << "[FAIL] Public key reconstruction\n";
            return EXIT_FAILURE;
        }

        const std::string restored_hex =
            public_key_to_hex(restored);

        if (restored_hex != encoded) {
            EVP_PKEY_free(restored);
            std::cerr << "[FAIL] Public key round trip\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Public key round trip verified\n";

        const std::string message =
            "Caesar CZR portable key test";

        const auto signature =
            sign_message(keys.private_key, message);

        if (!verify_signature(
                restored,
                message,
                signature)) {

            EVP_PKEY_free(restored);
            std::cerr << "[FAIL] Signature verification with restored key\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Signature verified using reconstructed key\n";

        EVP_PKEY_free(restored);

        std::cout << "ALL KEY ENCODING TESTS PASSED\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
