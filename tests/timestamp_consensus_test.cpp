/*
 * Timestamp consensus regression test.
 *
 * Caesar CZR's original timestamp rule was simply:
 *
 *     block.timestamp >= previous.timestamp
 *
 * That leaves the protocol open to two problems:
 *
 *   1. A miner can set block.timestamp arbitrarily far in the future
 *      (the check is only against the previous block, not the wall
 *      clock), which lets them manipulate the difficulty retarget.
 *
 *   2. A single previous block's timestamp is easy to control; using
 *      the median of the last 11 timestamps is strictly stronger.
 *
 * timestamp_consensus.hpp introduces:
 *
 *   CZR_MAX_FUTURE_DRIFT        - 2h wall-clock bound
 *   CZR_MEDIAN_TIME_WINDOW      - 11
 *   median_of_timestamps()      - pure median
 *   compute_median_time_past()  - MTP over chain
 *   minimum_allowed_timestamp() - MTP + 1
 *   maximum_allowed_timestamp() - now + drift
 *   validate_block_timestamp_canonical()
 *
 * This test exercises each rule with fixed inputs so the semantics do
 * not silently drift. It does not rely on the wall clock, so it is
 * deterministic.
 */

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/timestamp_consensus.hpp>

using namespace caesar;

namespace {

Block make_block(std::uint64_t timestamp) {
    Block b;
    b.header.timestamp = timestamp;
    return b;
}

Block chain_with_last_timestamps(const std::vector<std::uint64_t>& ts) {
    Block b;
    for (auto t : ts) {
        b.header.timestamp = t;
    }
    return b;
}

} // namespace

int main() {
    // -----------------------------------------------------------------
    // 1. median_of_timestamps: odd and even behaviour
    // -----------------------------------------------------------------
    assert(median_of_timestamps({}) == 0);
    assert(median_of_timestamps({5}) == 5);
    assert(median_of_timestamps({3, 1, 2}) == 2);

    // Even count: upper of the two middle values (Bitcoin convention).
    assert(median_of_timestamps({1, 2, 3, 4}) == 3);
    assert(median_of_timestamps({10, 20, 30, 40, 50, 60}) == 40);

    std::cout << "[ts] median cases: OK\n";

    // -----------------------------------------------------------------
    // 2. compute_median_time_past
    // -----------------------------------------------------------------
    {
        std::vector<Block> empty;
        assert(compute_median_time_past(empty) == 0);
    }

    {
        std::vector<Block> chain;
        chain.push_back(make_block(0));
        chain.push_back(make_block(120));
        chain.push_back(make_block(240));
        // 3 blocks -> middle element of {0,120,240} is 120.
        assert(compute_median_time_past(chain) == 120);
    }

    {
        // 12 blocks -> uses last 11 (indices 1..11).
        std::vector<Block> chain;
        chain.push_back(make_block(0));
        for (int i = 1; i <= 11; ++i)
            chain.push_back(make_block(static_cast<std::uint64_t>(i) * 120));

        const std::vector<std::uint64_t> window = {120, 240, 360,  480,  600, 720,
                                                   840, 960, 1080, 1200, 1320};
        assert(compute_median_time_past(chain) == median_of_timestamps(window));
    }

    std::cout << "[ts] MTP cases: OK\n";

    // -----------------------------------------------------------------
    // 3. minimum and maximum helpers
    // -----------------------------------------------------------------
    {
        std::vector<Block> chain;
        chain.push_back(make_block(0));
        chain.push_back(make_block(100));
        chain.push_back(make_block(200));
        // MTP = 100 -> minimum allowed = 101.
        assert(minimum_allowed_timestamp(chain) == 101);
    }

    assert(maximum_allowed_timestamp(1000) == 1000 + CZR_MAX_FUTURE_DRIFT);
    assert(maximum_allowed_timestamp(0) == CZR_MAX_FUTURE_DRIFT);

    std::cout << "[ts] bounds: OK\n";

    // -----------------------------------------------------------------
    // 4. validate_block_timestamp_canonical
    // -----------------------------------------------------------------
    const std::uint64_t now = 1'000'000;

    std::vector<Block> chain;
    chain.push_back(make_block(0));
    chain.push_back(make_block(120));
    chain.push_back(make_block(240));
    const std::uint64_t mtp = compute_median_time_past(chain);

    // 4a. Block exactly at MTP is rejected (strict inequality).
    {
        Block b = make_block(mtp);
        assert(!validate_block_timestamp_canonical(b, chain, now));
    }

    // 4b. Block one second above MTP and well before now is accepted.
    {
        Block b = make_block(mtp + 1);
        assert(validate_block_timestamp_canonical(b, chain, now));
    }

    // 4c. Block exactly at the future-drift boundary is accepted.
    {
        Block b = make_block(maximum_allowed_timestamp(now));
        assert(validate_block_timestamp_canonical(b, chain, now));
    }

    // 4d. Block one second past the boundary is rejected.
    {
        Block b = make_block(maximum_allowed_timestamp(now) + 1);
        assert(!validate_block_timestamp_canonical(b, chain, now));
    }

    // 4e. Massive future timestamp is rejected.
    {
        Block b = make_block(now + 100 * 365 * 24 * 3600ULL);
        assert(!validate_block_timestamp_canonical(b, chain, now));
    }

    // 4f. Empty chain: only future-drift rule applies.
    {
        std::vector<Block> empty;
        Block b = make_block(now);
        assert(validate_block_timestamp_canonical(b, empty, now));

        Block b2 = make_block(now + 100 * 365 * 24 * 3600ULL);
        assert(!validate_block_timestamp_canonical(b2, empty, now));
    }

    std::cout << "[ts] validation cases: OK\n";

    std::cout << "CaesarTimestampConsensusTest: PASS\n";
    return 0;
}
