#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/block.hpp>

using namespace caesar;

bool check(const char* name, bool condition) {
    std::cout << name << " | "
              << (condition ? "PASS" : "FAIL")
              << '\n';
    return condition;
}

int main() {
    bool ok = true;

    std::vector<std::uint64_t> exact(
        CZR_DIFFICULTY_WINDOW, 120);

    Block valid;
    valid.header.version = 1;
    valid.header.height = 12;
    valid.header.difficulty = 10;

    Block invalid = valid;
    invalid.header.difficulty = 11;

    ok &= check(
        "Block with expected difficulty accepted",
        valid.validate_difficulty_against_history(10, exact));

    ok &= check(
        "Block with wrong difficulty rejected",
        !invalid.validate_difficulty_against_history(10, exact));

    std::vector<std::uint64_t> slow(
        CZR_DIFFICULTY_WINDOW, 240);

    Block slow_block = valid;
    slow_block.header.difficulty = 9;

    ok &= check(
        "Slow history requires lower difficulty",
        slow_block.validate_difficulty_against_history(10, slow));

    slow_block.header.difficulty = 10;

    ok &= check(
        "Old difficulty rejected after slow history",
        !slow_block.validate_difficulty_against_history(10, slow));

    std::cout << "Overall: "
              << (ok ? "PASS" : "FAIL")
              << '\n';

    return ok ? 0 : 1;
}
