#include <cstdlib>
#include <iostream>
#include <string>

#include <caesar/serialization.hpp>

using namespace caesar;

int main() {
    std::cout << "=== Caesar CZR Serialization Tests ===\n";

    try {
        BinaryWriter writer;

        writer.write_u8(0xCA);
        writer.write_u32(0x01020304);
        writer.write_u64(0x0102030405060708ULL);
        writer.write_string("Caesar");

        const auto& data = writer.data();

        if (data.size() != 1 + 4 + 8 + 4 + 6) {
            std::cerr << "[FAIL] Unexpected serialized size\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Deterministic binary serialization size\n";

        if (data[0] != 0xCA ||
            data[1] != 0x04 ||
            data[2] != 0x03 ||
            data[3] != 0x02 ||
            data[4] != 0x01) {

            std::cerr << "[FAIL] Little-endian u32 encoding\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Little-endian u32 encoding\n";

        if (data[5] != 0x08 ||
            data[6] != 0x07 ||
            data[7] != 0x06 ||
            data[8] != 0x05 ||
            data[9] != 0x04 ||
            data[10] != 0x03 ||
            data[11] != 0x02 ||
            data[12] != 0x01) {

            std::cerr << "[FAIL] Little-endian u64 encoding\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Little-endian u64 encoding\n";

        BinaryWriter same;

        same.write_u8(0xCA);
        same.write_u32(0x01020304);
        same.write_u64(0x0102030405060708ULL);
        same.write_string("Caesar");

        if (writer.data() != same.data()) {
            std::cerr << "[FAIL] Serialization is not deterministic\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Serialization is deterministic\n";

        std::cout << "ALL SERIALIZATION TESTS PASSED\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
