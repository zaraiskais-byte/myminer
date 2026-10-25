// Copyright (c) 2026 zaraiskais-byte. All rights reserved.
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <iterator>

#include <caesar/persistent_wallet.hpp>
#include <caesar/secure_wallet.hpp>
#include <caesar/wallet_auth.hpp>

namespace {

std::filesystem::path make_test_dir() {
    const auto base =
        std::filesystem::temp_directory_path() /
        "caesar-persistent-wallet-test";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base, ec);

    if (ec) {
        throw std::runtime_error("cannot create test directory");
    }

    return base;
}

void test_create_and_reload() {
    const auto dir = make_test_dir();
    const auto key_path = dir / "wallet.pem";

    std::string original_public_key;
    std::string original_address;

    {
        caesar::PersistentWallet wallet(key_path);

        original_public_key = wallet.public_key();
        original_address = wallet.address();

        assert(!original_public_key.empty());
        assert(!original_address.empty());
        assert(std::filesystem::is_regular_file(key_path));
    }

    {
        caesar::PersistentWallet wallet(key_path);

        assert(wallet.public_key() == original_public_key);
        assert(wallet.address() == original_address);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    assert(!ec);
}

void test_explicit_save_is_atomic_and_persistent() {
    const auto dir = make_test_dir();
    const auto key_path = dir / "wallet.pem";
    const auto temp_path = dir / "wallet.pem.tmp";

    std::string original_public_key;
    std::string original_address;

    {
        caesar::PersistentWallet wallet(key_path);

        original_public_key = wallet.public_key();
        original_address = wallet.address();

        assert(std::filesystem::is_regular_file(key_path));

        // Simulate a stale temporary file from an interrupted previous save.
        {
            std::ofstream stale(temp_path, std::ios::binary | std::ios::trunc);
            assert(stale);
            stale << "stale temporary data\n";
        }

        assert(std::filesystem::is_regular_file(temp_path));

        // This exercises the actual replacement path:
        // temp write -> fsync -> atomic rename -> directory fsync.
        wallet.save();

        assert(std::filesystem::is_regular_file(key_path));
        assert(!std::filesystem::exists(temp_path));
        assert(wallet.public_key() == original_public_key);
        assert(wallet.address() == original_address);
    }

    {
        caesar::PersistentWallet wallet(key_path);

        assert(wallet.public_key() == original_public_key);
        assert(wallet.address() == original_address);
    }

#if defined(__linux__) || defined(__ANDROID__)
    {
        std::error_code ec;
        const auto perms = std::filesystem::status(key_path, ec).permissions();

        assert(!ec);

        const auto expected =
            std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write;

        assert((perms & std::filesystem::perms::group_read) ==
               std::filesystem::perms::none);
        assert((perms & std::filesystem::perms::group_write) ==
               std::filesystem::perms::none);
        assert((perms & std::filesystem::perms::group_exec) ==
               std::filesystem::perms::none);
        assert((perms & std::filesystem::perms::others_read) ==
               std::filesystem::perms::none);
        assert((perms & std::filesystem::perms::others_write) ==
               std::filesystem::perms::none);
        assert((perms & std::filesystem::perms::others_exec) ==
               std::filesystem::perms::none);

        assert((perms & expected) == expected);
    }
#endif

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    assert(!ec);
}

void test_corrupt_key_is_rejected() {
    const auto dir = make_test_dir();
    const auto key_path = dir / "wallet.pem";

    {
        std::ofstream out(key_path, std::ios::binary | std::ios::trunc);
        assert(out);
        out << "not a valid private key\n";
    }

    bool rejected = false;

    try {
        caesar::PersistentWallet wallet(key_path);
    } catch (const std::exception&) {
        rejected = true;
    }

    assert(rejected);
    assert(std::filesystem::is_regular_file(key_path));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    assert(!ec);
}

void test_existing_directory_is_supported() {
    const auto dir = make_test_dir();
    const auto key_path = dir / "wallet.pem";

    caesar::PersistentWallet wallet(key_path);
    assert(wallet.public_key().size() == 64);
    assert(std::filesystem::is_regular_file(key_path));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    assert(!ec);
}


void test_encrypted_wallet_unlock_roundtrip() {
    const auto dir = make_test_dir();
    const auto plain_path = dir / "plain.pem";
    const auto encrypted_path = dir / "encrypted.wallet";
    const std::string pin = "test-pin-7391";
    std::string expected_address;
    std::string expected_public_key;

    {
        caesar::PersistentWallet original(plain_path);
        expected_address = original.address();
        expected_public_key = original.public_key();
    }

    std::ifstream input(plain_path, std::ios::binary);
    assert(input);
    std::vector<std::uint8_t> pem(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    assert(!pem.empty());

    caesar::SecureWalletStorage::encrypt(encrypted_path, pin, pem);

    {
        caesar::PersistentWallet restored(encrypted_path);
        assert(!restored.is_loaded());
        restored.unlock_with_pin(pin);
        assert(restored.is_loaded());
        assert(restored.address() == expected_address);
        assert(restored.public_key() == expected_public_key);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    assert(!ec);
}

void test_truncated_encrypted_wallet_is_rejected() {
    const auto dir = make_test_dir();
    const auto path = dir / "truncated.wallet";

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        assert(out);
        out << "short";
    }

    bool rejected = false;
    try {
        (void)caesar::SecureWalletStorage::decrypt(path, "test-pin");
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);
    assert(std::filesystem::is_regular_file(path));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    assert(!ec);
}

void test_pin_hash_rejects_truncated_file() {
    const auto dir = make_test_dir();
    const auto path = dir / "pin.hash";
    const std::string pin = "test-pin-7391";

    caesar::save_pin(path, pin);
    assert(caesar::verify_pin(path, pin));
    assert(!caesar::verify_pin(path, "wrong-pin"));

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        assert(out);
        out << "truncated";
    }

    assert(!caesar::verify_pin(path, pin));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    assert(!ec);
}

}  // namespace

int main() {
    test_create_and_reload();
    test_explicit_save_is_atomic_and_persistent();
    test_corrupt_key_is_rejected();
    test_existing_directory_is_supported();
    test_encrypted_wallet_unlock_roundtrip();
    test_truncated_encrypted_wallet_is_rejected();
    test_pin_hash_rejects_truncated_file();

    return 0;
}
