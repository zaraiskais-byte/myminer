#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/block_builder.hpp>

namespace caesar {

/*
 * Exact unsigned 512-bit integer sufficient for cumulative CZR
 * chainwork without requiring an external multiprecision library.
 *
 * A single block contributes 2^difficulty, with difficulty <= 256.
 * The representation has eight 64-bit limbs, little-endian.
 */
class ChainWork {
public:
    ChainWork() = default;

    explicit ChainWork(std::uint64_t value) {
        limbs_[0] = value;
    }

    static ChainWork power_of_two(std::uint32_t exponent) {
        if (exponent >= 512)
            throw std::runtime_error(
                "chain work exponent exceeds 512 bits");

        ChainWork result;
        result.limbs_[exponent / 64] =
            std::uint64_t{1} << (exponent % 64);
        return result;
    }

    ChainWork& operator+=(const ChainWork& other) {
        std::uint64_t carry = 0;

        for (std::size_t i = 0; i < limbs_.size(); ++i) {
            const std::uint64_t a = limbs_[i];
            const std::uint64_t b = other.limbs_[i];

            const std::uint64_t sum = a + b;
            const std::uint64_t carry1 = sum < a ? 1 : 0;

            const std::uint64_t sum2 = sum + carry;
            const std::uint64_t carry2 = sum2 < sum ? 1 : 0;

            limbs_[i] = sum2;
            carry = (carry1 | carry2);
        }

        if (carry != 0)
            throw std::overflow_error(
                "cumulative chain work exceeds 512 bits");

        return *this;
    }

    friend ChainWork operator+(
        ChainWork lhs,
        const ChainWork& rhs) {

        lhs += rhs;
        return lhs;
    }

    friend bool operator==(
        const ChainWork& lhs,
        const ChainWork& rhs) {

        return lhs.limbs_ == rhs.limbs_;
    }

    friend bool operator!=(
        const ChainWork& lhs,
        const ChainWork& rhs) {

        return !(lhs == rhs);
    }

    friend bool operator<(
        const ChainWork& lhs,
        const ChainWork& rhs) {

        for (std::size_t i = lhs.limbs_.size(); i-- > 0;) {
            if (lhs.limbs_[i] < rhs.limbs_[i])
                return true;
            if (lhs.limbs_[i] > rhs.limbs_[i])
                return false;
        }

        return false;
    }

    friend bool operator>(
        const ChainWork& lhs,
        const ChainWork& rhs) {

        return rhs < lhs;
    }

    friend bool operator<=(
        const ChainWork& lhs,
        const ChainWork& rhs) {

        return !(rhs < lhs);
    }

    friend bool operator>=(
        const ChainWork& lhs,
        const ChainWork& rhs) {

        return !(lhs < rhs);
    }

private:
    std::array<std::uint64_t, 8> limbs_{};
};

inline ChainWork block_work(const Block& block) {
    if (block.header.difficulty > CZR_MAX_DIFFICULTY)
        throw std::runtime_error(
            "invalid difficulty for chain work");

    return ChainWork::power_of_two(
        block.header.difficulty);
}

inline ChainWork cumulative_chain_work(
    const std::vector<Block>& chain) {

    if (chain.empty())
        throw std::runtime_error(
            "cannot calculate chain work for empty chain");

    ChainWork total;

    for (const auto& block : chain)
        total += block_work(block);

    return total;
}

inline bool has_more_work(
    const std::vector<Block>& candidate,
    const std::vector<Block>& current) {

    if (candidate.empty())
        return false;

    if (current.empty())
        return true;

    return cumulative_chain_work(candidate) >
           cumulative_chain_work(current);
}

/*
 * Select a candidate only after validating the complete candidate chain.
 *
 * Chainwork is a selection metric, not a consensus substitute.
 */
inline bool select_higher_work_chain(
    const std::vector<Block>& candidate,
    const std::vector<Block>& current) {

    if (candidate.empty())
        return false;

    if (!validate_candidate_chain(candidate))
        return false;

    if (current.empty())
        return true;

    if (!validate_candidate_chain(current))
        return false;

    return has_more_work(candidate, current);
}

} // namespace caesar
