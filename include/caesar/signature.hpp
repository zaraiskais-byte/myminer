#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/evp.h>

namespace caesar {

class KeyPair {
public:
    EVP_PKEY* private_key{nullptr};
    EVP_PKEY* public_key{nullptr};

    KeyPair() = default;

    explicit KeyPair(EVP_PKEY* key)
        : private_key(key), public_key(nullptr) {

        if (!key)
            throw std::runtime_error("null key");

        if (EVP_PKEY_up_ref(key) != 1) {
            EVP_PKEY_free(key);
            private_key = nullptr;
            throw std::runtime_error("failed to reference public key");
        }

        public_key = key;
    }

    ~KeyPair() {
        if (private_key)
            EVP_PKEY_free(private_key);

        if (public_key)
            EVP_PKEY_free(public_key);
    }

    KeyPair(const KeyPair&) = delete;
    KeyPair& operator=(const KeyPair&) = delete;

    KeyPair(KeyPair&& other) noexcept
        : private_key(other.private_key),
          public_key(other.public_key) {

        other.private_key = nullptr;
        other.public_key = nullptr;
    }

    KeyPair& operator=(KeyPair&& other) noexcept {
        if (this != &other) {
            if (private_key)
                EVP_PKEY_free(private_key);

            if (public_key)
                EVP_PKEY_free(public_key);

            private_key = other.private_key;
            public_key = other.public_key;

            other.private_key = nullptr;
            other.public_key = nullptr;
        }

        return *this;
    }
};

inline KeyPair generate_keypair() {
    EVP_PKEY_CTX* ctx =
        EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);

    if (!ctx)
        throw std::runtime_error("failed to create key context");

    if (EVP_PKEY_keygen_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("failed to initialize key generation");
    }

    EVP_PKEY* key = nullptr;

    if (EVP_PKEY_keygen(ctx, &key) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("failed to generate keypair");
    }

    EVP_PKEY_CTX_free(ctx);

    return KeyPair(key);
}

inline std::vector<unsigned char> sign_message(
    EVP_PKEY* private_key,
    const std::string& message) {

    if (!private_key)
        throw std::runtime_error("missing private key");

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();

    if (!ctx)
        throw std::runtime_error("failed to create signing context");

    if (EVP_DigestSignInit(
            ctx, nullptr, nullptr, nullptr, private_key) != 1) {

        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("failed to initialize signing");
    }

    std::size_t signature_size = 0;

    if (EVP_DigestSign(
            ctx,
            nullptr,
            &signature_size,
            reinterpret_cast<const unsigned char*>(message.data()),
            message.size()) != 1) {

        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("failed to determine signature size");
    }

    std::vector<unsigned char> signature(signature_size);

    if (EVP_DigestSign(
            ctx,
            signature.data(),
            &signature_size,
            reinterpret_cast<const unsigned char*>(message.data()),
            message.size()) != 1) {

        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("failed to sign message");
    }

    signature.resize(signature_size);

    EVP_MD_CTX_free(ctx);

    return signature;
}


inline bool verify_signature(
    EVP_PKEY* public_key,
    const std::string& message,
    const std::vector<unsigned char>& signature) {

    if (!public_key || signature.empty())
        return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();

    if (!ctx)
        return false;

    if (EVP_DigestVerifyInit(
            ctx, nullptr, nullptr, nullptr, public_key) != 1) {

        EVP_MD_CTX_free(ctx);
        return false;
    }

    const int result = EVP_DigestVerify(
        ctx,
        signature.data(),
        signature.size(),
        reinterpret_cast<const unsigned char*>(message.data()),
        message.size());

    EVP_MD_CTX_free(ctx);

    return result == 1;
}

} // namespace caesar
