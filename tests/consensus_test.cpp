#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <caesar/consensus.hpp>

using namespace caesar;

static bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    std::cout << "[PASS] " << message << '\n';
    return true;
}

int main() {
    std::cout << "=== Caesar CZR Consensus Tests ===\n";

    std::vector<std::uint8_t> header{
        0x43, 0x5a, 0x52, 0x01,
        0x10, 0x20, 0x30, 0x40
    };

    const Hash256 hash1 = calculate_pow_hash(header, 123);
    const Hash256 hash2 = calculate_pow_hash(header, 123);
    const Hash256 hash3 = calculate_pow_hash(header, 124);

    if (!check(hash1 == hash2, "PoW is deterministic"))
        return EXIT_FAILURE;

    if (!check(hash1 != hash3, "Nonce changes PoW result"))
        return EXIT_FAILURE;

    if (!check(!pow_meets_difficulty(hash1, 257),
               "Difficulty above 256 rejected"))
        return EXIT_FAILURE;

    std::uint64_t nonce = 0;
    Hash256 mined{};

    if (!check(
            mine_pow(header, 4, 0, 100000, nonce, mined),
            "Low-difficulty PoW can be mined"))
        return EXIT_FAILURE;

    if (!check(
            validate_pow(header, nonce, 4),
            "Mined PoW validates"))
        return EXIT_FAILURE;

    if (!check(
            !validate_pow(header, nonce + 1, 4) ||
            calculate_pow_hash(header, nonce + 1) != mined,
            "Different nonce is independently checked"))
        return EXIT_FAILURE;

    std::cout << "ALL CONSENSUS TESTS PASSED\n";
    return EXIT_SUCCESS;
}
