#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <caesar/crypto.hpp>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size) {

    if (data == nullptr)
        return 0;

    try {
        std::string input(
            reinterpret_cast<const char*>(data),
            size);

        // SHA-256 must be deterministic.
        const caesar::Hash256 hash1 =
            caesar::sha256(input);

        const caesar::Hash256 hash2 =
            caesar::sha256(input);

        if (hash1 != hash2)
            __builtin_trap();

        // Hex encoding must always represent 32 bytes as 64 hex chars.
        const std::string hex =
            caesar::hash_to_hex(hash1);

        if (hex.size() != 64)
            __builtin_trap();

        // Derive a second hash from the same fuzz input.
        const caesar::Hash256 other =
            caesar::sha256(input + "CAESAR");

        // Pair hashing must also be deterministic.
        const caesar::Hash256 pair1 =
            caesar::hash_pair(hash1, other);

        const caesar::Hash256 pair2 =
            caesar::hash_pair(hash1, other);

        if (pair1 != pair2)
            __builtin_trap();

        // Build a Merkle tree from fuzz-derived hashes.
        std::vector<caesar::Hash256> leaves;

        if (size == 0) {
            leaves.push_back(hash1);
        } else {
            const std::size_t count =
                (size % 16) + 1;

            leaves.reserve(count);

            for (std::size_t i = 0; i < count; ++i) {
                std::string leaf_data = input;
                leaf_data.push_back(
                    static_cast<char>(i));

                leaves.push_back(
                    caesar::sha256(leaf_data));
            }
        }

        const caesar::Hash256 root1 =
            caesar::merkle_root(leaves);

        const caesar::Hash256 root2 =
            caesar::merkle_root(leaves);

        if (root1 != root2)
            __builtin_trap();

        const std::string root_hex =
            caesar::hash_to_hex(root1);

        if (root_hex.size() != 64)
            __builtin_trap();

    } catch (...) {
        // Malformed fuzz input must not crash the target.
    }

    return 0;
}
