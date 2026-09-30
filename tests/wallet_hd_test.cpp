#include <caesar/wallet_hd.hpp>
#include <caesar/signature.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

void test_same_mnemonic_same_wallet() {
    const std::string m =
        "abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon about";

    caesar::Wallet a = caesar::wallet_from_mnemonic(m);
    caesar::Wallet b = caesar::wallet_from_mnemonic(m);
    caesar::Wallet c = caesar::wallet_from_mnemonic(m);

    assert(a.public_key() == b.public_key());
    assert(b.public_key() == c.public_key());
    assert(a.address()    == b.address());
    assert(b.address()    == c.address());

    std::cout << "PASS: same mnemonic -> same wallet (3x)\n";
}

void test_different_mnemonic_different_wallet() {
    const std::string m1 =
        "abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon about";

    const std::string m2 =
        "legal winner thank year wave sausage worth useful "
        "legal winner thank yellow";

    caesar::Wallet a = caesar::wallet_from_mnemonic(m1);
    caesar::Wallet b = caesar::wallet_from_mnemonic(m2);

    assert(a.public_key() != b.public_key());
    assert(a.address()    != b.address());

    std::cout << "PASS: different mnemonics -> different wallets\n";
}

void test_passphrase_changes_wallet() {
    const std::string m =
        "abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon about";

    caesar::Wallet a = caesar::wallet_from_mnemonic(m, "");
    caesar::Wallet b = caesar::wallet_from_mnemonic(m, "TREZOR");

    assert(a.public_key() != b.public_key());
    assert(a.address()    != b.address());

    std::cout << "PASS: passphrase changes identity\n";
}

void test_wallet_is_valid() {
    const std::string m =
        "abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon about";

    caesar::Wallet w = caesar::wallet_from_mnemonic(m);
    assert(w.valid());

    // Public key must be 64 hex chars (32 bytes raw).
    assert(w.public_key().size() == 64);

    // Address must look like a CZ1... address.
    const std::string addr = w.address();
    assert(addr.size() > 4);
    assert(addr.substr(0, 3) == "CZ1");

    std::cout << "PASS: wallet valid, address = "
              << addr.substr(0, 20) << "...\n";
}

void test_deterministic_signing() {
    const std::string m =
        "abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon about";

    caesar::Wallet a = caesar::wallet_from_mnemonic(m);
    caesar::Wallet b = caesar::wallet_from_mnemonic(m);

    const std::string msg = "Caesar CZR deterministic test";

    auto sig_a = caesar::sign_message(a.private_key(), msg);

    // Verify using b's public key — proves a and b share identity.
    assert(caesar::verify_signature(b.public_key_handle(), msg, sig_a));

    // Also verify with a's own public key.
    assert(caesar::verify_signature(a.public_key_handle(), msg, sig_a));

    // Corrupt the message — must fail.
    assert(!caesar::verify_signature(b.public_key_handle(), msg + "x", sig_a));

    std::cout << "PASS: deterministic signing (cross-wallet verify)\n";
}

void test_invalid_mnemonic_rejected() {
    bool threw = false;
    try {
        caesar::wallet_from_mnemonic("not a valid mnemonic");
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        caesar::wallet_from_mnemonic("");
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);

    std::cout << "PASS: invalid mnemonic rejected\n";
}

void test_roundtrip_entropy_mnemonic_wallet() {
    // Simulate: user creates wallet from 128-bit entropy,
    // then recovers it later from the 12 words.
    const std::vector<uint8_t> entropy = {
        0x42, 0x13, 0x37, 0xAA, 0xCD, 0xEF, 0x00, 0x11,
        0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
    };

    const std::string mnemonic = caesar::bip39::entropy_to_mnemonic(entropy);
    assert(caesar::bip39::validate_mnemonic(mnemonic));

    caesar::Wallet original = caesar::wallet_from_mnemonic(mnemonic);
    caesar::Wallet recovered = caesar::wallet_from_mnemonic(mnemonic);

    assert(original.public_key() == recovered.public_key());
    assert(original.address()    == recovered.address());

    std::cout << "PASS: entropy -> mnemonic -> wallet -> recover\n";
}

}  // namespace

int main() {
    std::cout << "=== Wallet HD (BIP39 + SLIP-0010) Test Suite ===\n\n";
    test_same_mnemonic_same_wallet();
    test_different_mnemonic_different_wallet();
    test_passphrase_changes_wallet();
    test_wallet_is_valid();
    test_deterministic_signing();
    test_invalid_mnemonic_rejected();
    test_roundtrip_entropy_mnemonic_wallet();
    std::cout << "\n=== ALL WALLET HD TESTS PASSED ===\n";
    return 0;
}
