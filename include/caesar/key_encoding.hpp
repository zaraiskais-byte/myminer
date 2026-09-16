#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>

namespace caesar {

inline std::string public_key_to_hex(
    EVP_PKEY* public_key) {

    if (!public_key)
        throw std::runtime_error("missing public key");

    std::size_t size = 0;

    if (EVP_PKEY_get_raw_public_key(
            public_key,
            nullptr,
            &size) != 1) {

        throw std::runtime_error(
            "failed to get public key size");
    }

    std::vector<unsigned char> raw(size);

    if (EVP_PKEY_get_raw_public_key(
            public_key,
            raw.data(),
            &size) != 1) {

        throw std::runtime_error(
            "failed to extract public key");
    }

    static constexpr char hex[] = "0123456789abcdef";

    std::string result;
    result.reserve(size * 2);

    for (std::size_t i = 0; i < size; ++i) {
        result.push_back(hex[(raw[i] >> 4) & 0x0f]);
        result.push_back(hex[raw[i] & 0x0f]);
    }

    return result;
}

inline std::vector<unsigned char> hex_to_bytes(
    const std::string& hex) {

    if (hex.size() % 2 != 0)
        throw std::runtime_error("invalid hex length");

    auto value = [](char c) -> unsigned char {
        if (c >= '0' && c <= '9')
            return static_cast<unsigned char>(c - '0');

        if (c >= 'a' && c <= 'f')
            return static_cast<unsigned char>(c - 'a' + 10);

        if (c >= 'A' && c <= 'F')
            return static_cast<unsigned char>(c - 'A' + 10);

        throw std::runtime_error("invalid hex character");
    };

    std::vector<unsigned char> result;
    result.reserve(hex.size() / 2);

    for (std::size_t i = 0; i < hex.size(); i += 2) {
        result.push_back(
            static_cast<unsigned char>(
                (value(hex[i]) << 4) |
                value(hex[i + 1])));
    }

    return result;
}

inline EVP_PKEY* public_key_from_hex(
    const std::string& hex) {

    const auto raw = hex_to_bytes(hex);

    if (raw.size() != 32)
        throw std::runtime_error(
            "Ed25519 public key must be 32 bytes");

    EVP_PKEY* key =
        EVP_PKEY_new_raw_public_key(
            EVP_PKEY_ED25519,
            nullptr,
            raw.data(),
            raw.size());

    if (!key)
        throw std::runtime_error(
            "failed to create Ed25519 public key");

    return key;
}

} // namespace caesar
