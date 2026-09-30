#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <caesar/persistent_wallet.hpp>

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

}  // namespace

int main() {
    test_create_and_reload();
    test_explicit_save_is_atomic_and_persistent();
    test_corrupt_key_is_rejected();
    test_existing_directory_is_supported();

    return 0;
}
