#include <cstddef>
#include <cstdint>
#include <vector>

#include <caesar/transaction.hpp>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size) {

    if (data == nullptr || size == 0)
        return 0;

    try {
        std::vector<std::uint8_t> bytes(data, data + size);

        caesar::Transaction tx =
            caesar::Transaction::deserialize_full(bytes);

        auto serialized =
            tx.serialize_full_binary();

        if (serialized.empty())
            __builtin_trap();

    } catch (...) {
        // Malformed input is expected and must not crash the fuzzer.
    }

    return 0;
}
