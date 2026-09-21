#include <iostream>
#include <unistd.h>

#include "evidence.hpp"

int main() {
    const auto evidence =
        caesar::doctor::collect_evidence(
            ".",
            static_cast<int>(getpid()));

    std::cout
        << caesar::doctor::to_json(
            evidence);

    return 0;
}
