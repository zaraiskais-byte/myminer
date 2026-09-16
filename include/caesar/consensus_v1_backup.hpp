#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include <caesar/crypto.hpp>
#include <caesar/serialization.hpp>

namespace caesar {

constexpr std::uint32_t CZR_POW_VERSION = 1;
constexpr std::size_t CZR_POW_MEMORY_WORDS = 1024;
constexpr std::uint32_t CZR_POW_ROUNDS = 256;

inline std::uint64_t pow_mix64(std::uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

inline Hash256 calculate_pow_hash(
    const std::vector<std::uint8_t>& header,
    std::uint64_t nonce) {

    BinaryWriter writer;
    writer.write_u32(CZR_POW_VERSION);
    writer.write_bytes(header);
    writer.write_u64(nonce);

    Hash256 state = sha256(bytes_to_binary_string(writer.data()));

    std::array<std::uint64_t, CZR_POW_MEMORY_WORDS> memory{};

    for (std::size_t i = 0; i < memory.size(); ++i) {
        std::uint64_t word = 0;
        for (int j = 0; j < 8; ++j)
            word |= static_cast<std::uint64_t>(
                state[(i + static_cast<std::size_t>(j)) % state.size()])
                << (j * 8);

        memory[i] = pow_mix64(
            word ^ static_cast<std::uint64_t>(i) ^
            (nonce * 0x9e3779b97f4a7c15ULL));
    }

    for (std::uint32_t round = 0; round < CZR_POW_ROUNDS; ++round) {
        const std::size_t a =
            static_cast<std::size_t>(
                state[round % state.size()]) %
            memory.size();

        const std::size_t b =
            static_cast<std::size_t>(
                state[(round + 7) % state.size()]) %
            memory.size();

        memory[a] = pow_mix64(
            memory[a] ^
            memory[b] ^
            static_cast<std::uint64_t>(round) ^
            nonce);

        std::array<std::uint8_t, 32> material = state;

        for (std::size_t j = 0; j < material.size(); ++j) {
            const std::uint64_t word =
                memory[(a + j) % memory.size()];
            material[j] ^= static_cast<std::uint8_t>(
                word >> ((j % 8) * 8));
        }

        state = sha256(bytes_to_binary_string(
            std::vector<std::uint8_t>(
                material.begin(), material.end())));
    }

    return state;
}

inline std::uint32_t count_leading_zero_bits(const Hash256& hash) {
    std::uint32_t count = 0;

    for (std::uint8_t byte : hash) {
        if (byte == 0) {
            count += 8;
            continue;
        }

        for (int bit = 7; bit >= 0; --bit) {
            if ((byte & (static_cast<std::uint8_t>(1) << bit)) == 0)
                ++count;
            else
                return count;
        }
    }

    return count;
}

inline bool pow_meets_difficulty(
    const Hash256& hash,
    std::uint32_t difficulty) {

    if (difficulty > 256)
        return false;

    return count_leading_zero_bits(hash) >= difficulty;
}

inline bool validate_pow(
    const std::vector<std::uint8_t>& header,
    std::uint64_t nonce,
    std::uint32_t difficulty) {

    return pow_meets_difficulty(
        calculate_pow_hash(header, nonce),
        difficulty);
}

inline bool mine_pow(
    const std::vector<std::uint8_t>& header,
    std::uint32_t difficulty,
    std::uint64_t start_nonce,
    std::uint64_t max_attempts,
    std::uint64_t& found_nonce,
    Hash256& found_hash) {

    if (difficulty > 256)
        return false;

    for (std::uint64_t i = 0; i < max_attempts; ++i) {
        if (start_nonce > std::numeric_limits<std::uint64_t>::max() - i)
            break;

        const std::uint64_t nonce = start_nonce + i;
        const Hash256 hash = calculate_pow_hash(header, nonce);

        if (pow_meets_difficulty(hash, difficulty)) {
            found_nonce = nonce;
            found_hash = hash;
            return true;
        }
    }

    return false;
}

} // namespace caesar
