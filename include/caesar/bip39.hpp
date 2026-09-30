#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/evp.h>

#include <caesar/bip39_wordlist.hpp>

namespace caesar::bip39 {

inline std::array<uint8_t, 32> sha256_bytes(const uint8_t* data, size_t len) {
    std::array<uint8_t, 32> out{};
    unsigned int out_len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("bip39: SHA-256 ctx failed");
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data, len) != 1 ||
        EVP_DigestFinal_ex(ctx, out.data(), &out_len) != 1 ||
        out_len != 32) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("bip39: SHA-256 failed");
    }
    EVP_MD_CTX_free(ctx);
    return out;
}

// ============================================================
// Entropy -> mnemonic
// ============================================================
inline std::string entropy_to_mnemonic(const std::vector<uint8_t>& entropy) {
    if (entropy.size() != 16 && entropy.size() != 20 &&
        entropy.size() != 24 && entropy.size() != 28 &&
        entropy.size() != 32) {
        throw std::runtime_error("bip39: invalid entropy size");
    }

    const auto hash = sha256_bytes(entropy.data(), entropy.size());

    const size_t entropy_bits = entropy.size() * 8;
    const size_t checksum_bits = entropy_bits / 32;
    const size_t total_bits = entropy_bits + checksum_bits;
    const size_t word_count = total_bits / 11;

    std::string mnemonic;
    mnemonic.reserve(word_count * 8);

    for (size_t i = 0; i < word_count; ++i) {
        uint16_t index = 0;
        for (size_t j = 0; j < 11; ++j) {
            const size_t pos = i * 11 + j;
            uint8_t bit;
            if (pos < entropy_bits) {
                const size_t b = pos / 8;
                const size_t k = 7 - (pos % 8);
                bit = (entropy[b] >> k) & 1;
            } else {
                const size_t c = pos - entropy_bits;
                const size_t b = c / 8;
                const size_t k = 7 - (c % 8);
                bit = (hash[b] >> k) & 1;
            }
            index = static_cast<uint16_t>((index << 1) | bit);
        }
        if (i > 0) mnemonic += ' ';
        mnemonic += WORDLIST[index];
    }

    return mnemonic;
}

// ============================================================
// Mnemonic -> 64-byte seed (PBKDF2-HMAC-SHA512, 2048 iterations)
// ============================================================
inline std::array<uint8_t, 64> mnemonic_to_seed(
    const std::string& mnemonic,
    const std::string& passphrase = "") {

    std::array<uint8_t, 64> seed{};
    std::string salt = "mnemonic" + passphrase;

    if (PKCS5_PBKDF2_HMAC(
            mnemonic.data(), static_cast<int>(mnemonic.size()),
            reinterpret_cast<const unsigned char*>(salt.data()),
            static_cast<int>(salt.size()),
            2048, EVP_sha512(), 64, seed.data()) != 1) {
        throw std::runtime_error("bip39: PBKDF2-HMAC-SHA512 failed");
    }

    return seed;
}

// ============================================================
// Helper: split on whitespace
// ============================================================
inline std::vector<std::string> split_words(const std::string& s) {
    std::vector<std::string> words;
    std::string cur;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!cur.empty()) { words.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) words.push_back(cur);
    return words;
}

// ============================================================
// Validate mnemonic (word count + list membership + checksum)
// ============================================================
inline bool validate_mnemonic(const std::string& mnemonic) {
    try {
        const auto words = split_words(mnemonic);
        if (words.size() != 12 && words.size() != 15 &&
            words.size() != 18 && words.size() != 21 &&
            words.size() != 24) {
            return false;
        }

        const size_t total_bits = words.size() * 11;
        const size_t checksum_bits = total_bits / 33;
        const size_t entropy_bits = total_bits - checksum_bits;
        const size_t entropy_bytes = entropy_bits / 8;

        std::vector<uint8_t> bits;
        bits.reserve(total_bits);

        for (const auto& w : words) {
            uint16_t idx = 0;
            bool found = false;
            for (size_t i = 0; i < WORDLIST.size(); ++i) {
                if (WORDLIST[i] == w) { idx = static_cast<uint16_t>(i); found = true; break; }
            }
            if (!found) return false;
            for (int b = 10; b >= 0; --b) {
                bits.push_back(static_cast<uint8_t>((idx >> b) & 1));
            }
        }

        std::vector<uint8_t> entropy(entropy_bytes, 0);
        for (size_t i = 0; i < entropy_bits; ++i) {
            if (bits[i]) entropy[i / 8] |= static_cast<uint8_t>(1u << (7 - (i % 8)));
        }

        const auto hash = sha256_bytes(entropy.data(), entropy.size());

        for (size_t i = 0; i < checksum_bits; ++i) {
            const uint8_t expected =
                static_cast<uint8_t>((hash[i / 8] >> (7 - (i % 8))) & 1);
            if (bits[entropy_bits + i] != expected) return false;
        }

        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace caesar::bip39
