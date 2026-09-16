#include <cstdlib>
#include <iostream>
#include <string>

#include <openssl/evp.h>

std::string sha256(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return {};

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int size = 0;

    bool ok =
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
        EVP_DigestUpdate(ctx, input.data(), input.size()) == 1 &&
        EVP_DigestFinal_ex(ctx, digest, &size) == 1;

    EVP_MD_CTX_free(ctx);

    if (!ok) return {};

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);

    for (unsigned int i = 0; i < size; ++i) {
        out += hex[digest[i] >> 4];
        out += hex[digest[i] & 0x0f];
    }

    return out;
}

int main() {
    std::cout << "=== Caesar CZR Cryptographic Tests ===\n";

    const std::string expected =
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";

    if (sha256("abc") != expected) {
        std::cerr << "[FAIL] SHA-256 known-answer test\n";
        return EXIT_FAILURE;
    }

    std::cout << "[PASS] SHA-256 known-answer test\n";
    std::cout << "ALL CRYPTOGRAPHIC FOUNDATION TESTS PASSED\n";

    return EXIT_SUCCESS;
}
