#pragma once

#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#if defined(__linux__) || defined(__ANDROID__)
#include <sys/stat.h>
#endif

#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include <caesar/blockchain_storage.hpp>
#include <caesar/signature.hpp>
#include <caesar/wallet.hpp>
#include <caesar/secure_wallet.hpp>
#include <caesar/wallet_hd.hpp>
namespace caesar {

class PersistentWallet {
   public:
    explicit PersistentWallet(const std::filesystem::path& key_path)
        : key_path_(key_path) {
        load_or_defer();
    }

    PersistentWallet(const PersistentWallet&) = delete;
    PersistentWallet& operator=(const PersistentWallet&) = delete;

    std::string public_key() const {
        if (!wallet_) throw std::runtime_error("wallet not loaded");
        return wallet_->public_key();
    }

    std::string address() const {
        if (!wallet_) throw std::runtime_error("wallet not loaded");
        return wallet_->address();
    }

    EVP_PKEY* private_key() const {
        if (!wallet_) throw std::runtime_error("wallet not loaded");
        return wallet_->private_key();
    }

    EVP_PKEY* public_key_handle() const {
        if (!wallet_) throw std::runtime_error("wallet not loaded");
        return wallet_->public_key_handle();
    }

    void save() {
        if (!wallet_ || !wallet_->valid()) {
            throw std::runtime_error("cannot save invalid wallet");
        }

        const auto parent = key_path_.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                throw std::runtime_error("cannot create wallet directory");
            }
        }

        /*
         * Never truncate the canonical wallet file in place.
         *
         * Write the complete PEM to a sibling temporary file, force the
         * bytes to durable storage, atomically replace the canonical file,
         * then force the parent directory entry to durable storage.
         */
        const std::filesystem::path temp_path =
            key_path_.string() + ".tmp";

        std::error_code cleanup_ec;
        std::filesystem::remove(temp_path, cleanup_ec);

        FILE* fp = std::fopen(temp_path.string().c_str(), "wb");
        if (!fp) {
            throw std::runtime_error(
                "cannot open temporary wallet key for writing");
        }

        const int ok = PEM_write_PrivateKey(
            fp, wallet_->private_key(), nullptr, nullptr, 0, nullptr, nullptr);

        const int flush_result = std::fflush(fp);
        const int close_result = std::fclose(fp);

        if (ok != 1 || flush_result != 0 || close_result != 0) {
            std::filesystem::remove(temp_path, cleanup_ec);
            throw std::runtime_error("failed to write temporary PEM key");
        }

#if defined(__linux__) || defined(__ANDROID__)
        /*
         * Wallet keys are private material. Restrict the temporary file
         * before it becomes the canonical key file.
         */
        if (::chmod(temp_path.c_str(), 0600) != 0) {
            std::filesystem::remove(temp_path, cleanup_ec);
            throw std::runtime_error("failed to secure temporary wallet key");
        }
#endif

        try {
            detail::fsync_file(temp_path);

            std::error_code rename_ec;
            std::filesystem::rename(temp_path, key_path_, rename_ec);
            if (rename_ec) {
                throw std::runtime_error(
                    "failed to atomically replace wallet key: " +
                    rename_ec.message());
            }

            detail::fsync_parent_dir(key_path_);
        } catch (...) {
            std::filesystem::remove(temp_path, cleanup_ec);
            throw;
        }

    }

    // ==========================================================
    // Encrypted storage (Phase 3)
    // ==========================================================
    void save_encrypted(const std::string& pin) {
        if (!wallet_ || !wallet_->valid()) {
            throw std::runtime_error("cannot save invalid wallet");
        }

        const auto parent = key_path_.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                throw std::runtime_error("cannot create wallet directory");
            }
        }

        // Serialize current PEM into memory, then hand it to SecureWalletStorage.
        // Use a temporary in-memory buffer via BIO.
        BIO* bio = BIO_new(BIO_s_mem());
        if (!bio) {
            throw std::runtime_error("failed to allocate BIO");
        }

        const int ok = PEM_write_bio_PrivateKey(
            bio, wallet_->private_key(), nullptr, nullptr, 0, nullptr, nullptr);

        if (ok != 1) {
            BIO_free(bio);
            throw std::runtime_error("failed to serialize private key");
        }

        BUF_MEM* mem = nullptr;
        BIO_get_mem_ptr(bio, &mem);
        if (!mem || !mem->data || mem->length == 0) {
            BIO_free(bio);
            throw std::runtime_error("empty PEM buffer");
        }

        std::vector<std::uint8_t> plaintext(
            reinterpret_cast<std::uint8_t*>(mem->data),
            reinterpret_cast<std::uint8_t*>(mem->data) + mem->length);

        BIO_free(bio);

        // Encrypt straight to a sibling temp file, then atomically rename.
        const std::filesystem::path temp_path =
            key_path_.string() + ".tmp";

        std::error_code cleanup_ec;
        std::filesystem::remove(temp_path, cleanup_ec);

        SecureWalletStorage::encrypt(temp_path, pin, plaintext);

#if defined(__linux__) || defined(__ANDROID__)
        if (::chmod(temp_path.c_str(), 0600) != 0) {
            std::filesystem::remove(temp_path, cleanup_ec);
            throw std::runtime_error("failed to secure encrypted wallet file");
        }
#endif

        try {
            detail::fsync_file(temp_path);

            std::error_code rename_ec;
            std::filesystem::rename(temp_path, key_path_, rename_ec);
            if (rename_ec) {
                throw std::runtime_error(
                    "failed to atomically replace encrypted wallet: " +
                    rename_ec.message());
            }

            detail::fsync_parent_dir(key_path_);
        } catch (...) {
            std::filesystem::remove(temp_path, cleanup_ec);
            throw;
        }
    }

    void replace_with_mnemonic(const std::string& mnemonic) {
        if (!bip39::validate_mnemonic(mnemonic)) {
            throw std::runtime_error("invalid BIP39 mnemonic");
        }

        Wallet fresh = wallet_from_mnemonic(mnemonic);
        wallet_ = std::make_unique<Wallet>(std::move(fresh));
        save();
    }

    std::string create_new_hd_wallet() {
        std::vector<std::uint8_t> entropy(16);
        if (RAND_bytes(entropy.data(), static_cast<int>(entropy.size())) != 1) {
            throw std::runtime_error("RAND_bytes failed");
        }

        std::string mnemonic = bip39::entropy_to_mnemonic(entropy);
        wallet_ = std::make_unique<Wallet>(wallet_from_mnemonic(mnemonic));
        save();
        return mnemonic;
    }

    // ==========================================================
    // Deferred-load helpers
    // ==========================================================
    bool is_loaded() const { return wallet_ != nullptr; }

    bool is_file_encrypted() const {
        std::error_code ec;
        if (!std::filesystem::exists(key_path_, ec) || ec) return false;

        const auto sz = std::filesystem::file_size(key_path_, ec);
        if (ec) return false;

        // Our encrypted envelope is at least salt(16)+iv(12)+tag(16)+1 = 45 bytes.
        constexpr std::size_t MIN_ENCRYPTED_SIZE = 45;
        if (sz < MIN_ENCRYPTED_SIZE) return false;

        // A valid PEM starts with '-'. Encrypted data does not.
        FILE* fp = std::fopen(key_path_.string().c_str(), "rb");
        if (!fp) return false;
        unsigned char first = 0;
        std::fread(&first, 1, 1, fp);
        std::fclose(fp);
        return first != '-';
    }

    void unlock_with_pin(const std::string& pin) {
        auto plaintext = SecureWalletStorage::decrypt(key_path_, pin);
        BIO* bio = BIO_new_mem_buf(plaintext.data(),
                                    static_cast<int>(plaintext.size()));
        if (!bio) throw std::runtime_error("unlock_with_pin: BIO alloc");
        EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (!key) throw std::runtime_error("unlock_with_pin: PEM parse");
        if (EVP_PKEY_base_id(key) != EVP_PKEY_ED25519) {
            EVP_PKEY_free(key);
            throw std::runtime_error("unlock_with_pin: not Ed25519");
        }
        KeyPair kp(key);
        auto loaded = std::make_unique<Wallet>(std::move(kp));
        if (!loaded->valid()) {
            throw std::runtime_error("unlock_with_pin: invalid");
        }
        wallet_ = std::move(loaded);
    }

   private:
    std::filesystem::path key_path_;
    std::unique_ptr<Wallet> wallet_;

    void load_or_defer() {
        std::error_code ec;
        const bool exists = std::filesystem::exists(key_path_, ec);
        if (ec) throw std::runtime_error("cannot inspect wallet key path");

        if (!exists) {
            wallet_ = std::make_unique<Wallet>();
            save();
            return;
        }

        if (!std::filesystem::is_regular_file(key_path_, ec) || ec) {
            throw std::runtime_error(
                "existing wallet path is not a regular file");
        }

        // Attempt to parse as plaintext PEM. This is the authoritative
        // test for "is this a valid key file?" -- not the first byte.
        if (try_load_plaintext_pem()) {
            return;
        }

        // Not a valid PEM. Distinguish encrypted from corrupted:
        // our encrypted envelope is salt(16) + iv(12) + tag(16) + >=1 byte,
        // i.e. at least 45 bytes on disk. Anything smaller cannot be
        // a valid encrypted wallet and must be treated as corruption.
        std::error_code size_ec;
        const auto fsize = std::filesystem::file_size(key_path_, size_ec);
        if (size_ec) {
            throw std::runtime_error(
                "cannot stat existing wallet key file");
        }

        constexpr std::size_t MIN_ENCRYPTED_SIZE = 45;
        if (fsize < MIN_ENCRYPTED_SIZE) {
            throw std::runtime_error(
                "existing wallet key is invalid; refusing replacement");
        }

        // Large enough to be encrypted. Defer loading until unlock.
        wallet_ = nullptr;
    }

    bool try_load_plaintext_pem() {
        FILE* fp = std::fopen(key_path_.string().c_str(), "rb");
        if (!fp) {
            throw std::runtime_error(
                "existing wallet key cannot be opened; refusing replacement");
        }

        EVP_PKEY* key = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
        const int close_result = std::fclose(fp);

        if (!key) {
            return false;
        }
        if (close_result != 0) {
            EVP_PKEY_free(key);
            throw std::runtime_error(
                "failed to close existing wallet key file");
        }
        if (EVP_PKEY_base_id(key) != EVP_PKEY_ED25519) {
            EVP_PKEY_free(key);
            throw std::runtime_error(
                "existing wallet key is not Ed25519; refusing replacement");
        }

        KeyPair kp(key);
        auto loaded = std::make_unique<Wallet>(std::move(kp));
        if (!loaded->valid()) {
            throw std::runtime_error(
                "existing wallet key failed validation");
        }
        wallet_ = std::move(loaded);
        return true;
    }


};

}  // namespace caesar
