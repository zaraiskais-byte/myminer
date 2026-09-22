#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <caesar/consensus.hpp>

using namespace caesar;

static bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    std::cout << "[PASS] " << message << '\n';
    return true;
}

int main() {
    std::cout << "=== Caesar CZR Consensus Tests ===\n";

    std::vector<std::uint8_t> header{
        0x43, 0x5a, 0x52, 0x01,
        0x10, 0x20, 0x30, 0x40
    };

    const Hash256 hash1 = calculate_pow_hash(header, 123);
    const Hash256 hash2 = calculate_pow_hash(header, 123);
    const Hash256 hash3 = calculate_pow_hash(header, 124);

    if (!check(hash1 == hash2, "PoW is deterministic"))
        return EXIT_FAILURE;

    if (!check(hash1 != hash3, "Nonce changes PoW result"))
        return EXIT_FAILURE;

    if (!check(!pow_meets_difficulty(hash1, 257),
               "Difficulty above 256 rejected"))
        return EXIT_FAILURE;

    std::uint64_t nonce = 0;
    Hash256 mined{};

    if (!check(
            mine_pow(header, 4, 0, 100000, nonce, mined),
            "Low-difficulty PoW can be mined"))
        return EXIT_FAILURE;

    if (!check(
            validate_pow(header, nonce, 4),
            "Mined PoW validates"))
        return EXIT_FAILURE;

    if (!check(
            !validate_pow(header, nonce + 1, 4) ||
            calculate_pow_hash(header, nonce + 1) != mined,
            "Different nonce is independently checked"))
        return EXIT_FAILURE;


    // Chain-work / fork-choice foundation.
    {
        const ChainWork zero = block_work(0);
        const ChainWork one = block_work(1);
        const ChainWork twelve = block_work(12);
        const ChainWork twenty = block_work(20);

        if (!check(
                zero.limbs[0] == 1,
                "Difficulty 0 represents one unit of work"))
            return EXIT_FAILURE;

        if (!check(
                one.limbs[0] == 2,
                "Difficulty 1 represents two units of work"))
            return EXIT_FAILURE;

        if (!check(
                twelve.limbs[0] == 4096,
                "Difficulty 12 represents 2^12 work"))
            return EXIT_FAILURE;

        if (!check(
                twenty.limbs[0] == 1048576,
                "Difficulty 20 represents 2^20 work"))
            return EXIT_FAILURE;

        if (!check(
                twenty > twelve,
                "Higher difficulty has greater block work"))
            return EXIT_FAILURE;

        const ChainWork difficulty256 = block_work(256);

        if (!check(
                difficulty256.limbs[4] == 1 &&
                difficulty256.limbs[0] == 0 &&
                difficulty256.limbs[1] == 0 &&
                difficulty256.limbs[2] == 0 &&
                difficulty256.limbs[3] == 0 &&
                difficulty256.limbs[5] == 0,
                "Difficulty 256 is represented without overflow"))
            return EXIT_FAILURE;

        std::vector<std::uint32_t> difficulties_a{12, 12};
        std::vector<std::uint32_t> difficulties_b{13};

        // Use the same work arithmetic through explicit accumulation.
        ChainWork work_a;
        for (const auto difficulty : difficulties_a)
            work_a.add_power_of_two(difficulty);

        ChainWork work_b;
        for (const auto difficulty : difficulties_b)
            work_b.add_power_of_two(difficulty);

        if (!check(
                work_a == work_b,
                "Cumulative work compares actual PoW work, not summed difficulty"))
            return EXIT_FAILURE;
    }


    // Fork-choice foundation: cumulative work, not height.
    {
        auto make_work = [](std::initializer_list<std::uint32_t> difficulties) {
            ChainWork work;
            for (const auto difficulty : difficulties)
                work.add_power_of_two(difficulty);
            return work;
        };

        const ChainWork longer_lower_work =
            make_work({12, 12});

        const ChainWork shorter_higher_work =
            make_work({13, 13});

        if (!check(
                shorter_higher_work > longer_lower_work,
                "Higher cumulative PoW work beats a lower-work chain"))
            return EXIT_FAILURE;

        const ChainWork equal_a =
            make_work({12, 12});

        const ChainWork equal_b =
            make_work({13});

        if (!check(
                equal_a == equal_b,
                "Equal cumulative work produces a fork-choice tie"))
            return EXIT_FAILURE;

        if (!check(
                !(equal_a > equal_b) && !(equal_b > equal_a),
                "Equal-work chains have no ordering advantage"))
            return EXIT_FAILURE;

        const ChainWork longer_chain =
            make_work({12, 12, 12});

        if (!check(
                longer_chain > equal_b,
                "Additional valid PoW work increases cumulative chain work"))
            return EXIT_FAILURE;
    }

    std::cout << "ALL CONSENSUS TESTS PASSED\n";
    return EXIT_SUCCESS;
}
