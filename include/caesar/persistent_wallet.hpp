#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include <openssl/pem.h>

#include <caesar/signature.hpp>
#include <caesar/wallet.hpp>

namespace caesar {

class PersistentWallet {
   public:
    explicit PersistentWallet(const std::filesystem::path& key_path)
        : key_path_(key_path) {
        load_or_create();
    }

    PersistentWallet(const PersistentWallet&) = delete;
    PersistentWallet& operator=(const PersistentWallet&) = delete;

    std::string public_key() const {
        return wallet_->public_key();
    }

    std::string address() const {
        return wallet_->address();
    }

    EVP_PKEY* private_key() const {
        return wallet_->private_key();
    }

    void save() {
        std::error_code ec;
        std::filesystem::create_directories(key_path_.parent_path(), ec);

        FILE* fp = std::fopen(key_path_.string().c_str(), "wb");
        if (!fp)
            throw std::runtime_error("cannot open key file for writing");

        const int ok = PEM_write_PrivateKey(fp, wallet_->private_key(), nullptr,
                                              nullptr, 0, nullptr, nullptr);

        std::fclose(fp);

        if (ok != 1)
            throw std::runtime_error("failed to write PEM key");
    }

   private:
    std::filesystem::path key_path_;
    std::unique_ptr<Wallet> wallet_;

    void load_or_create() {
        if (std::filesystem::exists(key_path_)) {
            FILE* fp = std::fopen(key_path_.string().c_str(), "rb");
            if (fp) {
                EVP_PKEY* key = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
                std::fclose(fp);

                if (key) {
                    try {
                        KeyPair kp(key);
                        wallet_ = std::make_unique<Wallet>(std::move(kp));
                        return;
                    } catch (...) {
                        EVP_PKEY_free(key);
                    }
                }
            }
        }

        wallet_ = std::make_unique<Wallet>();
        save();
    }
};

}  // namespace caesar
