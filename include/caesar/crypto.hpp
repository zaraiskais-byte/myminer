#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

#include <openssl/evp.h>

namespace caesar {

using Hash256 = std::array<std::uint8_t, 32>;

inline Hash256 sha256(const std::string& input) {
    Hash256 result{};
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();

    if (!ctx)
        throw std::runtime_error("failed to create SHA-256 context");

    unsigned int size = 0;

    const bool ok =
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
        EVP_DigestUpdate(ctx, input.data(), input.size()) == 1 &&
        EVP_DigestFinal_ex(ctx, result.data(), &size) == 1 &&
        size == result.size();

    EVP_MD_CTX_free(ctx);

    if (!ok)
        throw std::runtime_error("SHA-256 calculation failed");

    return result;
}

inline std::string hash_to_hex(const Hash256& hash) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');

    for (std::uint8_t byte : hash)
        out << std::setw(2) << static_cast<unsigned int>(byte);

    return out.str();
}

inline Hash256 hash_pair(const Hash256& left, const Hash256& right) {
    std::string data;
    data.reserve(64);
    data.append(reinterpret_cast<const char*>(left.data()), left.size());
    data.append(reinterpret_cast<const char*>(right.data()), right.size());
    return sha256(data);
}

inline Hash256 merkle_root(std::vector<Hash256> hashes) {
    if (hashes.empty())
        return sha256("");

    while (hashes.size() > 1) {
        std::vector<Hash256> next;
        next.reserve((hashes.size() + 1) / 2);

        for (std::size_t i = 0; i < hashes.size(); i += 2) {
            const Hash256& left = hashes[i];
            const Hash256& right =
                (i + 1 < hashes.size()) ? hashes[i + 1] : hashes[i];

            next.push_back(hash_pair(left, right));
        }

        hashes = std::move(next);
    }

    return hashes.front();
}

} // namespace caesar
