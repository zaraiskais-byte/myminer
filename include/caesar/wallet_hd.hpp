#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/evp.h>

#include <caesar/bip39.hpp>
#include <caesar/slip10.hpp>
#include <caesar/wallet.hpp>

namespace caesar {

// ============================================================
// Build an Ed25519 KeyPair from a 32-byte private seed.
//
// OpenSSL's EVP_PKEY_new_raw_private_key for ED25519 expects the
// 32-byte RFC 8032 private seed (a.k.a. "expanded" not needed).
// OpenSSL derives the public key deterministically from this seed.
// ============================================================
inline KeyPair keypair_from_ed25519_seed32(
    const std::array<uint8_t, 32>& private_seed) {

    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519,
        nullptr,
        private_seed.data(),
        private_seed.size());

    if (!pkey) {
        throw std::runtime_error(
            "wallet_hd: failed to create Ed25519 EVP_PKEY from seed");
    }

    return KeyPair(pkey);
}

// ============================================================
// Deterministic Wallet from a BIP39 mnemonic.
//
// Flow: mnemonic -> PBKDF2-HMAC-SHA512 -> 64B seed
//           -> SLIP-0010 m/44'/0'/0'/0'/0' -> 32B Ed25519 seed
//           -> EVP_PKEY -> Wallet
//
// Same mnemonic + passphrase always yields the same Wallet.
// ============================================================
inline Wallet wallet_from_mnemonic(
    const std::string& mnemonic,
    const std::string& passphrase = "") {

    if (!bip39::validate_mnemonic(mnemonic)) {
        throw std::runtime_error("wallet_hd: invalid BIP39 mnemonic");
    }

    const auto seed64 = bip39::mnemonic_to_seed(mnemonic, passphrase);

    const auto ext = slip10::derive_path(
        seed64.data(), seed64.size(), slip10::DEFAULT_PATH);

    return Wallet(keypair_from_ed25519_seed32(ext.key));
}

}  // namespace caesar
