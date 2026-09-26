#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/consensus.hpp>

using namespace caesar;

namespace {

Block make_synthetic_block(
    std::uint64_t height,
    std::uint64_t timestamp,
    std::uint32_t difficulty) {

    Block b;
    b.header.version = 1;
    b.header.height = height;
    b.header.timestamp = timestamp;
    b.header.difficulty = difficulty;
    return b;
}

} // namespace

int main() {

    // ---- CASE 1: chain shorter than window -> carry previous difficulty ----
    {
        std::vector<Block> chain;
        chain.push_back(make_synthetic_block(0, 0, 0));
        for (int i = 1; i < 11; ++i)
            chain.push_back(make_synthetic_block(i, i * 10, 12));

        const std::uint32_t d = expected_next_difficulty(chain);
        std::cout << "[case1] short chain d=" << d << "\n";
        assert(d == 12);
    }

    // ---- CASE 2: full window, fast blocks -> increase ----
    {
        std::vector<Block> chain;
        chain.push_back(make_synthetic_block(0, 0, 0));
        for (int i = 1; i <= 12; ++i)
            chain.push_back(make_synthetic_block(i, i * 10, 12));

        const std::uint32_t d = expected_next_difficulty(chain);
        std::cout << "[case2] fast window d=" << d << "\n";
        assert(d > 12);
    }

    // ---- CASE 3: full window, slow blocks -> decrease ----
    {
        std::vector<Block> chain;
        chain.push_back(make_synthetic_block(0, 0, 0));
        for (int i = 1; i <= 12; ++i)
            chain.push_back(make_synthetic_block(i, i * 480, 20));

        const std::uint32_t d = expected_next_difficulty(chain);
        std::cout << "[case3] slow window d=" << d << "\n";
        assert(d < 20);
    }

    // ---- CASE 4: on-target blocks -> stable ----
    {
        std::vector<Block> chain;
        chain.push_back(make_synthetic_block(0, 0, 0));
        for (int i = 1; i <= 12; ++i)
            chain.push_back(make_synthetic_block(i, i * 120, 15));

        const std::uint32_t d = expected_next_difficulty(chain);
        std::cout << "[case4] on-target d=" << d << "\n";
        assert(d == 15);
    }

    // ---- CASE 5: helper matches raw adjust_difficulty_window ----
    {
        std::vector<Block> chain;
        chain.push_back(make_synthetic_block(0, 0, 0));
        for (int i = 1; i <= 12; ++i)
            chain.push_back(make_synthetic_block(i, i * 60, 12));

        std::vector<std::uint64_t> intervals;
        const std::size_t prev_idx = chain.size() - 1;
        const std::size_t first =
            prev_idx + 1 - CZR_DIFFICULTY_WINDOW;

        for (std::size_t i = first; i <= prev_idx; ++i)
            intervals.push_back(
                chain[i].header.timestamp -
                chain[i - 1].header.timestamp);

        const std::uint32_t via_helper =
            expected_next_difficulty(chain);
        const std::uint32_t via_raw =
            adjust_difficulty_window(12, intervals);

        std::cout << "[case5] helper=" << via_helper
                  << " raw=" << via_raw << "\n";
        assert(via_helper == via_raw);
    }

    std::cout << "CaesarDynamicMiningDifficultyTest: PASS\n";
    return 0;
}
