#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/consensus.hpp>

bool check(const char* name, bool condition) {
    std::cout << name << " | "
              << (condition ? "PASS" : "FAIL")
              << '\n';
    return condition;
}

bool validate_expected_difficulty(
    std::uint32_t previous_difficulty,
    const std::vector<std::uint64_t>& intervals,
    std::uint32_t candidate_difficulty) {

    const std::uint32_t expected =
        caesar::adjust_difficulty_window(
            previous_difficulty,
            intervals);

    return candidate_difficulty == expected;
}

int main() {
    bool ok = true;

    const std::vector<std::uint64_t> exact(
        caesar::CZR_DIFFICULTY_WINDOW, 120);

    const std::vector<std::uint64_t> slow(
        caesar::CZR_DIFFICULTY_WINDOW, 240);

    const std::vector<std::uint64_t> fast(
        caesar::CZR_DIFFICULTY_WINDOW, 60);

    ok &= check(
        "Correct difficulty accepted",
        validate_expected_difficulty(10, exact, 10));

    ok &= check(
        "Wrong difficulty rejected",
        !validate_expected_difficulty(10, exact, 11));

    ok &= check(
        "Slower window lowers difficulty",
        validate_expected_difficulty(10, slow, 9));

    ok &= check(
        "Fast window raises difficulty",
        validate_expected_difficulty(10, fast, 11));

    std::vector<std::uint64_t> outlier(
        caesar::CZR_DIFFICULTY_WINDOW, 120);

    outlier[0] = 1;

    ok &= check(
        "Timestamp outlier does not alter median",
        validate_expected_difficulty(10, outlier, 10));

    std::vector<std::uint64_t> majority_slow(
        caesar::CZR_DIFFICULTY_WINDOW, 120);

    for (std::size_t i = 0; i < 6; ++i)
        majority_slow[i] = 240;

    ok &= check(
        "Majority slow blocks lower difficulty",
        validate_expected_difficulty(10, majority_slow, 9));

    ok &= check(
        "Invalid window cannot produce expected change",
        caesar::adjust_difficulty_window(
            10,
            std::vector<std::uint64_t>{120, 120, 120}) == 10);

    std::cout << "Overall: "
              << (ok ? "PASS" : "FAIL")
              << '\n';

    return ok ? 0 : 1;
}
