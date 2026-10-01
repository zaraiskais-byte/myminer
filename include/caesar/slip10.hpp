#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace caesar::slip10 {

struct ExtendedKey {
    std::array<uint8_t, 32> key;         // For ed25519: 32-byte private seed
    std::array<uint8_t, 32> chain_code;  // 32-byte chain code
};

// ============================================================
// Master key: I = HMAC-SHA512("ed25519 seed", seed)
// ============================================================
inline ExtendedKey master_key_from_seed(const uint8_t* seed, size_t seed_len) {
    static constexpr char curve[] = "ed25519 seed";
    static constexpr size_t curve_len = sizeof(curve) - 1;

    unsigned int out_len = 64;
    std::array<uint8_t, 64> I{};

    if (HMAC(EVP_sha512(), curve, static_cast<int>(curve_len),
             seed, seed_len, I.data(), &out_len) == nullptr || out_len != 64) {
        throw std::runtime_error("slip10: master HMAC-SHA512 failed");
    }

    ExtendedKey ek;
    std::copy(I.begin(), I.begin() + 32, ek.key.begin());
    std::copy(I.begin() + 32, I.end(), ek.chain_code.begin());
    return ek;
}

// ============================================================
// Hardened child: I = HMAC-SHA512(c_par, 0x00 || k_par || ser32(i))
// ============================================================
inline ExtendedKey derive_hardened_child(const ExtendedKey& parent, uint32_t index) {
    if ((index & 0x80000000u) == 0) {
        throw std::runtime_error("slip10: ed25519 requires hardened index (>= 0x80000000)");
    }

    std::array<uint8_t, 37> data{};
    data[0] = 0x00;
    std::copy(parent.key.begin(), parent.key.end(), data.begin() + 1);
    data[33] = static_cast<uint8_t>((index >> 24) & 0xff);
    data[34] = static_cast<uint8_t>((index >> 16) & 0xff);
    data[35] = static_cast<uint8_t>((index >>  8) & 0xff);
    data[36] = static_cast<uint8_t>( index        & 0xff);

    unsigned int out_len = 64;
    std::array<uint8_t, 64> I{};

    if (HMAC(EVP_sha512(),
             parent.chain_code.data(), static_cast<int>(parent.chain_code.size()),
             data.data(), data.size(),
             I.data(), &out_len) == nullptr || out_len != 64) {
        throw std::runtime_error("slip10: child HMAC-SHA512 failed");
    }

    ExtendedKey ek;
    std::copy(I.begin(), I.begin() + 32, ek.key.begin());
    std::copy(I.begin() + 32, I.end(), ek.chain_code.begin());
    return ek;
}

// ============================================================
// Parse "m/44'/7999'/0'/0'/0'" style paths
// ============================================================
inline std::vector<uint32_t> parse_path(const std::string& path) {
    std::vector<uint32_t> indices;

    if (path.empty() || path[0] != 'm') {
        throw std::runtime_error("slip10: path must start with 'm'");
    }

    size_t pos = 1;
    while (pos < path.size()) {
        if (path[pos] != '/') {
            throw std::runtime_error("slip10: expected '/'");
        }
        pos++;

        uint32_t idx = 0;
        bool has_digit = false;
        bool has_hardened = false;

        while (pos < path.size() && path[pos] != '/') {
            char c = path[pos];
            if (c == '\'' || c == 'h' || c == 'H') {
                has_hardened = true;
                pos++;
                break;
            }
            if (c < '0' || c > '9') {
                throw std::runtime_error("slip10: invalid char in path");
            }
            idx = idx * 10u + static_cast<uint32_t>(c - '0');
            if (idx >= 0x80000000u) {
                throw std::runtime_error("slip10: index overflow");
            }
            has_digit = true;
            pos++;
        }

        if (!has_digit) {
            throw std::runtime_error("slip10: missing index");
        }

        if (has_hardened) idx |= 0x80000000u;
        indices.push_back(idx);
    }

    return indices;
}

// ============================================================
// Derive a full path from a 64-byte seed
// ============================================================
inline ExtendedKey derive_path(const uint8_t* seed, size_t seed_len,
                                const std::string& path) {
    ExtendedKey ek = master_key_from_seed(seed, seed_len);
    for (uint32_t idx : parse_path(path)) {
        if ((idx & 0x80000000u) == 0) {
            throw std::runtime_error("slip10: ed25519 only supports hardened derivation");
        }
        ek = derive_hardened_child(ek, idx);
    }
    return ek;
}

// Default path for Caesar CZR (SLIP-0010 hardened only).
// Note: coin type 0 is a placeholder; a real SLIP-44 number should be
// registered for CAESAR CZR before mainnet release.
inline constexpr const char* DEFAULT_PATH = "m/44'/7999'/0'/0'/0'";

}  // namespace caesar::slip10
