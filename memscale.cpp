#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
#include <caesar/crypto.hpp>
#include <caesar/serialization.hpp>

using namespace caesar;

static Hash256 memory_pow(const std::vector<std::uint8_t>& header,
                           std::uint64_t nonce,
                           std::size_t bytes) {
    std::vector<std::uint8_t> memory(bytes);
    Hash256 state = sha256(bytes_to_binary_string(header));

    for (std::size_t i = 0; i < bytes; ++i)
        memory[i] = state[i % state.size()] ^
                    static_cast<std::uint8_t>(nonce + i);

    for (std::size_t i = 0; i < bytes; i += 32) {
        std::vector<std::uint8_t> block(32);
        for (std::size_t j = 0; j < 32; ++j)
            block[j] = memory[(i + j) % bytes];

        state = sha256(bytes_to_binary_string(block));
        memory[i % bytes] ^= state[i % state.size()];
    }

    return state;
}

int main() {
    const std::vector<std::uint8_t> header{
        0x43,0x5a,0x52,0x01,0x10,0x20,0x30,0x40
    };

    const std::size_t sizes[] = {
        128 * 1024,
        512 * 1024,
        2 * 1024 * 1024
    };

    for (std::size_t bytes : sizes) {
        std::uint64_t attempts = 0;
        Hash256 last{};

        auto start = std::chrono::steady_clock::now();
        auto end = start + std::chrono::seconds(5);

        while (std::chrono::steady_clock::now() < end)
            last = memory_pow(header, attempts++, bytes);

        double seconds =
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();

        double rate = static_cast<double>(attempts) / seconds;

        std::cout << "=== MEMORY "
                  << (bytes / 1024)
                  << " KB ===\n";

        std::cout << "Attempts: " << attempts << "\n";
        std::cout << "Hashrate: "
                  << std::fixed << std::setprecision(2)
                  << rate << " H/s\n\n";
    }

    return 0;
}
