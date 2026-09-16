#include <cstdlib>
#include <iostream>
#include <string>

#include <caesar/address.hpp>

using namespace caesar;

int main() {
    std::cout << "=== Caesar CZR Address Tests ===\n";

    const std::string public_key =
        "caesar-ed25519-public-key-test";

    const std::string address =
        address_from_public_key(public_key);

    if (address.rfind("CZ1", 0) != 0) {
        std::cerr << "[FAIL] Address prefix\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] CZR address prefix\n";

    if (address.size() != 75) {
        std::cerr << "[FAIL] Address length\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] CZR address length\n";

    if (!is_valid_address(address)) {
        std::cerr << "[FAIL] Valid address rejected\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Valid address accepted\n";

    std::string modified = address;
    modified[20] =
        modified[20] == '0' ? '1' : '0';

    if (is_valid_address(modified)) {
        std::cerr << "[FAIL] Checksum failed to detect modification\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Modified address rejected by checksum\n";

    std::string bad_checksum = address;
    bad_checksum.back() =
        bad_checksum.back() == '0' ? '1' : '0';

    if (is_valid_address(bad_checksum)) {
        std::cerr << "[FAIL] Invalid checksum accepted\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Invalid checksum rejected\n";

    std::string bad_prefix = address;
    bad_prefix[0] = 'X';

    if (is_valid_address(bad_prefix)) {
        std::cerr << "[FAIL] Invalid prefix accepted\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Invalid prefix rejected\n";

    std::cout << "Address: " << address << '\n';
    std::cout << "ALL ADDRESS CHECKSUM TESTS PASSED\n";

    return EXIT_SUCCESS;
}
