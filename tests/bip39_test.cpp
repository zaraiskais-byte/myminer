#include <caesar/bip39.hpp>

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>(
            std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

std::string bytes_to_hex(const std::vector<uint8_t>& bytes) {
    static const char* h = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (auto b : bytes) {
        out += h[(b >> 4) & 0xf];
        out += h[b & 0xf];
    }
    return out;
}

void test_entropy_to_mnemonic() {
    struct V { const char* e; const char* m; };
    const V vecs[] = {
        {"00000000000000000000000000000000",
         "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"},
        {"7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f",
         "legal winner thank year wave sausage worth useful legal winner thank yellow"},
        {"80808080808080808080808080808080",
         "letter advice cage absurd amount doctor acoustic avoid letter advice cage above"},
        {"ffffffffffffffffffffffffffffffff",
         "zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo wrong"},
    };
    for (const auto& v : vecs) {
        auto e = hex_to_bytes(v.e);
        std::string got = caesar::bip39::entropy_to_mnemonic(e);
        if (got != v.m) {
            std::cerr << "FAIL entropy=" << v.e << "\n";
            std::cerr << "  got: " << got << "\n";
            std::cerr << "  exp: " << v.m << "\n";
            std::exit(1);
        }
    }
    std::cout << "PASS: entropy_to_mnemonic (4 official vectors)\n";
}

void test_mnemonic_to_seed() {
    struct V { const char* m; const char* p; const char* s; };
    const V vecs[] = {
        {"abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about",
         "TREZOR",
         "c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04"},
        {"abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about",
         "",
         "5eb00bbddcf069084889a8ab9155568165f5c453ccb85e70811aaed6f6da5fc19a5ac40b389cd370d086206dec8aa6c43daea6690f20ad3d8d48b2d2ce9e38e4"},
    };
    for (const auto& v : vecs) {
        auto seed = caesar::bip39::mnemonic_to_seed(v.m, v.p);
        std::string hex = bytes_to_hex(std::vector<uint8_t>(seed.begin(), seed.end()));
        if (hex != v.s) {
            std::cerr << "FAIL seed passphrase='" << v.p << "'\n";
            std::cerr << "  got: " << hex << "\n";
            std::cerr << "  exp: " << v.s << "\n";
            std::exit(1);
        }
    }
    std::cout << "PASS: mnemonic_to_seed (2 official vectors)\n";
}

void test_validate_mnemonic() {
    assert(caesar::bip39::validate_mnemonic(
        "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about") == true);

    assert(caesar::bip39::validate_mnemonic(
        "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon") == false);

    assert(caesar::bip39::validate_mnemonic(
        "notaword notaword notaword notaword notaword notaword notaword notaword notaword notaword notaword notaword") == false);

    assert(caesar::bip39::validate_mnemonic("") == false);
    assert(caesar::bip39::validate_mnemonic("abandon") == false);

    std::cout << "PASS: validate_mnemonic\n";
}

void test_roundtrip() {
    for (int t = 0; t < 10; ++t) {
        std::vector<uint8_t> entropy(16);
        for (auto& b : entropy) b = static_cast<uint8_t>(std::rand() & 0xff);
        std::string m = caesar::bip39::entropy_to_mnemonic(entropy);
        assert(caesar::bip39::validate_mnemonic(m) == true);
    }
    std::cout << "PASS: roundtrip (10 random)\n";
}

}  // namespace

int main() {
    std::cout << "=== BIP39 Test Suite ===\n\n";
    test_entropy_to_mnemonic();
    test_mnemonic_to_seed();
    test_validate_mnemonic();
    test_roundtrip();
    std::cout << "\n=== ALL BIP39 TESTS PASSED ===\n";
    return 0;
}
