#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#include <caesar/crypto.hpp>
#include <caesar/serialization.hpp>

namespace caesar {

constexpr std::uint32_t CZR_POW_VERSION = 2;
constexpr std::size_t CZR_POW_MEMORY_WORDS = 65536;
constexpr std::uint32_t CZR_POW_ROUNDS = 4;

inline std::uint64_t pow_mix64(std::uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

inline std::uint64_t pow_load64(
    const Hash256& state,
    std::size_t offset) {

    std::uint64_t value = 0;

    for (std::size_t i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(
            state[(offset + i) % state.size()]) << (i * 8);

    return value;
}

inline Hash256 calculate_pow_hash(
    const std::vector<std::uint8_t>& header,
    std::uint64_t nonce) {

    BinaryWriter writer;
    writer.write_u32(CZR_POW_VERSION);
    writer.write_bytes(header);
    writer.write_u64(nonce);

    Hash256 state =
        sha256(bytes_to_binary_string(writer.data()));

    std::vector<std::uint64_t> memory(
        CZR_POW_MEMORY_WORDS);

    const std::uint64_t seed =
        pow_load64(state, 0) ^
        pow_load64(state, 8) ^
        nonce;

    for (std::size_t i = 0; i < memory.size(); ++i) {
        const std::uint64_t previous =
            (i == 0) ? seed : memory[i - 1];

        memory[i] = pow_mix64(
            previous ^
            pow_load64(state, i & 24) ^
            static_cast<std::uint64_t>(i) *
                0x9e3779b97f4a7c15ULL ^
            nonce);
    }

    for (std::uint32_t round = 0;
         round < CZR_POW_ROUNDS;
         ++round) {

        std::uint64_t accumulator =
            pow_load64(state, round * 8);

        for (std::size_t i = 0;
             i < memory.size();
             ++i) {

            const std::size_t index =
                static_cast<std::size_t>(
                    pow_mix64(
                        accumulator ^
                        memory[i] ^
                        nonce ^
                        static_cast<std::uint64_t>(round))) %
                memory.size();

            const std::size_t other =
                (index ^
                 (i * 0x9e3779b9U)) %
                memory.size();

            const std::uint64_t mixed =
                pow_mix64(
                    memory[index] ^
                    memory[other] ^
                    accumulator ^
                    static_cast<std::uint64_t>(i));

            memory[index] = mixed;
            accumulator =
                pow_mix64(accumulator ^ mixed);
        }

        BinaryWriter round_writer;
        round_writer.write_u64(accumulator);

        for (std::size_t i = 0; i < 8; ++i)
            round_writer.write_u64(
                memory[
                    (static_cast<std::size_t>(
                        accumulator >> (i * 8))) %
                    memory.size()]);

        state = sha256(
            bytes_to_binary_string(
                round_writer.data()));
    }

    BinaryWriter final_writer;
    final_writer.write_u32(CZR_POW_VERSION);
    final_writer.write_bytes(
        std::vector<std::uint8_t>(
            state.begin(), state.end()));
    final_writer.write_u64(nonce);

    for (std::size_t i = 0; i < 8; ++i)
        final_writer.write_u64(
            memory[
                (static_cast<std::size_t>(
                    pow_load64(state, i * 4))) %
                memory.size()]);

    return sha256(
        bytes_to_binary_string(
            final_writer.data()));
}

inline std::uint32_t count_leading_zero_bits(
    const Hash256& hash) {

    std::uint32_t count = 0;

    for (std::uint8_t byte : hash) {
        if (byte == 0) {
            count += 8;
            continue;
        }

        for (int bit = 7; bit >= 0; --bit) {
            if ((byte &
                 (static_cast<std::uint8_t>(1) << bit)) == 0)
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

    for (std::uint64_t i = 0;
         i < max_attempts;
         ++i) {

        if (start_nonce >
            std::numeric_limits<std::uint64_t>::max() - i)
            break;

        const std::uint64_t nonce =
            start_nonce + i;

        const Hash256 hash =
            calculate_pow_hash(header, nonce);

        if (pow_meets_difficulty(hash, difficulty)) {
            found_nonce = nonce;
            found_hash = hash;
            return true;
        }
    }

    return false;
}


constexpr std::size_t CZR_DIFFICULTY_WINDOW = 11;
constexpr std::uint64_t CZR_TARGET_BLOCK_TIME = 120;
constexpr std::uint32_t CZR_MIN_DIFFICULTY = 0;
constexpr std::uint32_t CZR_MAX_DIFFICULTY = 256;

inline std::uint64_t difficulty_median_interval(
    std::vector<std::uint64_t> intervals) {

    if (intervals.size() != CZR_DIFFICULTY_WINDOW)
        return 0;

    std::sort(intervals.begin(), intervals.end());

    return intervals[intervals.size() / 2];
}

inline std::uint32_t adjust_difficulty_window(
    std::uint32_t current_difficulty,
    const std::vector<std::uint64_t>& intervals) {

    if (current_difficulty > CZR_MAX_DIFFICULTY ||
        intervals.size() != CZR_DIFFICULTY_WINDOW)
        return current_difficulty;

    std::uint64_t observed =
        difficulty_median_interval(intervals);

    if (observed == 0)
        observed = 1;

    const std::uint64_t min_time =
        CZR_TARGET_BLOCK_TIME / 4;

    const std::uint64_t max_time =
        CZR_TARGET_BLOCK_TIME * 4;

    observed = std::max(
        min_time,
        std::min(observed, max_time));

    std::uint32_t next = current_difficulty;

    if (observed > CZR_TARGET_BLOCK_TIME) {
        while (observed >= CZR_TARGET_BLOCK_TIME * 2ULL &&
               next > CZR_MIN_DIFFICULTY) {
            observed /= 2;
            --next;
        }
    }
    else if (observed < CZR_TARGET_BLOCK_TIME) {
        while (observed * 2ULL <= CZR_TARGET_BLOCK_TIME &&
               next < CZR_MAX_DIFFICULTY) {
            observed *= 2;
            ++next;
        }
    }

    return next;
}

} // namespace caesar
