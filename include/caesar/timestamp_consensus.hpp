#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <caesar/block.hpp>

namespace caesar {

/*
 * Maximum drift a block's timestamp is allowed to be ahead of the
 * receiving node's wall clock, expressed in seconds. Two hours is the
 * value used by Bitcoin. Tightening it below the observed peer-to-peer
 * clock skew would reject valid blocks; loosening it opens the door
 * to timestamp manipulation for difficulty retargeting.
 */
inline constexpr std::uint64_t CZR_MAX_FUTURE_DRIFT = 2 * 60 * 60;

/*
 * Number of trailing blocks sampled when computing median-time-past
 * (MTP). Matches Bitcoin's 11-block window and reuses the same size
 * as the difficulty window so the two do not drift apart silently.
 * The constants are kept separate in case future consensus wants
 * them to differ.
 */
inline constexpr std::size_t CZR_MEDIAN_TIME_WINDOW = 11;

/*
 * Returns the median of a vector of timestamps.
 *
 * For an odd-sized input this is the middle value after sorting.
 * For an even-sized input this returns the element at index N/2,
 * i.e. the upper of the two middle values. This matches Bitcoin's
 * definition of MedianTimePast and avoids introducing a fractional
 * value for a protocol that only deals in integer seconds.
 *
 * Takes the vector by value so the caller's data is not reordered.
 */
inline std::uint64_t median_of_timestamps(std::vector<std::uint64_t> values) noexcept {
    if (values.empty())
        return 0;

    std::sort(values.begin(), values.end());

    return values[values.size() / 2];
}

/*
 * Computes median-time-past for a chain.
 *
 * Uses the last CZR_MEDIAN_TIME_WINDOW timestamps of the chain, or
 * fewer if the chain is shorter. Returns 0 for an empty chain.
 *
 * MTP is strictly stronger than "previous block's timestamp": an
 * attacker who controls the last block cannot lower the required
 * floor below the median of the preceding window, which limits how
 * far they can push timestamps around for difficulty retargeting.
 */
inline std::uint64_t compute_median_time_past(const std::vector<Block>& chain) noexcept {
    if (chain.empty())
        return 0;

    const std::size_t sample_size = std::min(chain.size(), CZR_MEDIAN_TIME_WINDOW);

    const std::size_t start = chain.size() - sample_size;

    std::vector<std::uint64_t> timestamps;
    timestamps.reserve(sample_size);

    for (std::size_t i = start; i < chain.size(); ++i) {
        timestamps.push_back(chain[i].header.timestamp);
    }

    return median_of_timestamps(std::move(timestamps));
}

/*
 * Lowest timestamp a valid next block is allowed to carry.
 *
 * Consensus rule: block.timestamp must be STRICTLY GREATER than the
 * median-time-past of the chain it extends. Equality is forbidden so
 * a miner cannot freeze the clock by always reusing the current MTP.
 */
inline std::uint64_t minimum_allowed_timestamp(const std::vector<Block>& chain) noexcept {
    if (chain.empty())
        return 0;

    return compute_median_time_past(chain) + 1;
}

/*
 * Highest timestamp a valid block is allowed to carry given the
 * receiving node's current wall-clock time.
 */
inline std::uint64_t maximum_allowed_timestamp(std::uint64_t now) noexcept {
    return now + CZR_MAX_FUTURE_DRIFT;
}

/*
 * Full canonical timestamp check for a block that extends `chain`.
 *
 * Rules:
 *   - block.timestamp <= now + CZR_MAX_FUTURE_DRIFT
 *   - block.timestamp >  median-time-past(chain)   (if chain non-empty)
 *
 * This function is intentionally separate from the weaker
 * "timestamp >= previous.timestamp" check still used in the mining,
 * storage, and validator paths. It is currently opt-in and exercised
 * only by tests. Wiring it into production validation is a follow-up
 * so the protocol change can be rolled out deliberately.
 *
 * Returns true if the block satisfies both rules.
 */
inline bool validate_block_timestamp_canonical(const Block& block, const std::vector<Block>& chain,
                                               std::uint64_t now) noexcept {
    if (block.header.timestamp > maximum_allowed_timestamp(now)) {
        return false;
    }

    if (!chain.empty() && block.header.timestamp <= compute_median_time_past(chain)) {
        return false;
    }

    return true;
}

} // namespace caesar
