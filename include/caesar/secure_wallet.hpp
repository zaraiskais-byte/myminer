#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace caesar {

// AES-256-GCM encrypted wallet storage
// Key derived from user PIN via PBKDF2 (100k iterations)
class SecureWalletStorage {
   public:
    static constexpr std::size_t SALT_LEN = 16;
    static constexpr std::size_t IV_LEN = 12;
    static constexpr std::size_t TAG_LEN = 16;
    static constexpr int PBKDF2_ITER = 100000;

    static std::vector<std::uint8_t> derive_key(const std::string& pin,
                                                  const std::vector<std::uint8_t>& salt) {
        std::vector<std::uint8_t> key(32);
        if (PKCS5_PBKDF2_HMAC(pin.data(), static_cast<int>(pin.size()),
                              salt.data(), static_cast<int>(salt.size()),
                              PBKDF2_ITER, EVP_sha256(),
                              32, key.data()) != 1) {
            throw std::runtime_error("PBKDF2 failed");
        }
        return key;
    }

    static void encrypt(const std::filesystem::path& path,
                        const std::string& pin,
                        const std::vector<std::uint8_t>& plaintext) {
        std::vector<std::uint8_t> salt(SALT_LEN);
        std::vector<std::uint8_t> iv(IV_LEN);
        if (RAND_bytes(salt.data(), SALT_LEN) != 1) throw std::runtime_error("RAND salt");
        if (RAND_bytes(iv.data(), IV_LEN) != 1) throw std::runtime_error("RAND iv");

        const auto key = derive_key(pin, salt);

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) throw std::runtime_error("ctx");

        std::vector<std::uint8_t> ciphertext(plaintext.size() + 16);
        std::vector<std::uint8_t> tag(TAG_LEN);
        int len = 0, total = 0;

        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, nullptr) != 1 ||
            EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1 ||
            EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(),
                              static_cast<int>(plaintext.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("encrypt init");
        }
        total = len;
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1 ||
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("encrypt final");
        }
        total += len;
        EVP_CIPHER_CTX_free(ctx);

        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(salt.data()), SALT_LEN);
        out.write(reinterpret_cast<const char*>(iv.data()), IV_LEN);
        out.write(reinterpret_cast<const char*>(tag.data()), TAG_LEN);
        out.write(reinterpret_cast<const char*>(ciphertext.data()), total);
    }

    static std::vector<std::uint8_t> decrypt(const std::filesystem::path& path,
                                              const std::string& pin) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("cannot open wallet file");

        std::vector<std::uint8_t> salt(SALT_LEN), iv(IV_LEN), tag(TAG_LEN);
        in.read(reinterpret_cast<char*>(salt.data()), SALT_LEN);
        in.read(reinterpret_cast<char*>(iv.data()), IV_LEN);
        in.read(reinterpret_cast<char*>(tag.data()), TAG_LEN);

        std::vector<std::uint8_t> ciphertext((std::istreambuf_iterator<char>(in)),
                                               std::istreambuf_iterator<char>());
        if (ciphertext.empty()) throw std::runtime_error("empty wallet file");

        const auto key = derive_key(pin, salt);

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) throw std::runtime_error("ctx");

        std::vector<std::uint8_t> plaintext(ciphertext.size());
        int len = 0, total = 0;

        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, nullptr) != 1 ||
            EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1 ||
            EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(),
                              static_cast<int>(ciphertext.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("decrypt init");
        }
        total = len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag.data()) != 1 ||
            EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("wrong PIN or tampered file");
        }
        total += len;
        EVP_CIPHER_CTX_free(ctx);

        plaintext.resize(total);
        return plaintext;
    }
};

}  // namespace caesar
