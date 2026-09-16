#include <cstddef>
#include <cstdint>
#include <vector>

#include <caesar/block.hpp>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size) {

    if (data == nullptr || size == 0)
        return 0;

    try {
        std::vector<std::uint8_t> bytes(data, data + size);

        caesar::Block block =
            caesar::Block::deserialize_full(bytes);

        auto serialized =
            block.serialize_full_binary();

        if (serialized.empty())
            __builtin_trap();

    } catch (...) {
        // Invalid/corrupted blocks are expected fuzz inputs.
    }

    return 0;
}
