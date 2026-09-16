#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size) {

    volatile std::uint8_t value = 0;

    for (std::size_t i = 0; i < size; ++i)
        value ^= data[i];

    return static_cast<int>(value & 0);
}
